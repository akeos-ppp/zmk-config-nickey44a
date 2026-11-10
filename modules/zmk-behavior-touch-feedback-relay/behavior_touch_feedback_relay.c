/*
 * Nickey36A + nemu-link: &touch_feedback on the dongle.
 *
 * The IQS9151 driver's touch-feedback behavior is GLOBAL, so the split
 * central invokes it on every peripheral and then locally. The nemu-link
 * dongle has no IQS9151 / DRV2605, so the driver's own implementation is not
 * built there and the binding could not be resolved. This central-side
 * stand-in has the same compatible and locality: the central forwards the
 * binding (by this device's name, "touch_feedback") to both halves, where the
 * real driver behavior runs; the local call is a no-op.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_touch_feedback

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int pressed(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    return binding->param1 > 1 ? -EINVAL : ZMK_BEHAVIOR_OPAQUE;
}

static int released(struct zmk_behavior_binding *binding,
                    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api api = {
    .binding_pressed = pressed,
    .binding_released = released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

#define DEFINE(n)                                                                                  \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);
DT_INST_FOREACH_STATUS_OKAY(DEFINE)

#endif
