#ifndef BASE_STATION_NETWORK_H
#define BASE_STATION_NETWORK_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the base-station network task
 *
 * @return 0 on success, 1 if task creation failed
 */
uint8_t base_station_network_start(void);
#ifdef __cplusplus
}
#endif

#endif