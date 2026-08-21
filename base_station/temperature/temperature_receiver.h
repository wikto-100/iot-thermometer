#ifndef TEMPERATURE_RECEIVER_H
#define TEMPERATURE_RECEIVER_H

#include <stdint.h>

/**
 * @brief Create the nRF24L01 temperature receiver tasks
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t temperature_receiver_start(void);

/**
 * @brief Ask the sensor for a fresh reading
 *
 * Triggers a REQUEST/READING round trip asynchronously; any reply is
 * published through the normal receiver path once it arrives. This
 * function itself returns immediately without waiting for one.
 *
 * @return 0 once the request has been handed to the radio task,
 *         1 if the radio task is not running yet
 */
uint8_t temperature_receiver_request(void);

#endif
