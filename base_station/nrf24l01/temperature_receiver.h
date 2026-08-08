#ifndef TEMPERATURE_RECEIVER_H
#define TEMPERATURE_RECEIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the temperature receiver task
 *
 * Call this before vTaskStartScheduler().
 *
 * @return 0 on success, 1 on failure
 */
uint8_t temperature_receiver_start(void);

#ifdef __cplusplus
}
#endif

#endif