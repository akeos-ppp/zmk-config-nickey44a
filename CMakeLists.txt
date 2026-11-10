# Bundled until the standalone touch-inertia module is published and SHA-pinned.
add_subdirectory(modules/zmk-input-processor-touch-inertia)
add_subdirectory(modules/zmk-input-processor-touch-swipe)
# Vendored TPS43 driver (replaces the geeksville west project; see its README).
add_subdirectory(modules/zmk-driver-azoteq-tps43)
add_subdirectory(modules/zmk-input-processor-three-finger-swipe)
add_subdirectory(modules/zmk-input-processor-circle-keys)
add_subdirectory(modules/zmk-input-processor-pinch-keys)
# Nickey36A: run &bootloader on the left peripheral from a combo.
add_subdirectory(modules/zmk-behavior-peripheral-invoke)
# nemu-link dongle: central-side &touch_feedback that forwards to both halves.
add_subdirectory(modules/zmk-behavior-touch-feedback-relay)
# Battery voltage divider with the Ni-MH mv-to-pct-thresholds curve.
add_subdirectory(modules/zmk-battery-voltage-divider-thresholds)
