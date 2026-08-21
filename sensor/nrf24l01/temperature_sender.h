/*
 * ============================================================================
 * TEMPERATURE SENDER - PUBLIC INTERFACE
 * ============================================================================
 *
 * This module owns the nRF24L01+ radio chip and the DS18B20 temperature
 * sensor on this board. It is responsible for sending a temperature
 * reading over the radio whenever the physical button is pressed, or
 * whenever a REQUEST for one arrives over the radio. See
 * temperature_sender.c for the full explanation of how this works.
 */

#ifndef TEMPERATURE_SENDER_H
#define TEMPERATURE_SENDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the temperature sender application task
 *
 * Call this before vTaskStartScheduler() (see sensor.cpp). It creates the
 * background task that does everything this board is responsible for:
 * initializing the DS18B20 sensor and the radio, listening for the button
 * and for incoming REQUESTs, and sending READING packets in response to
 * either.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t temperature_sender_start(void);

#ifdef __cplusplus
}
#endif

#endif
