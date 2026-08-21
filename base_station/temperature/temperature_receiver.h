#ifndef TEMPERATURE_RECEIVER_H
#define TEMPERATURE_RECEIVER_H

#include <stdint.h>

/**
 * @brief Create the nRF24L01 temperature receiver task
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t temperature_receiver_start(void);

#endif
