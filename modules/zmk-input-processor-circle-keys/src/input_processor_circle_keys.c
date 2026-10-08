/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_circle_keys
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "circle-keys requires a central-side devicetree instance"
#endif

#include <math.h>
#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct circle_config {
    uint16_t x_code;
    uint16_t y_code;
    uint16_t touch_code;
    int32_t segment_distance;
    float step_degrees;
    float max_turn_degrees;
    int32_t idle_reset_ms;
    uint32_t cw_keycode;
    uint32_t ccw_keycode;
};

struct circle_data {
    /* Motion of the current driver report, completed by the sync event. */
    int32_t pending_x;
    int32_t pending_y;
    /* Motion accumulated into the segment being built. */
    int32_t seg_x;
    int32_t seg_y;
    bool have_heading;
    float heading;
    float turn;
    int64_t last_motion_ms;
};

static void reset_motion(struct circle_data *data) { *data = (struct circle_data){0}; }

static void tap(uint32_t keycode) {
    int64_t timestamp = k_uptime_get();
    int press = raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
    int release = raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
    if (press < 0 || release < 0) {
        LOG_WRN("circle key output failed: %d/%d", press, release);
    }
}

static void add_sample(const struct circle_config *cfg, struct circle_data *data, int32_t dx,
                       int32_t dy) {
    int64_t now = k_uptime_get();
    if (data->last_motion_ms && now - data->last_motion_ms > cfg->idle_reset_ms) {
        reset_motion(data);
    }
    data->last_motion_ms = now;
    data->seg_x = CLAMP(data->seg_x + dx, -32768, 32767);
    data->seg_y = CLAMP(data->seg_y + dy, -32768, 32767);

    int64_t len2 = (int64_t)data->seg_x * data->seg_x + (int64_t)data->seg_y * data->seg_y;
    if (len2 < (int64_t)cfg->segment_distance * cfg->segment_distance) {
        return;
    }

    /* Screen coordinates: X right, Y down, so a positive angle change is clockwise. */
    float heading = atan2f((float)data->seg_y, (float)data->seg_x) * (180.0f / 3.14159265f);
    data->seg_x = 0;
    data->seg_y = 0;
    if (!data->have_heading) {
        data->have_heading = true;
        data->heading = heading;
        return;
    }

    float delta = heading - data->heading;
    if (delta > 180.0f) {
        delta -= 360.0f;
    } else if (delta <= -180.0f) {
        delta += 360.0f;
    }
    data->heading = heading;
    if (fabsf(delta) > cfg->max_turn_degrees) {
        /* A sharp corner or reversal is not circular motion. */
        data->turn = 0.0f;
        return;
    }

    data->turn += delta;
    while (data->turn >= cfg->step_degrees) {
        data->turn -= cfg->step_degrees;
        tap(cfg->cw_keycode);
    }
    while (data->turn <= -cfg->step_degrees) {
        data->turn += cfg->step_degrees;
        tap(cfg->ccw_keycode);
    }
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct circle_data *data = dev->data;
    const struct circle_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        reset_motion(data);
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code == cfg->x_code) {
        data->pending_x += event->value;
    } else if (event->code == cfg->y_code) {
        data->pending_y += event->value;
    } else {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->sync) {
        int32_t dx = data->pending_x;
        int32_t dy = data->pending_y;
        data->pending_x = 0;
        data->pending_y = 0;
        if (dx || dy) {
            add_sample(cfg, data, dx, dy);
        }
    }
    /* Observe only; a later processor decides whether the cursor moves. */
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, segment_distance) > 0, "segment-distance must be positive");     \
    BUILD_ASSERT(DT_INST_PROP(n, step_degrees) > 0, "step-degrees must be positive");             \
    BUILD_ASSERT(DT_INST_PROP(n, max_turn_degrees) > 0 &&                                          \
                     DT_INST_PROP(n, max_turn_degrees) < 180,                                      \
                 "max-turn-degrees must be in 1..179");                                            \
    static struct circle_data data_##n;                                                            \
    static const struct circle_config config_##n = {                                               \
        .x_code = DT_INST_PROP(n, x_code),                                                         \
        .y_code = DT_INST_PROP(n, y_code),                                                         \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .segment_distance = DT_INST_PROP(n, segment_distance),                                     \
        .step_degrees = (float)DT_INST_PROP(n, step_degrees),                                      \
        .max_turn_degrees = (float)DT_INST_PROP(n, max_turn_degrees),                              \
        .idle_reset_ms = DT_INST_PROP(n, idle_reset_ms),                                           \
        .cw_keycode = DT_INST_PROP(n, clockwise_keycode),                                          \
        .ccw_keycode = DT_INST_PROP(n, counterclockwise_keycode),                                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)
#endif
