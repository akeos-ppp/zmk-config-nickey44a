# Bundled until the standalone touch-inertia module is published and SHA-pinned.
add_subdirectory(modules/zmk-input-processor-touch-inertia)
add_subdirectory(modules/zmk-input-processor-touch-swipe)
# Vendored TPS43 driver (replaces the geeksville west project; see its README).
add_subdirectory(modules/zmk-driver-azoteq-tps43)
add_subdirectory(modules/zmk-input-processor-three-finger-swipe)
add_subdirectory(modules/zmk-input-processor-circle-keys)
