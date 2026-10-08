/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_three_finger_swipe
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "three-finger-swipe requires a central-side devicetree instance"
#endif

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

enum tfs_dir { TFS_LEFT, TFS_RIGHT, TFS_UP, TFS_DOWN, TFS_DIRS };

struct tfs_config {
    uint16_t touch_code;
    uint16_t codes[TFS_DIRS];
    uint32_t keycodes[TFS_DIRS];
    int32_t threshold;
    int32_t axis_ratio;
    /* Three-finger tap; disabled when tap_keycode is 0. */
    uint16_t three_finger_code;
    uint16_t click_codes[2];
    uint32_t tap_keycode;
    int32_t tap_max_ms;
    int32_t tap_max_samples;
    int32_t click_suppress_ms;
};

struct tfs_data {
    bool touching;
    bool fired;
    /* Sample counts; one count per driver report, saturated well below overflow. */
    uint16_t count[TFS_DIRS];
    bool three_seen;
    int64_t touch_start_ms;
    /* Survives reset: hides tap/click events trailing a three-finger touch. */
    int64_t click_suppress_until_ms;
};

static void tap(uint32_t keycode);

static void reset(struct tfs_data *data) {
    int64_t suppress = data->click_suppress_until_ms;
    *data = (struct tfs_data){0};
    data->click_suppress_until_ms = suppress;
}

static int32_t total_samples(const struct tfs_data *data) {
    int32_t total = 0;
    for (int i = 0; i < TFS_DIRS; i++) {
        total += data->count[i];
    }
    return total;
}

static void finish_touch(const struct tfs_config *cfg, struct tfs_data *data) {
    if (!data->touching || !data->three_seen) {
        return;
    }
    int64_t now = k_uptime_get();
    data->click_suppress_until_ms = now + cfg->click_suppress_ms;
    if (cfg->tap_keycode && !data->fired && now - data->touch_start_ms <= cfg->tap_max_ms &&
        total_samples(data) <= cfg->tap_max_samples) {
        LOG_DBG("three-finger tap fired");
        tap(cfg->tap_keycode);
    }
}

static int find_dir(const struct tfs_config *cfg, uint16_t code) {
    for (int i = 0; i < TFS_DIRS; i++) {
        if (cfg->codes[i] == code) {
            return i;
        }
    }
    return -1;
}

static void tap(uint32_t keycode) {
    int64_t timestamp = k_uptime_get();
    int press = raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
    int release = raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
    if (press < 0 || release < 0) {
        LOG_WRN("three-finger swipe key output failed: %d/%d", press, release);
    }
}

/* Returns the direction to fire on this axis, or -1. */
static int axis_winner(const struct tfs_config *cfg, const struct tfs_data *data, int neg,
                       int pos, int other_a, int other_b) {
    int32_t net = (int32_t)data->count[pos] - (int32_t)data->count[neg];
    int32_t magnitude = net < 0 ? -net : net;
    int32_t other = (int32_t)data->count[other_a] + (int32_t)data->count[other_b];
    if (magnitude >= cfg->threshold && magnitude >= other * cfg->axis_ratio) {
        return net < 0 ? neg : pos;
    }
    return -1;
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct tfs_data *data = dev->data;
    const struct tfs_config *cfg = dev->config;

    if (event->type != INPUT_EV_KEY) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code == cfg->touch_code) {
        if (!event->value) {
            finish_touch(cfg, data);
            reset(data);
        } else if (!data->touching) {
            reset(data);
            data->touching = true;
            data->touch_start_ms = k_uptime_get();
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code == cfg->three_finger_code) {
        if (event->value && data->touching) {
            data->three_seen = true;
        }
        return ZMK_INPUT_PROC_STOP;
    }
    if (event->code == cfg->click_codes[0] || event->code == cfg->click_codes[1]) {
        /* Hardware single/two-finger taps can accompany a three-finger touch. */
        if ((data->touching && data->three_seen) ||
            k_uptime_get() < data->click_suppress_until_ms) {
            return ZMK_INPUT_PROC_STOP;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int dir = find_dir(cfg, event->code);
    if (dir < 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    /* Direction codes are always consumed; count presses only. */
    if (!event->value || !data->touching || data->fired) {
        return ZMK_INPUT_PROC_STOP;
    }
    if (data->count[dir] < UINT16_MAX / 4) {
        data->count[dir]++;
    }

    int winner = axis_winner(cfg, data, TFS_LEFT, TFS_RIGHT, TFS_UP, TFS_DOWN);
    if (winner < 0) {
        winner = axis_winner(cfg, data, TFS_UP, TFS_DOWN, TFS_LEFT, TFS_RIGHT);
    }
    if (winner >= 0) {
        /* Latch before output: one action per complete touch. */
        data->fired = true;
        LOG_DBG("three-finger swipe fired: %d", winner);
        tap(cfg->keycodes[winner]);
    }
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, threshold) > 0, "threshold must be positive");                   \
    BUILD_ASSERT(DT_INST_PROP(n, axis_ratio) > 0, "axis-ratio must be positive");                 \
    static struct tfs_data data_##n;                                                               \
    static const struct tfs_config config_##n = {                                                  \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .codes = {DT_INST_PROP(n, left_code), DT_INST_PROP(n, right_code),                         \
                  DT_INST_PROP(n, up_code), DT_INST_PROP(n, down_code)},                           \
        .keycodes = {DT_INST_PROP(n, left_keycode), DT_INST_PROP(n, right_keycode),                \
                     DT_INST_PROP(n, up_keycode), DT_INST_PROP(n, down_keycode)},                  \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .axis_ratio = DT_INST_PROP(n, axis_ratio),                                                 \
        .three_finger_code = DT_INST_PROP(n, three_finger_code),                                   \
        .click_codes = {INPUT_BTN_0, INPUT_BTN_1},                                                 \
        .tap_keycode = DT_INST_PROP(n, tap_keycode),                                               \
        .tap_max_ms = DT_INST_PROP(n, tap_max_ms),                                                 \
        .tap_max_samples = DT_INST_PROP(n, tap_max_samples),                                       \
        .click_suppress_ms = DT_INST_PROP(n, click_suppress_ms),                                   \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
