/*
 * ============================================================================
 * TEMPERATURE RECEIVER - PUBLIC INTERFACE
 * ============================================================================
 *
 * This module owns the nRF24L01+ radio chip on the base station: it listens
 * for temperature readings sent wirelessly by the sensor board, and it can
 * also ask the sensor for a fresh reading on demand (used by the "Request
 * temperature" button on the web page).
 *
 * The full explanation of HOW it does this - the radio chip, interrupts,
 * FreeRTOS tasks, and the request/response protocol - lives in
 * temperature_receiver.c, right next to the actual code. This header only
 * lists the two functions the rest of the program is allowed to call.
 */

#ifndef TEMPERATURE_RECEIVER_H
#define TEMPERATURE_RECEIVER_H

#include <stdint.h>

/**
 * @brief Create the nRF24L01 temperature receiver tasks
 *
 * Call this once during startup (see app/main.c). It sets up the radio
 * chip and creates the background FreeRTOS tasks that listen for incoming
 * readings and handle outgoing requests. After this returns successfully,
 * the radio is live and listening - no further setup is needed.
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t temperature_receiver_start(void);

/**
 * @brief Ask the sensor for a fresh reading
 *
 * This is called from the web server whenever someone clicks the "Request
 * temperature" button on the page. It does NOT talk to the radio directly
 * itself - instead it just wakes up the background task that owns the
 * radio and tells it "please send a request", then returns immediately
 * without waiting to see what happens next. This matters because the
 * function that handles web requests must stay fast; the radio exchange
 * that follows can take up to roughly a second (mostly spent waiting for
 * the sensor to take a new temperature reading), which would make the web
 * page feel frozen if we waited for it here.
 *
 * If the sensor does reply, its reading is picked up automatically by the
 * usual "an incoming reading arrived" code path and appears in the
 * temperature history the next time the web page refreshes (about once a
 * second) - the caller of this function does not need to do anything
 * further to see the result.
 *
 * @return 0 once the request has been handed to the radio task,
 *         1 if the radio task is not running yet (should not normally
 *         happen, since temperature_receiver_start() is always called
 *         during startup before the web server can receive any requests)
 */
uint8_t temperature_receiver_request(void);

#endif
