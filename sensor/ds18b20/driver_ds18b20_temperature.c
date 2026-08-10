#include "driver_ds18b20_temperature.h"

#include <stddef.h>

#include "driver_ds18b20.h"
#include "driver_ds18b20_interface.h"

/*
 * At 12-bit resolution, one raw DS18B20 count represents
 * 1/16 of a degree Celsius.
 */
#define DS18B20_TEMPERATURE_RAW_COUNTS_PER_C 16L
#define DS18B20_TEMPERATURE_CENTI_PER_C      100L

static ds18b20_handle_t gs_ds18b20_handle;

/**
 * @brief Initialize the single DS18B20 temperature sensor
 */
uint8_t ds18b20_temperature_init(void)
{
    uint8_t result;

    /*
     * Connect the platform-independent DS18B20 driver to
     * the RP2350 interface functions.
     */
    DRIVER_DS18B20_LINK_INIT(
        &gs_ds18b20_handle,
        ds18b20_handle_t
    );

    DRIVER_DS18B20_LINK_BUS_INIT(
        &gs_ds18b20_handle,
        ds18b20_interface_init
    );

    DRIVER_DS18B20_LINK_BUS_DEINIT(
        &gs_ds18b20_handle,
        ds18b20_interface_deinit
    );

    DRIVER_DS18B20_LINK_BUS_READ(
        &gs_ds18b20_handle,
        ds18b20_interface_read
    );

    DRIVER_DS18B20_LINK_BUS_WRITE(
        &gs_ds18b20_handle,
        ds18b20_interface_write
    );

    DRIVER_DS18B20_LINK_DELAY_MS(
        &gs_ds18b20_handle,
        ds18b20_interface_delay_ms
    );

    DRIVER_DS18B20_LINK_DELAY_US(
        &gs_ds18b20_handle,
        ds18b20_interface_delay_us
    );

    DRIVER_DS18B20_LINK_ENABLE_IRQ(
        &gs_ds18b20_handle,
        ds18b20_interface_enable_irq
    );

    DRIVER_DS18B20_LINK_DISABLE_IRQ(
        &gs_ds18b20_handle,
        ds18b20_interface_disable_irq
    );

    DRIVER_DS18B20_LINK_DEBUG_PRINT(
        &gs_ds18b20_handle,
        ds18b20_interface_debug_print
    );

    result = ds18b20_init(&gs_ds18b20_handle);

    if (result != 0U)
    {
        ds18b20_interface_debug_print(
            "ds18b20 temperature: initialization failed with %u.\n",
            result
        );

        return 1;
    }

    /*
     * Skip ROM addresses every device on the bus.
     * This project currently has exactly one DS18B20.
     */
    result = ds18b20_set_mode(
        &gs_ds18b20_handle,
        DS18B20_MODE_SKIP_ROM
    );

    if (result != 0U)
    {
        ds18b20_interface_debug_print(
            "ds18b20 temperature: setting Skip ROM mode failed.\n"
        );

        (void)ds18b20_deinit(&gs_ds18b20_handle);

        return 1;
    }

    /*
     * 12-bit resolution produces one raw count per 0.0625 °C.
     */
    result = ds18b20_scratchpad_set_resolution(
        &gs_ds18b20_handle,
        DS18B20_RESOLUTION_12BIT
    );

    if (result != 0U)
    {
        ds18b20_interface_debug_print(
            "ds18b20 temperature: setting resolution failed.\n"
        );

        (void)ds18b20_deinit(&gs_ds18b20_handle);

        return 1;
    }

    return 0;
}

/**
 * @brief Read temperature in hundredths of a degree Celsius
 */
uint8_t ds18b20_temperature_read(
    int16_t *temperature_centi_c
)
{
    int16_t raw_temperature;
    int32_t scaled_temperature;
    float driver_temperature;

    if (temperature_centi_c == NULL)
    {
        return 1;
    }

    /*
     * The core driver:
     * - starts temperature conversion
     * - waits for completion
     * - reads the scratchpad
     * - validates the CRC
     * - returns raw and floating-point results
     */
    if (ds18b20_read(
            &gs_ds18b20_handle,
            &raw_temperature,
            &driver_temperature) != 0U)
    {
        return 1;
    }

    /*
     * At 12-bit resolution:
     *
     *     Celsius = raw / 16
     *
     * Therefore:
     *
     *     centi-Celsius = raw * 100 / 16
     */
    scaled_temperature =
        (int32_t)raw_temperature *
        DS18B20_TEMPERATURE_CENTI_PER_C;

    /*
     * Add half of the divisor before integer division to round
     * to the closest whole centi-degree.
     */
    if (scaled_temperature >= 0)
    {
        scaled_temperature +=
            DS18B20_TEMPERATURE_RAW_COUNTS_PER_C / 2L;
    }
    else
    {
        scaled_temperature -=
            DS18B20_TEMPERATURE_RAW_COUNTS_PER_C / 2L;
    }

    *temperature_centi_c = (int16_t)(
        scaled_temperature /
        DS18B20_TEMPERATURE_RAW_COUNTS_PER_C
    );

    return 0;
}

/**
 * @brief Deinitialize the DS18B20 temperature sensor
 */
uint8_t ds18b20_temperature_deinit(void)
{
    if (ds18b20_deinit(&gs_ds18b20_handle) != 0U)
    {
        return 1;
    }

    return 0;
}