#ifndef DRIVER_DS18B20_TEMPERATURE_H
#define DRIVER_DS18B20_TEMPERATURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the single DS18B20 temperature sensor
 *
 * Configures:
 * - Skip ROM mode for one sensor on the bus
 * - 12-bit temperature resolution
 *
 * @return 0 on success, 1 on failure
 */
uint8_t ds18b20_temperature_init(void);

/**
 * @brief Read temperature in hundredths of a degree Celsius
 *
 * Examples:
 *     2345 means 23.45 °C
 *     -725 means -7.25 °C
 *
 * @param[out] temperature_centi_c measured temperature
 *
 * @return 0 on success, 1 on failure
 */
uint8_t ds18b20_temperature_read(
    int16_t *temperature_centi_c
);

/**
 * @brief Deinitialize the DS18B20 temperature sensor
 *
 * @return 0 on success, 1 on failure
 */
uint8_t ds18b20_temperature_deinit(void);

#ifdef __cplusplus
}
#endif

#endif