# Nickey44A-PAD: 左右TPS43

右central／左peripheral。左右ともSDA=P0.09、SCL=P0.10、RDY=P1.10、
NRST=左P0.16／右P1.00、I2Cアドレス=0x74。各MCUに独立したI2Cバスがあるため、
アドレスは同じでよい。westのrevision、ZMK本体、Azoteqドライバーは変更しない。

## イベント経路

```text
左TPS43 → 左input-splitの標準processors → BLE split (reg=0)
        → 右input-split proxy (reg=0) → 左専用input-listener
        → 音量サークル検出 → 二本指左右で戻る/進む・上下でタブ切替 → WHEEL・X/Y 0化 → 共通HID
右TPS43 → 右input-listener → orientation → 三本指ジェスチャー → inertia/Snipe → 共通HID
```

左のprocessor順序は次の通り。変換はすべてBLE転送前に行う。

| 順序 | Processor | 結果 |
|---|---|---|
| 1 | `left_middle_click_mapper` | BTN_1をBTN_2へ変換、DOWN/UPは保持 |

REL_X/Y、WHEEL、HWHEELは変換せずに転送する。右の左専用listenerは
次の順で処理する。

| 順序 | Processor | 結果 |
|---|---|---|
| 1 | `left_circle_volume` | 一本指の回転で音量（時計回り=アップ、45度ごと） |
| 2 | `left_touch_swipe` | 二本指左右スワイプ1回で戻る/進む、上下スワイプ1回でタブ切替（Ctrl+Tab / Ctrl+Shift+Tab）、HWHEELは常に破棄 |
| 3 | `left_wheel_blocker 0 1` | タブ切替後のWHEELを0化（スクロールはしない） |
| 4 | `zip_xy_scaler 0 1` | X/Yを0化、左パッドでカーソルは動かない |

右パッドの二本指左右スワイプはtouch-swipeを通さず、そのまま横スクロールになる。


BTN_0は変換しない。左はsingle-tapとpress-and-holdを有効にせず、
two-finger-tapとscrollのみ有効。一本指の動きは音量サークル専用。
標準scalerはイベントを破棄せず値を0にするため、X/Yと元HWHEELの
BLE通知件数は減らない。sync情報もそのまま転送される。

右の実測補正はXY交換と両軸反転を含む。左は上下操作をドライバーの
WHEELとして扱うため、右のコントローラー設定からswitch-xyを省き、
invert-xは維持する。左に追加のorientation processorはない。
同じ物理向きの実装を前提とした設定であり、左の取り付け向きと実機の
上下・左右判定は下記手順で確認する。

右TPS43はpress-and-holdとdrag-lock（離しても押下維持、次のタップで解除）、
三本指スワイプ／タップを有効にしている。既存のタップ、スクロール、
swipe、inertia、orientation、Layer 1 Snipe、感度、電源管理、RST/RDYは維持。
固定ZMKのHIDはボタン押下をカウントするため、右の移動イベントは左の
BTN_0保持を解除しない。左保持中の右タップも、タップ終了後に左の押下が残る。

## 自動確認

左右のFWを通常のbuild.yaml設定でビルドしてから実行する。
Zephyr付属のpython-devicetreeを使い、生成されたDevicetreeと.configを検証する。

```powershell
python scripts/check_dual_tps43.py `
  --zephyr-base <Zephyrのパス> `
  --left-build <左buildディレクトリ> `
  --right-build <右buildディレクトリ>
```

確認対象はピン、RSTのActive HIGH、GPIO/pinctrl競合、gesture設定、
split reg/device/listener経路、central/peripheral、バッテリー・sleep・Studioの
有効設定、processor順序。生成されたprocessorの設定値からイベント変換を
モデル化し、負値を含むX/Y抑止、HWHEEL抑止、WHEEL変換、BTN_1変換、
BTN_0とsync保持、左保持中の右タップを確認する。

この検証は実際のC処理・BLE通信・TPS43のgesture判定・HIDタイミングを
実行するものではない。実機動作の合格を意味しない。

2026-10-06の確認結果：左右TPS43ドライバーとinput-splitを含むFWビルド成功。
左の標準scaler/code mapperもコンパイル成功。上記自動確認はすべてPASS。
Devicetree/bindingエラーなし。非推奨設定、peripheralのZMK_USB依存、
右ZMKのcombo/event_manager配列境界についてビルド警告あり。

## 実機確認（未実施）

左右それぞれ新しいUF2を書き込み、通常のBLE split接続後に確認する。
既存のペアリング情報はまず維持する。settings_resetは通常の更新では不要。

| 対象 | 操作 | 合格条件 |
|---|---|---|
| 右 | 1本指移動／タップ、2本指タップ | 移動／左クリック／右クリック |
| 右 | 2本指上下／左右 | 縦スクロール／戻る・進む |
| 右 | スクロール後に離す、Layer 1移動 | 従来通りの慣性／Snipe |
| 右 | 1本指長押しして移動 | 左ボタン保持によるドラッグを開始しない |
| 左 | 1本指移動／軽いタップ | カーソル移動・クリックなし |
| 左 | 1本指長押し、保持、離す | BTN_0 DOWN、保持継続、BTN_0 UP |
| 左 | 2本指上下／左右 | 水平スクロール／スクロールなし |
| 左 | 2本指タップ | 中クリックのみ、右クリックなし |
| Split | 左右のキーとバッテリー | 入力と両側バッテリー取得が従来通り |
| Split | 左右同時操作を繰り返す | クラッシュ・ボタン固着・入力欠落なし |
| Sleep | 両側Deep Sleep後にキーで復帰 | split再接続、両TPS43とキーが操作可能 |

主要ケース：右で対象へ移動 → 左を80ms以上長押し → 左を保持したまま
右でドラッグ → 左指を離してドロップ。左指を少し動かしても座標に影響せず、
右を止めても左の保持が継続すること。保持中の右タップ後も、左指を離すまで
保持が残ることを追加確認する。

必要ならホストのHIDイベント表示でBTN_0 DOWN/UP、BTN_2 DOWN/UP、
水平wheel、左由来の非ゼロX/Yがないことを確認する。左操作のみでは
BTN_1がホストへ出ないことも確認する。

Deep Sleepでは起動時の既存RST処理を利用する。新たなsleep hookはない。
保持中のBLE切断・peripheral再起動でUPが届かなかった場合の状態復旧は、
この設定では追加していない。接続断を伴うケースは通常ドラッグと分けて確認する。
