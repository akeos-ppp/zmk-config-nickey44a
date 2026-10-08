/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_pinch_keys
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct pinch_config {
    uint16_t zoom_code;
    uint16_t touch_code;
    int32_t step;
    uint32_t zoom_in_keycode;
    uint32_t zoom_out_keycode;
};

struct pinch_data {
    int32_t accum;
};

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

/* Modifiers go down before and up after the base key, like a physical shortcut. */
static void tap(uint32_t keycode) {
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
        LOG_WRN("pinch key output failed: %d", err);
    }
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct pinch_data *data = dev->data;
    const struct pinch_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        /* Each pinch starts from zero; leftovers never carry into the next touch. */
        data->accum = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL || event->code != cfg->zoom_code) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->accum = CLAMP((int64_t)data->accum + event->value, -INT16_MAX * 4, INT16_MAX * 4);
    while (data->accum >= cfg->step) {
        data->accum -= cfg->step;
        LOG_DBG("pinch zoom in");
        tap(cfg->zoom_in_keycode);
    }
    while (data->accum <= -cfg->step) {
        data->accum += cfg->step;
        LOG_DBG("pinch zoom out");
        tap(cfg->zoom_out_keycode);
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
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .step = DT_INST_PROP(n, step),                                                             \
        .zoom_in_keycode = DT_INST_PROP(n, zoom_in_keycode),                                       \
        .zoom_out_keycode = DT_INST_PROP(n, zoom_out_keycode),                                     \
    };                                                                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                       \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
