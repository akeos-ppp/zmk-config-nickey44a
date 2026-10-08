/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_touch_swipe
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "touch-swipe requires a central-side devicetree instance"
#endif

#include <limits.h>
#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct touch_swipe_config {
    uint16_t hwheel_code;
    uint16_t wheel_code;
    uint16_t touch_code;
    int32_t threshold;
    int32_t axis_ratio;
    uint32_t left_keycode;
    uint32_t right_keycode;
    /* 0 disables vertical gestures (WHEEL is then only observed). */
    uint32_t up_keycode;
    uint32_t down_keycode;
    int32_t vertical_threshold;
};

struct touch_swipe_data {
    bool touching;
    bool swipe_fired;
    int32_t horizontal_accum;
    int32_t vertical_accum;
    /* Total vertical travel prevents opposite scroll samples cancelling out. */
    int32_t vertical_travel;
    /* Total horizontal travel guards vertical gestures the same way. */
    int32_t horizontal_travel;
};

static int64_t magnitude(int32_t value) {
    return value < 0 ? -(int64_t)value : (int64_t)value;
}

static int32_t bounded_add(int32_t accum, int64_t delta) {
    return CLAMP((int64_t)accum + delta, (int64_t)INT32_MIN, (int64_t)INT32_MAX);
}

static void reset(struct touch_swipe_data *data) {
    *data = (struct touch_swipe_data){0};
}

static void raise_key(uint32_t encoded, bool pressed, int64_t timestamp, int *err) {
    int ret = raise_zmk_keycode_state_changed_from_encoded(encoded, pressed, timestamp);
    if (ret < 0) {
        *err = ret;
    }
}

static uint32_t modifier_usage(int bit) {
    uint32_t id = HID_USAGE_KEY_KEYBOARD_LEFTCONTROL + bit;
    return ZMK_HID_USAGE(HID_USAGE_KEY, id);
}

static void tap(uint32_t keycode, const char *name) {
    /*
     * Latched by the caller before output: no failure may retrigger.
     * Modifiers of a combo such as LC(TAB) are sent as their own key events
     * around the base key (Ctrl down, Tab down, Tab up, Ctrl up), so the host
     * sees the modifier before the key, as with a physical shortcut.
     */
    int64_t timestamp = k_uptime_get();
    uint8_t mods = SELECT_MODS(keycode);
    uint32_t base = STRIP_MODS(keycode);
    int err = 0;
    for (int i = 0; i < 8; i++) {
        if (mods & BIT(i)) {
            raise_key(modifier_usage(i), true, timestamp, &err);
        }
    }
    raise_key(base, true, timestamp, &err);
    raise_key(base, false, timestamp, &err);
    for (int i = 7; i >= 0; i--) {
        if (mods & BIT(i)) {
            raise_key(modifier_usage(i), false, timestamp, &err);
        }
    }
    if (err < 0) {
        LOG_WRN("touch swipe key output failed: %d", err);
    }
    LOG_DBG("touch swipe %s fired", name);
}

static int observe_input(const struct device *dev, struct input_event *event,
                         uint32_t param1, uint32_t param2,
                         struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct touch_swipe_data *data = dev->data;
    const struct touch_swipe_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        if (!event->value) {
            reset(data);
            LOG_DBG("touch swipe release/reset");
        } else if (!data->touching) {
            reset(data);
            data->touching = true;
            LOG_DBG("touch swipe start");
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code == cfg->wheel_code) {
        if (data->touching && !data->swipe_fired) {
            data->vertical_accum = bounded_add(data->vertical_accum, event->value);
            data->vertical_travel = bounded_add(data->vertical_travel, magnitude(event->value));
            int64_t vertical = magnitude(data->vertical_accum);
            if (cfg->up_keycode && cfg->down_keycode && vertical >= cfg->vertical_threshold &&
                vertical >= (int64_t)data->horizontal_travel * cfg->axis_ratio) {
                data->swipe_fired = true;
                bool up = data->vertical_accum > 0;
                tap(up ? cfg->up_keycode : cfg->down_keycode, up ? "up" : "down");
            }
        }
        /* WHEEL passes on; a later processor decides whether it scrolls. */
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code != cfg->hwheel_code) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (data->touching && !data->swipe_fired) {
        data->horizontal_accum = bounded_add(data->horizontal_accum, event->value);
        data->horizontal_travel = bounded_add(data->horizontal_travel, magnitude(event->value));
        int64_t horizontal = magnitude(data->horizontal_accum);
        if (horizontal >= cfg->threshold &&
            horizontal >= (int64_t)data->vertical_travel * cfg->axis_ratio) {
            /* Latch before output: neither reversal nor output failure may retrigger. */
            data->swipe_fired = true;
            bool left = data->horizontal_accum < 0;
            tap(left ? cfg->left_keycode : cfg->right_keycode, left ? "left" : "right");
        }
    }
    /* Always suppress horizontal scrolling, including outside a tracked touch. */
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = observe_input};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, threshold) > 0 &&                                                 \
                 DT_INST_PROP(n, threshold) <= INT32_MAX, "threshold must be positive int32");   \
    BUILD_ASSERT(DT_INST_PROP(n, axis_ratio) > 0 &&                                                \
                 DT_INST_PROP(n, axis_ratio) <= INT32_MAX, "axis-ratio must be positive int32"); \
    BUILD_ASSERT(DT_INST_PROP(n, hwheel_code) != DT_INST_PROP(n, wheel_code),                      \
                 "horizontal and vertical codes must differ");                                   \
    BUILD_ASSERT(DT_INST_PROP_OR(n, vertical_threshold, DT_INST_PROP(n, threshold)) > 0,          \
                 "vertical-threshold must be positive");                                         \
    BUILD_ASSERT((DT_INST_PROP_OR(n, up_keycode, 0) == 0) ==                                       \
                     (DT_INST_PROP_OR(n, down_keycode, 0) == 0),                                 \
                 "set both up-keycode and down-keycode, or neither");                            \
    static struct touch_swipe_data data_##n;                                                       \
    static const struct touch_swipe_config config_##n = {                                          \
        .hwheel_code = DT_INST_PROP(n, hwheel_code),                                               \
        .wheel_code = DT_INST_PROP(n, wheel_code),                                                 \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .axis_ratio = DT_INST_PROP(n, axis_ratio),                                                 \
        .left_keycode = DT_INST_PROP(n, left_keycode),                                             \
        .right_keycode = DT_INST_PROP(n, right_keycode),                                           \
        .up_keycode = DT_INST_PROP_OR(n, up_keycode, 0),                                           \
        .down_keycode = DT_INST_PROP_OR(n, down_keycode, 0),                                       \
        .vertical_threshold =                                                                      \
            DT_INST_PROP_OR(n, vertical_threshold, DT_INST_PROP(n, threshold)),                    \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                       \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
