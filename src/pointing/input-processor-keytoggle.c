#define DT_DRV_COMPAT zmk_input_processor_keytoggle

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/input/input.h>
#include <zephyr/device.h>
#include <zephyr/sys/dlist.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/virtual_key_position.h>
#include <zmk/behavior_queue.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Configuration structure for the keytoggle input processor.
struct keytoggle_config {
    const struct zmk_behavior_binding *bindings;
    uint32_t release_delay_ms; // Delay after last movement before releasing the key.
    uint32_t tap_ms;   // Delay before the initial key press event.
    uint32_t wait_ms;  // Delay for subsequent behavior bindings (not used for hold/release logic).
};

// State structure for each instance of the keytoggle input processor.
struct keytoggle_state {
    bool is_pressed; // Flag to track if the key is currently pressed.
    const struct keytoggle_config *config; // Pointer to the instance's configuration.
    struct k_work_delayable key_release_work; // Work item for delayed key release.
};

/**
 * @brief Callback function for delayed key release.
 *
 * This function is executed when the `key_release_work` delayable work item
 * times out, indicating that no movement events have been detected for a while.
 * It releases the configured key if it was previously pressed.
 *
 * @param work Pointer to the `k_work` item that triggered this callback.
 */
static void key_release_callback(struct k_work *work) {
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    struct keytoggle_state *state = CONTAINER_OF(d_work, struct keytoggle_state, key_release_work);
    const struct keytoggle_config *config = state->config;

    struct zmk_behavior_binding_event behavior_event = {
        .layer = zmk_keymap_highest_layer_active(),
        .position = ZMK_VIRTUAL_KEY_POSITION_SENSOR(0),
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    if (state->is_pressed) {
        int ret = zmk_behavior_invoke_binding(&config->bindings[0], behavior_event, false);
        if (ret < 0) {
            LOG_WRN("Failed to invoke key release: %d", ret);
        }
        state->is_pressed = false;
        LOG_DBG("Key released");
    }
}

/**
 * @brief Handles input events for the keytoggle input processor.
 *
 * This function is called by the ZMK input subsystem whenever an input event
 * occurs. It specifically processes `INPUT_EV_REL` (relative movement) events
 * to control the state of the configured keycode.
 *
 * @param dev Pointer to the device instance of this input processor.
 * @param event Pointer to the input event structure.
 * @param param1 Event-specific parameter 1.
 * @param param2 Event-specific parameter 2.
 * @param processor_state Pointer to the current input processor state.
 * @return ZMK_INPUT_PROC_CONTINUE to continue processing the event.
 */
static int keytoggle_handle_event(const struct device *dev, struct input_event *event,
                                 uint32_t param1, uint32_t param2, struct zmk_input_processor_state *processor_state) {
    struct keytoggle_state *state = dev->data;
    const struct keytoggle_config *config = dev->config;

    if (event->type == INPUT_EV_REL) {
        // Only process non-zero movements to avoid triggering on zero events
        if (event->value != 0) {
            if (!state->is_pressed) {
                struct zmk_behavior_binding_event behavior_event = {
                    .layer = zmk_keymap_highest_layer_active(),
                    .position = ZMK_VIRTUAL_KEY_POSITION_SENSOR(0),
                    .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                    .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
                };
                
                int ret = zmk_behavior_invoke_binding(&config->bindings[0], behavior_event, true);
                if (ret < 0) {
                    LOG_WRN("Failed to invoke key press: %d", ret);
                    return ret;
                }
                state->is_pressed = true;
                LOG_DBG("Key pressed on movement: code=%d, value=%d", event->code, event->value);
            }
            // Reset the release timer on any movement
            k_work_reschedule(&state->key_release_work, K_MSEC(config->release_delay_ms));
        }
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

/**
 * @brief Initializes the keytoggle input processor device.
 *
 * This function is called during device initialization. It initializes the
 * delayable work item used for key release and sets up the configuration pointer.
 *
 * @param dev Pointer to the device instance to initialize.
 * @return 0 on success.
 */
static int keytoggle_init(const struct device *dev) {
    struct keytoggle_state *state = dev->data;
    state->config = dev->config; // Set the state's config pointer.
    k_work_init_delayable(&state->key_release_work, key_release_callback);
    return 0;
}

// Define the input processor driver API.
static const struct zmk_input_processor_driver_api keytoggle_driver_api = {
    .handle_event = keytoggle_handle_event,
};

#define TRANSFORMED_BINDINGS(n) \
    {LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))}

// Macro to define and instantiate each keytoggle input processor from device tree.
#define KEYTOGGLE_INST(n)                                                                      \
    /* Define the configuration for instance 'n' */                                            \
    static struct zmk_behavior_binding zip_keytoggle_config_bindings_##n[] =                    \
        TRANSFORMED_BINDINGS(n);                                                              \
    static const struct keytoggle_config keytoggle_config_##n = {                             \
        .bindings = zip_keytoggle_config_bindings_##n,                                         \
        .release_delay_ms = DT_INST_PROP_OR(n, release_delay_ms, 100),                        \
        .tap_ms = DT_INST_PROP_OR(n, tap_ms, 0),                                               \
        .wait_ms = DT_INST_PROP_OR(n, wait_ms, 0),                                             \
    };                                                                                         \
    /* Define the state for instance 'n' */                                                    \
    static struct keytoggle_state keytoggle_state_##n = {                                     \
        .is_pressed = false,                                                                  \
        /* .config will be set in keytoggle_init */                                            \
    };                                                                                         \
    /* Define the device instance 'n' */                                                       \
    DEVICE_DT_INST_DEFINE(n, keytoggle_init, NULL, &keytoggle_state_##n,                       \
                          &keytoggle_config_##n, POST_KERNEL, \
                          60, &keytoggle_driver_api);

// Iterate over all enabled instances of zmk,input-processor-keytoggle in the device tree.
DT_INST_FOREACH_STATUS_OKAY(KEYTOGGLE_INST)
