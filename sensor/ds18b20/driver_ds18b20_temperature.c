/*
 * ============================================================================
 * DS18B20 TEMPERATURE DRIVER - IMPLEMENTATION
 * ============================================================================
 *
 * See driver_ds18b20_temperature.h for the "why" (what this chip is, and
 * why this wrapper exists). This file follows the exact same "LINK
 * pattern" of dependency injection explained at length in
 * ../../common/nrf24l01/driver_nrf24l01_temperature.c - the
 * DRIVER_DS18B20_LINK_*() calls below plug this board's own
 * Pico-specific functions (in ./driver_ds18b20_interface.c) into the
 * vendored, hardware-independent DS18B20 driver, the same way the radio
 * driver does for the nRF24L01+ chip. See that file if the LINK pattern
 * itself is unfamiliar.
 *
 * TWO DS18B20-SPECIFIC IDEAS WORTH KNOWING
 *
 * "ROM" ADDRESSES AND "SKIP ROM" MODE
 * The 1-Wire protocol this chip uses (mentioned in the header file) is
 * designed to let MULTIPLE sensors share the same single data wire, each
 * with its own unique factory-programmed 64-bit "ROM" address (similar in
 * spirit to the "pipes and addresses" idea for the nRF24L01+ radio - see
 * that driver's header file) - normally, you would address ONE specific
 * sensor by its ROM code before talking to it. This project has exactly
 * ONE DS18B20 on the bus, so there is no need to pick a specific one out
 * of several - "Skip ROM" mode tells the chip "do not bother with
 * addressing, just talk to whichever single sensor is out there", which is
 * simpler and slightly faster.
 *
 * THE "SCRATCHPAD" AND CRC
 * The chip has a small internal memory area called the "scratchpad" where
 * it stores its settings (like the resolution, set below) and its most
 * recent measurement result. Reading a measurement means reading this
 * scratchpad back over the wire, and - just like the nRF24L01+ radio
 * packets (see that driver's header file) - the data includes a CRC
 * ("cyclic redundancy check") value the vendored driver checks
 * automatically, so a measurement corrupted in transit over the wire is
 * detected and rejected rather than silently trusted.
 */

#include "driver_ds18b20_temperature.h"

#include <stddef.h>

#include "driver_ds18b20.h"
#include "driver_ds18b20_interface.h"

/*
 * At 12-bit resolution, one raw DS18B20 count represents
 * 1/16 of a degree Celsius.
 *
 * The chip does not report temperature directly as "hundredths of a
 * degree" - it reports it as its own internal RAW count, whose meaning
 * depends on the configured resolution (see DS18B20_RESOLUTION_12BIT
 * below). At 12-bit resolution specifically, each raw count step
 * represents exactly 1/16th of a degree Celsius - these two constants are
 * what the math in ds18b20_temperature_read() below uses to convert that
 * raw count into this project's own "hundredths of a degree" convention
 * (see ../../common/temperature_format.h for why that convention is used
 * everywhere in this project).
 */
#define DS18B20_TEMPERATURE_RAW_COUNTS_PER_C 16L
#define DS18B20_TEMPERATURE_CENTI_PER_C      100L

/* The one shared "handle" structure the vendored driver uses to track this
 * sensor's state - see the matching gs_temperature_handle explanation in
 * the nRF24L01+ radio driver for the same idea. There is only ever one
 * DS18B20 on this board, so one global variable is enough. */
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
     *
     * See the "LINK pattern" explanation at the top of this file - each
     * call below plugs in one specific Pico-specific function (from
     * driver_ds18b20_interface.c) for a capability the vendored driver
     * needs but does not know how to do itself: reading/writing the
     * 1-Wire bus, waiting, and (new compared to the radio driver)
     * enabling/disabling interrupts - the 1-Wire protocol's timing is
     * strict enough that an interrupt firing at the wrong moment mid-bit
     * could corrupt communication, so the vendored driver briefly
     * disables interrupts around the most timing-sensitive parts and
     * re-enables them afterwards, using these two functions.
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

    /* With everything wired up above, this hands control to the vendored
     * driver to perform its actual startup sequence over the 1-Wire bus -
     * the first real communication with the physical chip. */
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
     *
     * See the "ROM addresses and Skip ROM mode" explanation at the top of
     * this file.
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
     *
     * The DS18B20 can be configured at several different resolutions
     * (fewer bits = a coarser, but FASTER, measurement; more bits = finer
     * detail, but a slower conversion - see the timing note in the header
     * file). 12-bit is the chip's highest available resolution, giving
     * the most precise readings this sensor can provide, at the cost of
     * the conversion taking up to roughly 750 milliseconds.
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
     *
     * See the "scratchpad and CRC" explanation at the top of this file.
     * "driver_temperature" (a float, a fractional number) is the
     * vendored driver's OWN ready-made floating-point result - we do not
     * actually use it below, since this project deliberately works in
     * whole-number "hundredths of a degree" everywhere instead (see
     * ../../common/temperature_format.h for why) - we only need
     * "raw_temperature", the unconverted count this function converts by
     * hand below.
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
     *
     * Plain integer division in C always ROUNDS TOWARD ZERO, throwing
     * away any leftover fraction rather than rounding to the nearest
     * whole number (for example, 7 / 2 = 3, not the "nearer" value of
     * 3.5 rounded to 4). Adding HALF the divisor before dividing is a
     * standard trick that turns that truncation into proper
     * round-to-nearest behavior instead - it works the same way manually
     * rounding "3.7" up to "4" by looking at whether the fractional part
     * is at least a half does. The +/- split below handles positive and
     * negative temperatures symmetrically, so a value like -7.6 rounds to
     * -8, not toward 0 or in some inconsistent direction.
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
 *
 * Simply hands off to the vendored driver's own cleanup function. Neither
 * this file nor temperature_sender.c currently calls this during normal
 * operation - it exists mainly as a cleanup path for when
 * ds18b20_temperature_init() itself, or the radio's own setup right after
 * it, fails partway through (see temperature_sender_task() in
 * ../nrf24l01/temperature_sender.c).
 */
uint8_t ds18b20_temperature_deinit(void)
{
    if (ds18b20_deinit(&gs_ds18b20_handle) != 0U)
    {
        return 1;
    }

    return 0;
}
