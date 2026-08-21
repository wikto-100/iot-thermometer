/*
 * ============================================================================
 * DS18B20 TEMPERATURE DRIVER - PUBLIC INTERFACE
 * ============================================================================
 *
 * This is the sensor board's own thin wrapper around the vendored,
 * hardware-independent DS18B20 driver (../../external/ds18b20/), the same
 * pattern used for the nRF24L01+ radio (see
 * ../../common/nrf24l01/driver_nrf24l01_temperature.h for a fuller
 * explanation of why this "wrapper around a generic vendored driver"
 * pattern is used throughout this project). It applies the specific
 * settings this project needs (explained in driver_ds18b20_temperature.c),
 * and exposes just three simple functions: initialize, read, deinitialize.
 *
 * WHAT IS THE DS18B20?
 * It is a small, cheap digital temperature sensor chip. Unlike an
 * "analog" sensor (which would output a voltage the microcontroller has to
 * measure and interpret itself), the DS18B20 does its own temperature
 * measurement internally and reports the result as a precise DIGITAL
 * number, over a simple two-wire-style protocol called "1-Wire" (data and
 * ground share one wire, with power optionally taken from that same wire
 * too - see ../ds18b20/driver_ds18b20_interface.c for the actual GPIO pin
 * this project wires it to).
 */

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
 * ("Skip ROM mode" and "12-bit resolution" are explained in
 * driver_ds18b20_temperature.c, next to the actual code that sets them.)
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
 * Like the rest of this project, the result is a whole number representing
 * HUNDREDTHS of a degree, not a fractional number of degrees directly -
 * see ../../common/temperature_format.h for a fuller explanation of why.
 * This call physically triggers a fresh measurement on the sensor and
 * waits for it to finish, which can take a noticeable moment (up to
 * roughly 750 milliseconds at the 12-bit resolution this project uses) -
 * it does not just return whatever the sensor happened to measure last.
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
