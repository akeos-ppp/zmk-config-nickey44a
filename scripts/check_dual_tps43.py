"""Check built Devicetrees/configs and model the left pad processor chains.

Run with the Python environment used by west. This checks configuration and
event semantics; it does not simulate BLE, TPS43 silicon, or host HID timing.
"""

import argparse
from pathlib import Path
import re
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zephyr-base", required=True, type=Path)
    parser.add_argument("--left-build", required=True, type=Path)
    parser.add_argument("--right-build", required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.zephyr_base / "scripts/dts/python-devicetree/src"))
    from devicetree import dtlib

    left = dtlib.DT(str(args.left_build / "zephyr/zephyr.dts"))
    right = dtlib.DT(str(args.right_build / "zephyr/zephyr.dts"))
    constants = dict(re.findall(
        r"^#define\s+(INPUT_(?:EV|REL|BTN)_\w+)\s+(0x[0-9a-fA-F]+|\d+)\b",
        (args.zephyr_base / "include/zephyr/dt-bindings/input/input-event-codes.h")
        .read_text(), re.MULTILINE))
    c = {name: int(value, 0) for name, value in constants.items()}

    def cells_of(prop):
        # Mixed phandle/number properties cannot use dtlib's to_nums().
        return list(struct.unpack(">" + "I" * (len(prop.value) // 4), prop.value))

    def enabled(node, name):
        return name in node.props

    def config(build):
        return dict(re.findall(r"^(CONFIG_\w+)=(.+)$",
                              (build / "zephyr/.config").read_text(), re.MULTILINE))

    lc, rc = config(args.left_build), config(args.right_build)
    for cfg, build in ((lc, args.left_build), (rc, args.right_build)):
        for name in ("GPIO", "I2C", "INPUT", "ZMK_POINTING", "INPUT_TPS43",
                     "ZMK_INPUT_SPLIT", "ZMK_SPLIT", "ZMK_SLEEP",
                     "ZMK_BATTERY_REPORTING"):
            assert cfg.get("CONFIG_" + name) == "y", name
        assert (build / "zephyr/zmk.uf2").is_file(), "Missing firmware"
    assert lc.get("CONFIG_ZMK_SPLIT_ROLE_CENTRAL") != "y"
    assert rc.get("CONFIG_ZMK_SPLIT_ROLE_CENTRAL") == "y"
    assert rc.get("CONFIG_ZMK_INPUT_LISTENER") == "y"
    assert lc.get("CONFIG_ZMK_INPUT_PROCESSOR_CODE_MAPPER") == "y"
    assert rc.get("CONFIG_ZMK_INPUT_PROCESSOR_SCALER") == "y"
    for name in ("ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY",
                 "ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING", "ZMK_STUDIO"):
        assert rc.get("CONFIG_" + name) == "y", name

    lt = left.label2node["left_tps43"]
    rt = right.label2node["tps43"]
    for dt, node in ((left, lt), (right, rt)):
        assert node.props["reg"].to_nums() == [0x74]
        # NRST: left P0.16, right P1.00.
        rst_port, rst_pin = ("gpio0", 16) if dt is left else ("gpio1", 0)
        assert cells_of(node.props["rst-gpios"]) == [
            dt.label2node[rst_port].props["phandle"].to_num(), rst_pin, 0]
        assert cells_of(node.props["rdy-gpios"]) == [
            dt.label2node["gpio1"].props["phandle"].to_num(), 10, 0]
        assert enabled(node, "enable-power-management")
        for state in ("default", "sleep"):
            label = ("nickey44a_left_tps43_i2c0_" if dt is left
                     else "nickey44a_tps43_i2c0_") + state
            psels = dt.label2node[label].nodes["group1"].props["psels"].to_nums()
            assert [v & 0x1ff for v in psels] == [9, 10]
        # No other direct GPIO consumer or pinctrl group owns P0.16.
        gpio0 = dt.label2node["gpio0"].props["phandle"].to_num()
        for candidate in dt.node_iter():
            for prop in candidate.props.values():
                if prop.name == "gpios" or prop.name.endswith("-gpios"):
                    values = cells_of(prop)
                    for index in range(0, len(values), 3):
                        if values[index:index + 2] == [gpio0, 16]:
                            assert dt is left and candidate is node and \
                                prop.name == "rst-gpios"
                if prop.name == "psels":
                    assert all(v & 0x1ff != 16 for v in prop.to_nums())
    # Left: one-finger motion is the volume circle, so no press-and-hold.
    assert not enabled(lt, "press-and-hold") and not enabled(lt, "single-tap")
    assert not enabled(lt, "switch-xy")
    assert "hold-time" not in lt.props
    # Right: press-and-hold drag and three-finger gestures.
    for name in ("single-tap", "press-and-hold", "two-finger-tap", "scroll", "switch-xy",
                 "invert-x", "three-finger-swipe"):
        assert enabled(rt, name), name
    assert enabled(lt, "two-finger-tap") and enabled(lt, "scroll")

    ls = left.label2node["left_tps43_split"]
    rs = right.label2node["left_tps43_split"]
    assert ls.props["reg"].to_num() == rs.props["reg"].to_num() == 0
    assert ls.props["device"].to_node() is lt
    assert "device" not in rs.props
    listener = right.label2node["left_tps43_listener"]
    assert listener.props["device"].to_node() is rs

    def chain_of(dt, node):
        chain, cells = [], cells_of(node.props["input-processors"])
        while cells:
            proc = dt.phandle2node[cells.pop(0)]
            count = proc.props["#input-processor-cells"].to_num()
            chain.append((proc, cells[:count]))
            del cells[:count]
        return chain

    left_chain = chain_of(left, ls)
    assert [n.name for n, _ in left_chain] == ["left_middle_click_mapper"]
    assert [params for _, params in left_chain] == [[]]
    central_chain = chain_of(right, listener)
    assert [n.name for n, _ in central_chain] == [
        "left_circle_volume", "left_touch_swipe", "left_wheel_blocker", "zip_xy_scaler"]
    assert [params for _, params in central_chain] == [[], [], [0, 1], [0, 1]]
    # Right pad: HWHEEL passes as horizontal scroll (no touch-swipe in its chain).
    right_chain = [n.name for n, _ in chain_of(right, right.label2node["tps43_listener"])]
    assert right_chain == ["tps43_orientation", "tps43_three_finger_swipe",
                           "tps43_pinch_zoom", "tps43_touch_inertia"], right_chain
    assert enabled(rt, "soft-pinch") and not enabled(rt, "zoom")
    pinch = right.label2node["tps43_pinch_zoom"]
    assert pinch.props["zoom-code"].to_num() == c["INPUT_REL_MISC"]
    # Left pad scroll counts stay raw so slow swipes are not truncated to 0.
    assert lt.props["scroll-sensitivity"].to_num() == 100
    circle = central_chain[0][0]
    assert circle.props["x-code"].to_num() == c["INPUT_REL_X"]
    assert circle.props["y-code"].to_num() == c["INPUT_REL_Y"]
    chain = left_chain + central_chain

    def process(kind, code, value, sync=True):
        # Model the pinned standard processors using the actual built properties.
        for node, params in chain:
            compatible = node.props["compatible"].to_string()
            if compatible == "zmk,input-processor-circle-keys":
                continue  # Observes X/Y and touch only; never changes or stops events.
            if compatible == "zmk,input-processor-touch-swipe":
                if kind == rel and code == node.props["hwheel-code"].to_num():
                    return None  # Always consumed; may tap Back/Forward instead.
                continue
            if kind != node.props["type"].to_num():
                continue
            if compatible == "zmk,input-processor-scaler":
                if code in node.props["codes"].to_nums():
                    value = int(value * params[0] / params[1])
            elif compatible == "zmk,input-processor-code-mapper":
                mapping = node.props["map"].to_nums()
                for original, replacement in zip(mapping[::2], mapping[1::2]):
                    if code == original:
                        code = replacement
                        break
            else:
                raise AssertionError(compatible)
        return kind, code, value, sync

    rel, key = c["INPUT_EV_REL"], c["INPUT_EV_KEY"]
    for value in (-32768, -100, -1, 0, 1, 100, 32767):
        for code in ("INPUT_REL_X", "INPUT_REL_Y", "INPUT_REL_WHEEL"):
            assert process(rel, c[code], value) == (rel, c[code], 0, True)
        assert process(rel, c["INPUT_REL_HWHEEL"], value) is None
    for value in (0, 1):
        assert process(key, c["INPUT_BTN_1"], value) == (key, c["INPUT_BTN_2"], value, True)
        assert process(key, c["INPUT_BTN_0"], value) == (key, c["INPUT_BTN_0"], value, True)
    assert process(rel, c["INPUT_REL_X"], 25, False)[3] is False
    # X/Y cross BLE unchanged so the central circle processor sees real motion.
    for code in ("INPUT_REL_X", "INPUT_REL_Y"):
        value = 25
        for node, params in left_chain:
            if node.props["compatible"].to_string() == "zmk,input-processor-scaler" and \
                    c[code] in node.props["codes"].to_nums():
                value = int(value * params[0] / params[1])
        assert value == 25, code
    print("PASS: built pins, gestures, split routing, roles, battery/sleep/Studio config")
    print("PASS: processor order, signed motion blocking, HWHEEL to Back/Forward, WHEEL zeroed after tab switch, sync, right pinch zoom")
    print("PASS: left X/Y reach the central volume circle and never move the cursor")
    print("Hardware/BLE/Deep Sleep acceptance tests remain manual.")


if __name__ == "__main__":
    main()
