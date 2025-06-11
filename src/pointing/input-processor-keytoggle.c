#define DT_DRV_COMPAT zmk_input_processor_keytoggle

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>

#include "input_processor_keytoggle.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void key_release_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    struct keytoggle_state *state = CONTAINER_OF(d_work, struct keytoggle_state, key_release_work);

    if (state->is_pressed) {
        LOG_DBG("Releasing keycode: 0x%02X", state->config->keycode);
        ZMK_BEHAVIOR_QUEUE_RELEASE(0, state->config->keycode);
        state->is_pressed = false;
    }
}

static int keytoggle_handle_event(const struct device *dev, struct input_event *event, uint32_t param1, uint32_t param2, struct zmk_input_processor_state *processor_state) {
    const struct keytoggle_config *config = dev->config;
    struct keytoggle_state *state = dev->data;

    if (event->type == INPUT_EV_REL) {
        // Relative movement detected, press the key if not already pressed
        if (!state->is_pressed) {
            LOG_DBG("Pressing keycode: 0x%02X", config->keycode);
            ZMK_BEHAVIOR_QUEUE_PRESS(0, config->keycode);
            state->is_pressed = true;
        }
        // Reset the key release timer
        k_work_reschedule(&state->key_release_work, K_MSEC(100)); // Adjust delay as needed
    } else if (event->type == INPUT_EV_SYN && event->code == INPUT_SYN_REPORT) {
        // If no relative movement, and it's a sync report, check if we need to release the key
        // This is handled by the delayed work, so no direct action here.
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
        .keycode = DT_INST_PROP(n, keycode),                                                   \
    };

    static struct keytoggle_state keytoggle_state_##n = {                                     \
        .is_pressed = false,                                                                  \
    };

    DEVICE_DT_INST_DEFINE(n, keytoggle_init, NULL, &keytoggle_state_##n,                       \
                          &keytoggle_config_##n, POST_KERNEL,                                  \
                          CONFIG_INPUT_PROCESSOR_INIT_PRIORITY, &keytoggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KEYTOGGLE_INST)

