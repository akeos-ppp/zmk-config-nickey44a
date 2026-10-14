/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_pinch_keys
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "pinch-keys requires a central-side devicetree instance"
#endif

#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct pinch_config {
    uint16_t zoom_code;
    int32_t step;
    uint32_t modifier_keycode; /* 0 = none */
    uint8_t hold_buttons;       /* mouse button flags (MB1..MB5), 0 = none */
    bool invert_wheel;
};

struct pinch_data {
    int32_t accum;
    bool held;
};

static void send_mouse_report(void) {
#ifdef ZMK_ENDPOINT_NONE_COUNT
    zmk_endpoint_send_mouse_report();
#else
    zmk_endpoints_send_mouse_report();
#endif
}

/* Press (or release) the modifier key and mouse buttons held for the whole pinch. */
static void set_held(struct pinch_data *data, const struct pinch_config *cfg, bool pressed) {
    if (data->held == pressed) {
        return;
    }
    data->held = pressed;
    if (cfg->modifier_keycode != 0) {
        int ret = raise_zmk_keycode_state_changed_from_encoded(cfg->modifier_keycode, pressed,
                                                               k_uptime_get());
        if (ret < 0) {
            LOG_WRN("pinch modifier %s failed: %d", pressed ? "press" : "release", ret);
        }
    }
    if (cfg->hold_buttons != 0) {
        if (pressed) {
            zmk_hid_mouse_buttons_press(cfg->hold_buttons);
        } else {
            zmk_hid_mouse_buttons_release(cfg->hold_buttons);
        }
        zmk_hid_mouse_movement_set(0, 0);
        zmk_hid_mouse_scroll_set(0, 0);
        send_mouse_report();
    }
}

/*
 * Sent straight to the host instead of continuing as REL_WHEEL, so the
 * touch-inertia processor never sees the zoom wheel and cannot keep
 * scrolling after the fingers lift (when the modifier is already released).
 */
static void send_wheel(int8_t wheel) {
    zmk_hid_mouse_movement_set(0, 0);
    zmk_hid_mouse_scroll_set(0, wheel);
    send_mouse_report();
    zmk_hid_mouse_scroll_set(0, 0);
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct pinch_data *data = dev->data;
    const struct pinch_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL || event->code != cfg->zoom_code) {
        /*
         * The driver sends only REL_MISC while a pinch owns the contact, so any
         * other event (touch up, a finger lifting into cursor motion, a click)
         * ends the pinch: release what was held before that event goes on.
         */
        set_held(data, cfg, false);
        data->accum = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->accum = CLAMP((int64_t)data->accum + event->value, -INT16_MAX * 4, INT16_MAX * 4);
    int32_t ticks = CLAMP(data->accum / cfg->step, -127, 127);
    if (ticks != 0) {
        data->accum -= ticks * cfg->step;
        set_held(data, cfg, true);
        LOG_DBG("pinch wheel %d", ticks);
        send_wheel(cfg->invert_wheel ? -ticks : ticks);
    }
    /* The zoom amount is consumed here; the HID listener has no use for it. */
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, step) > 0, "step must be positive");                             \
    static struct pinch_data data_##n;                                                             \
    static const struct pinch_config config_##n = {                                                \
        .zoom_code = DT_INST_PROP(n, zoom_code),                                                   \
        .step = DT_INST_PROP(n, step),                                                             \
        .modifier_keycode = DT_INST_PROP_OR(n, modifier_keycode, 0),                               \
        .hold_buttons = DT_INST_PROP_OR(n, hold_buttons, 0),                                       \
        .invert_wheel = DT_INST_PROP(n, invert_wheel),                                             \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                       \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
