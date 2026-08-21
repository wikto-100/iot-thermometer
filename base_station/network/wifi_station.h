/*
 * ============================================================================
 * WI-FI STATION - PUBLIC INTERFACE
 * ============================================================================
 *
 * This module handles connecting the base station to a Wi-Fi network, so it
 * can be reached over the network by a web browser. "Station mode" (hence
 * the file name) is the Wi-Fi term for "join an existing network as a
 * client", the same way a laptop or phone joins your home Wi-Fi - as
 * opposed to "access point mode", where the device would create its OWN
 * network for others to join.
 *
 * The Pico 2 W has a separate chip for Wi-Fi and Bluetooth, called the
 * CYW43. This module is a thin wrapper around the Pico SDK's driver for
 * that chip.
 */

#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include <stdint.h>

/**
 * @brief Initialize CYW43 and connect to the configured Wi-Fi network
 *
 * This turns on the CYW43 Wi-Fi chip and tries to join the network named by
 * WIFI_SSID, using the password WIFI_PASSWORD. Both of those are NOT
 * written anywhere in the source code - they are passed in at build time
 * (see the WIFI_SSID / WIFI_PASSWORD options in CMakeLists.txt), so the
 * Wi-Fi password never has to be committed to the code itself. This
 * function waits until the connection either succeeds or times out before
 * returning, so it can take a few seconds.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t wifi_station_connect(void);

#endif
