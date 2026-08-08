#ifndef TEMPERATURE_SENDER_H
#define TEMPERATURE_SENDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the temperature sender application task
 *
 * Call this before vTaskStartScheduler().
 *
 * @return 0 on success, 1 on failure
 */
uint8_t temperature_sender_start(void);

#ifdef __cplusplus
}
#endif

#endif