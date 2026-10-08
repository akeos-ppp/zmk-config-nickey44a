# Azoteq TPS43 driver (vendored)

Vendored from [geeksville/zmk_driver_azoteq](https://github.com/geeksville/zmk_driver_azoteq)
at commit `66d510249d139569a4060c35bd78c6a2159f83ed` (MIT, see `LICENSE`).
It replaces the west project of the same driver; do not load both, because
they define the same `azoteq,tps43` compatible and `CONFIG_INPUT_TPS43`.

Layout changes only: sources moved to `src/`, built with `target_sources`
from the repository root `CMakeLists.txt`, Kconfig loaded through the root
`Kconfig`, and the devicetree bindings mirrored in
`boards/shields/nickey44a/dts/bindings/input/` (an automatic DTS root).

Behaviour change, `tps43_handle_swipe()` only: while three fingers are down,
each sample reports only its dominant axis as
`INPUT_BTN_WEST/EAST/NORTH/SOUTH`; an exact tie reports nothing. Upstream
reports every non-zero axis, so a slightly diagonal swipe produced horizontal
and vertical events together. One-finger hardware swipes are unchanged.
Directions are in controller coordinates (after `switch-xy`/`invert-x`); the
listener transform affects only relative events, not these key codes.

Second change: the work handler reports key code 0x14e
(`TPS43_BTN_THREE_FINGERS`, Linux BTN_TOOL_TRIPLETAP) as 1 when exactly
three fingers are down and 0 when that ends, for three-finger tap detection.

Third change: once three fingers are down, the touch stays a three-finger
session until every finger lifts. Movement with fewer fingers in that session
is still reported as three-finger direction events, not scroll or cursor
motion, because an outer finger leaves the small pad early in horizontal swipes.

Fourth change: optional `drag-lock` property. After a press-and-hold drag the
left button stays pressed when the finger lifts. The next single tap releases
it without clicking; another press-and-hold continues the same drag.
