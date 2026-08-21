/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      driver_ds18b20_interface_template.c
 * @brief     driver ds18b20 interface template source file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-04-06
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/04/06  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/12/20  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

/*
 * ============================================================================
 * DS18B20 INTERFACE - THE RASPBERRY PI PICO SPECIFIC "GLUE"
 * ============================================================================
 *
 * This is the DS18B20 equivalent of
 * ../../common/nrf24l01/driver_nrf24l01_interface.c: the small set of
 * functions that know how to talk to THIS chip (RP2350) and THIS specific
 * wiring, filling in the low-level details the vendored, hardware-
 * independent DS18B20 driver (../../external/ds18b20/) needs.
 *
 * WHAT IS "1-WIRE", AND WHY DOES read/write TOGGLE THE PIN DIRECTION?
 * Unlike SPI (used for the nRF24L01+ radio, with separate wires for data
 * in, data out, and clock), 1-Wire really does use just ONE single wire
 * for all data, shared between talking AND listening - plus a ground
 * wire. This is done with a trick called "open-drain": the wire is
 * pulled HIGH by default (via a resistor, physically on the circuit
 * board - a "pull-up"), and any device on the bus can pull it LOW simply
 * by connecting it to ground, but nothing ever actively drives it HIGH.
 * On a microcontroller GPIO pin, this is implemented by SWITCHING the
 * pin's direction back and forth:
 *   - To send a "0" bit: set the pin as an OUTPUT and drive it low.
 *   - To send a "1" bit, or to simply LISTEN (read): set the pin back to
 *     an INPUT, so it stops actively driving anything and just lets the
 *     pull-up resistor pull the wire back HIGH (or lets whatever the
 *     DS18B20 is doing take over instead).
 * This is exactly what ds18b20_interface_write() below does - notice it
 * never calls anything like gpio_put(pin, 1) to drive the pin high itself,
 * it only ever drives it LOW (for a 0) or releases it (for a 1) by
 * switching back to input mode. The precise TIMING of these pulses (how
 * long the pin stays low, etc.) is what actually encodes the data - that
 * timing logic lives in the vendored driver itself, not in this file; this
 * file only provides the raw "pull it low" / "read it" / "wait this many
 * microseconds" primitives that timing logic is built out of.
 *
 * WHY DISABLE INTERRUPTS DURING BUS OPERATIONS?
 * Because the 1-Wire protocol's timing is measured in single MICROSECONDS,
 * an interrupt firing at the wrong moment (pausing our code for even a
 * short time to go handle something else, like the radio's own interrupt)
 * could stretch a pulse's timing enough to corrupt it. ds18b20_interface_
 * enable_irq() / _disable_irq() below let the vendored driver briefly
 * silence ALL interrupts on this chip around its most timing-sensitive
 * bit-level operations, then restore them afterwards - trading a very
 * brief moment of reduced responsiveness elsewhere in the program for
 * reliable communication with the sensor.
 */

#include "driver_ds18b20_interface.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"

/* Which GPIO pin the DS18B20's single data wire is connected to. "OW"
 * stands for "One-Wire", another common name for this same 1-Wire
 * protocol. */
#define DS18B20_OW_PIN 14

#include "hardware/sync.h"

/* Remembers the chip's interrupt state across a disable/enable pair below,
 * so enabling can restore exactly whatever state interrupts were
 * previously in (rather than just blindly turning them back on, which
 * could be wrong if they were already off for some other reason). */
static uint32_t gs_ds18b20_irq_state;

/**
 * @brief  interface bus init
 * @return status code
 *         - 0 success
 *         - 1 bus init failed
 * @note   none
 *
 * Configures the pin as an input first (matching the "open-drain" idea
 * above - idle state is "listening", not "driving"), then checks that it
 * actually reads HIGH, as the pull-up resistor should make it - if it
 * reads LOW instead, something is wrong with the physical wiring (a
 * missing pull-up resistor, a short circuit, etc.), so this reports
 * failure rather than continuing on a bus that clearly is not working
 * correctly.
 */
uint8_t ds18b20_interface_init(void)
{
    gpio_init(DS18B20_OW_PIN);
    gpio_put(DS18B20_OW_PIN, 0);
    gpio_set_dir(DS18B20_OW_PIN, GPIO_IN);

    if (gpio_get(DS18B20_OW_PIN) == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief  interface bus deinit
 * @return status code
 *         - 0 success
 *         - 1 bus deinit failed
 * @note   none
 */
uint8_t ds18b20_interface_deinit(void)
{
    gpio_set_dir(DS18B20_OW_PIN, GPIO_IN);
    gpio_deinit(DS18B20_OW_PIN);
    return 0;
}

/**
 * @brief      interface bus read
 * @param[out] *value pointer to a value buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t ds18b20_interface_read(uint8_t *value)
{
    if (value == NULL)
        return 1;
    *value = gpio_get(DS18B20_OW_PIN);
    return 0;
}

/**
 * @brief     interface bus write
 * @param[in] value written value
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 *
 * See the "open-drain" explanation at the top of this file: a "0" is sent
 * by actively driving the pin low (switching it to an output); a "1" is
 * sent by simply releasing the pin back to being an input, letting the
 * external pull-up resistor pull it back high on its own.
 */
uint8_t ds18b20_interface_write(uint8_t value)
{
    if (value == 0U)
    {
        gpio_put(DS18B20_OW_PIN, 0);
        gpio_set_dir(DS18B20_OW_PIN, GPIO_OUT);
    }
    else
    {
        gpio_set_dir(DS18B20_OW_PIN, GPIO_IN);
    }
    return 0;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void ds18b20_interface_delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);

    if (ms > 0U && ticks == 0U)
    {
        ticks = 1U;
    }

    vTaskDelay(ticks);
}

/**
 * @brief     interface delay us
 * @param[in] us time
 * @note      none
 *
 * Unlike ds18b20_interface_delay_ms() above (which uses FreeRTOS's
 * vTaskDelay(), letting OTHER tasks run during the wait), this uses
 * busy_wait_us_32() - a tight loop that does nothing else until the exact
 * requested number of MICROSECONDS has passed. 1-Wire's bit timing is far
 * too short and precise for FreeRTOS's own millisecond-granularity
 * task-switching delay to be usable here; this trades away CPU efficiency
 * for exact timing, for these very brief moments.
 */
void ds18b20_interface_delay_us(uint32_t us)
{
    busy_wait_us_32(us);
}

/**
 * @brief interface enable the interrupt
 * @note  none
 *
 * See the "why disable interrupts" explanation at the top of this file.
 * restore_interrupts() puts interrupts back into WHATEVER state they were
 * actually in before the matching ds18b20_interface_disable_irq() call
 * below (captured into gs_ds18b20_irq_state at that point) - not simply
 * "turn interrupts back on unconditionally".
 */
void ds18b20_interface_enable_irq(void)
{
    restore_interrupts(gs_ds18b20_irq_state);
}

/**
 * @brief interface disable the interrupt
 * @note  none
 *
 * save_and_disable_interrupts() does two things at once: it remembers the
 * CURRENT interrupt state (so ds18b20_interface_enable_irq() above can put
 * it back correctly later) and then turns interrupts off.
 */
void ds18b20_interface_disable_irq(void)
{
    gs_ds18b20_irq_state = save_and_disable_interrupts();
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 *
 * A small wrapper around the standard C library's vprintf(), the same
 * pattern used by nrf24l01_interface_debug_print() in
 * ../../common/nrf24l01/driver_nrf24l01_interface.c - see that function's
 * comment for the fuller explanation of what va_list/vprintf() do.
 */
void ds18b20_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
