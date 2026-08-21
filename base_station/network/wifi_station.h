#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include <stdint.h>

/**
 * @brief Initialize CYW43 and connect to the configured Wi-Fi network
 *
 * WIFI_SSID and WIFI_PASSWORD must be supplied as compile definitions.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t wifi_station_connect(void);

#endif
