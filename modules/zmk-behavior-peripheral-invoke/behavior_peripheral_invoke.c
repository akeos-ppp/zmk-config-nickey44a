/*
 * Nickey36A: invoke a source-local behavior (such as &bootloader or
 * &sys_reset) on a chosen split peripheral, independent of where the
 * triggering key was pressed. Combos always run with the central as their
 * event source, so a plain &bootloader in a combo can only reset the central.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_peripheral_invoke

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_peripheral_invoke_config {
    struct zmk_behavior_binding binding;
    uint8_t peripheral;
};

static int peripheral_invoke(struct zmk_behavior_binding *binding,
                             struct zmk_behavior_binding_event event, bool pressed) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_peripheral_invoke_config *cfg = dev->config;

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    event.source = cfg->peripheral;
#endif
    LOG_DBG("peripheral invoke: %s on peripheral %d (%s)", cfg->binding.behavior_dev,
            cfg->peripheral, pressed ? "pressed" : "released");
    zmk_behavior_invoke_binding(&cfg->binding, event, pressed);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return peripheral_invoke(binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return peripheral_invoke(binding, event, false);
}

static const struct behavior_driver_api behavior_peripheral_invoke_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define PERIPHERAL_INVOKE_INST(n)                                                                  \
    static const struct behavior_peripheral_invoke_config behavior_peripheral_invoke_config_##n = { \
        .binding = ZMK_KEYMAP_EXTRACT_BINDING(0, DT_DRV_INST(n)),                                  \
        .peripheral = DT_INST_PROP(n, peripheral),                                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_peripheral_invoke_config_##n,           \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                            &behavior_peripheral_invoke_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PERIPHERAL_INVOKE_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
