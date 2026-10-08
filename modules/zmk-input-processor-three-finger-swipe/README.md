# Three-finger swipe

Local MIT-licensed processor for the right TPS43 listener, placed before
touch-swipe. It consumes the driver's per-sample three-finger direction key
events (`left-code`/`right-code`/`up-code`/`down-code`, mapped in the overlay
from controller to physical directions) and taps one configured ZMK keycode
per complete touch. The vendored driver reports only the dominant axis per
sample and `three-finger-swipe-throttle-ms = <0>` gives one event per sample.

TOUCH down resets on the idle-to-touch transition; TOUCH up resets fully.
A horizontal fire needs abs(right - left) >= threshold and
>= (up + down) * axis-ratio; vertical is symmetric. After firing nothing is
counted until all fingers release. The four direction codes always return
ZMK_INPUT_PROC_STOP; every other event passes unchanged.

The binding is mirrored in the shield's dts/bindings directory (automatic DTS
root); keep both YAML files in sync.

## Tuning

Nickey44A starts with threshold=3 samples and axis-ratio=2, with macOS
Ctrl+Up for up and down (Mission Control toggle) and Ctrl+Right/Ctrl+Left for
physical left/right swipes. If a direction is reversed on hardware, swap the
corresponding `*-code` pair in nickey44a_r.overlay. Raise threshold for
accidental fires, lower it for shorter swipes. The two-finger phase before the
third finger lands can still scroll or trigger touch-swipe.

## Three-finger tap

The vendored driver reports key code 0x14e (`TPS43_BTN_THREE_FINGERS`) as 1
while exactly three fingers are down. At full release, a touch that reached
three fingers, fired no swipe, lasted at most `tap-max-ms` and produced at
most `tap-max-samples` direction samples taps `tap-keycode` (0 disables).
INPUT_BTN_0/1 are dropped during such a touch and for `click-suppress-ms`
after it, so hardware single/two-finger taps do not click alongside.
Nickey44A uses LG(L), the shortcut of the keymap's LAUNCH macro; the macOS
Launchpad shortcut must be assigned to Cmd+L for this to open Launchpad.
