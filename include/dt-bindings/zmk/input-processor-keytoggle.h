#ifndef ZMK_INPUT_PROCESSOR_KEYTOGGLE_H
#define ZMK_INPUT_PROCESSOR_KEYTOGGLE_H

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>

struct keytoggle_config {
    uint32_t keycode;
};

struct keytoggle_state {
    bool is_pressed;
    const struct keytoggle_config *config; // Add pointer to config
    struct k_work_delayable key_release_work;
};

#endif // ZMK_INPUT_PROCESSOR_KEYTOGGLE_H

