/*
 * ============================================================================
 * BASE STATION NETWORK - PUBLIC INTERFACE
 * ============================================================================
 *
 * This module is the "glue" between two things that are each handled by
 * their own file: connecting to Wi-Fi (wifi_station.c) and starting the web
 * server (../web/web_server.c). It runs both of those, in order, on their
 * own background FreeRTOS task, so the rest of the program does not have to
 * wait for Wi-Fi to connect before it can carry on with other work.
 */

#ifndef BASE_STATION_NETWORK_H
#define BASE_STATION_NETWORK_H

#include <stdint.h>

/**
 * @brief Create the task that connects Wi-Fi and starts the HTTP server
 *
 * Call this once during startup (see app/main.c). It creates a background
 * task that connects to Wi-Fi and then starts the web server; this
 * function itself returns almost immediately, without waiting for the
 * Wi-Fi connection to actually finish.
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t base_station_network_start(void);

#endif
