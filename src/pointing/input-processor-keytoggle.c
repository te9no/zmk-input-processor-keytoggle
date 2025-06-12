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

// Configuration structure for the keytoggle input processor.
struct keytoggle_config {
    uint32_t keycode; // The keycode to be pressed/released.
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
    // Get the parent `keytoggle_state` structure from the `k_work_delayable` item.
    struct k_work_delayable *d_work = k_work_delayable_from_work(work);
    struct keytoggle_state *state = CONTAINER_OF(d_work, struct keytoggle_state, key_release_work);

    // If the key is currently pressed, release it.
    if (state->is_pressed) {
        LOG_DBG("Releasing keycode: 0x%02X after no movement detected.", state->config->keycode);
        // Send a key release event. Position and timestamp can be dummy values
        // for a simple release without an originating position state change.
        struct zmk_behavior_binding_event ev = {
            .position = 0, // Dummy position
            .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
        };
        zmk_behavior_queue_add(&ev, state->config->keycode, false, 0); // No additional delay for release
        state->is_pressed = false; // Update the pressed state.
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
static int keytoggle_handle_event(const struct device *dev, struct input_event *event, uint32_t param1, uint32_t param2, struct zmk_input_processor_state *processor_state) {
    const struct keytoggle_config *config = dev->config; // Get the configuration for this instance.
    struct keytoggle_state *state = dev->data;           // Get the state for this instance.

    // Check if the event type is a relative movement event.
    if (event->type == INPUT_EV_REL) {
        // Create a behavior binding event structure.
        // The position here is a placeholder as relative movement doesn't map directly
        // to a keymap position, but ZMK behavior queue requires it.
        struct zmk_behavior_binding_event ev = {
            .position = 0, // Use a dummy position for events not tied to a physical key.
            .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
        };

        // If a relative movement is detected and the key is not currently pressed, press it.
        if (!state->is_pressed) {
            LOG_DBG("Pressing keycode: 0x%02X due to movement detection.", config->keycode);
            // Send a key press event. `config->tap_ms` is the delay before this press.
            zmk_behavior_queue_add(&ev, config->keycode, true, config->tap_ms);
            state->is_pressed = true; // Mark the key as pressed.
        }

        // Reset the key release timer every time movement is detected.
        // This keeps the key pressed as long as movement continues.
        // The key will only be released if no movement is detected for `release_delay_ms`.
        k_work_reschedule(&state->key_release_work, K_MSEC(config->release_delay_ms));
    }

    // Always continue processing the event, as this input processor only controls a key state.
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

// Macro to define and instantiate each keytoggle input processor from device tree.
#define KEYTOGGLE_INST(n)                                                                      \
    /* Define the configuration for instance 'n' */                                            \
    static const struct keytoggle_config keytoggle_config_##n = {                             \
        .keycode = DT_INST_PROP(n, keycode),                                                   \
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
                          &keytoggle_config_##n, POST_KERNEL,                                  \
                          CONFIG_INPUT_PROCESSOR_INIT_PRIORITY, &keytoggle_driver_api);

// Iterate over all enabled instances of zmk,input-processor-keytoggle in the device tree.
DT_INST_FOREACH_STATUS_OKAY(KEYTOGGLE_INST)
