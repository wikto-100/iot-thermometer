#ifndef BASE_STATION_NETWORK_H
#define BASE_STATION_NETWORK_H

#include <stdint.h>

/**
 * @brief Create the task that connects Wi-Fi and starts the HTTP server
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t base_station_network_start(void);

#endif
