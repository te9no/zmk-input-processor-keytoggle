#define DT_DRV_COMPAT zmk_input_processor_keytoggle

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>


LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct keytoggle_config {
    uint32_t keycodes;
    uint32_t tap_ms;
    uint32_t wait_ms;
};

struct keytoggle_state {
    bool is_pressed;
    const struct keytoggle_config *config; // Add pointer to config
    struct k_work_delayable key_release_work;
};

static void key_release_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    struct keytoggle_state *state = CONTAINER_OF(d_work, struct keytoggle_state, key_release_work);

    if (state->is_pressed) {
        LOG_DBG("Releasing keycode: 0x%02X", state->config->keycodes);
        // ZMK_BEHAVIOR_QUEUE_RELEASE(0, state->config->keycodes);
        state->is_pressed = false;
    }
}

static int keytoggle_handle_event(const struct device *dev, struct input_event *event, uint32_t param1, uint32_t param2, struct zmk_input_processor_state *processor_state) {
    const struct keytoggle_config *config = dev->config;
    struct keytoggle_state *state = dev->data;

    if (event->type == INPUT_EV_REL) {
        
        struct zmk_behavior_binding_event ev = {
            .position = 12345 + 1,
            .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
        };

        // Relative movement detected, press the key if not already pressed
        if (!state->is_pressed) {
            LOG_DBG("Pressing keycode: 0x%02X", config->keycodes);
            zmk_behavior_queue_add(&ev, config->keycodes, true, config->tap_ms);
            zmk_behavior_queue_add(&ev, config->keycodes, false, config->wait_ms);
            state->is_pressed = true;
        }
        // Reset the key release timer
        k_work_reschedule(&state->key_release_work, K_MSEC(100)); // Adjust delay as needed
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static int keytoggle_init(const struct device *dev) {
    struct keytoggle_state *state = dev->data;
    k_work_init_delayable(&state->key_release_work, key_release_callback);
    return 0;
}

static const struct zmk_input_processor_driver_api keytoggle_driver_api = {
    .handle_event = keytoggle_handle_event,
};

#define KEYTOGGLE_INST(n)                                                                      \
    static const struct keytoggle_config keytoggle_config_##n = {                             \
        .keycodes = DT_INST_PROP(n, keycodes),                                                   \
        .tap_ms = DT_INST_PROP_OR(n, tap_ms, 30),                                                  \
        .wait_ms = DT_INST_PROP_OR(n, wait_ms, 15),                                                \
    };                                                                                         \
    static struct keytoggle_state keytoggle_state_##n = {                                     \
        .is_pressed = false,                                                                  \
    };                                                                                         \
    DEVICE_DT_INST_DEFINE(n, keytoggle_init, NULL, &keytoggle_state_##n,                       \
                          &keytoggle_config_##n, POST_KERNEL,                                  \
                          CONFIG_INPUT_PROCESSOR_INIT_PRIORITY, &keytoggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KEYTOGGLE_INST)

