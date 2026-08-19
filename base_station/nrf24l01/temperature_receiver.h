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

/**
 * @brief Get the most recently received temperature
 *
 * @param[out] temperature_centi_c latest temperature in hundredths
 *                                of a degree Celsius
 *
 * @return 0 when a temperature is available
 *         1 when no temperature has been received
 */
uint8_t temperature_receiver_get_latest(
    int16_t *temperature_centi_c
);

#ifdef __cplusplus
}
#endif

#endif