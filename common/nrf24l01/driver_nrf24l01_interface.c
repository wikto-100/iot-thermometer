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
 * @file      driver_nrf24l01_interface_template.c
 * @brief     driver nrf24l01 interface template source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2021-11-28
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/11/28  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */
/*
 * ============================================================================
 * NRF24L01+ INTERFACE - THE RASPBERRY PI PICO SPECIFIC "GLUE"
 * ============================================================================
 *
 * This file is what the big "LINK pattern" comment at the top of
 * driver_nrf24l01_temperature.c (in this same folder) refers to: it is the
 * set of small functions that actually know how to talk to THIS chip
 * (RP2350) and THIS specific wiring, filling in the "how do I do a basic
 * hardware operation" details the vendored, hardware-independent
 * nRF24L01+ driver (../../external/nrf24l01/) needs but does not know how
 * to do itself. Both boards use this exact same file, since both wire
 * their nRF24L01+ chip to the same set of pins.
 *
 * WHAT IS SPI?
 * SPI ("Serial Peripheral Interface") is a simple, very common way for a
 * microcontroller to talk to a nearby chip over a handful of wires: one
 * wire carries data FROM the Pico TO the chip (MOSI, "master out, slave
 * in"), one carries data the other way (MISO, "master in, slave out"),
 * one is a shared clock signal that keeps both sides in sync bit-by-bit
 * (SCK), and one, "chip select" (here called CSN, since it is
 * active-LOW - see the temperature driver's IRQ pin explanation for what
 * "active-low" means), tells the chip "the next few bits are meant for
 * you" - this matters because several different chips can share the same
 * MOSI/MISO/SCK wires, each with its OWN separate chip-select wire, so the
 * microcontroller can talk to one at a time. This project's nRF24L01+
 * chip is the only thing on this particular SPI bus, but the CSN handling
 * below still follows the same standard protocol regardless.
 *
 * This file's functions are grouped into: SPI communication (talking to
 * the chip), GPIO control (the separate CE pin, described in the
 * temperature driver file), timing (delays), debug printing, and one
 * default event-handling function that this project does not actually
 * use (see nrf24l01_interface_receive_callback() near the bottom).
 */

#include <stdarg.h>
#include <stdio.h>
#include "driver_nrf24l01_interface.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "FreeRTOS.h"
#include "task.h"

/* Which of the Pico's two built-in SPI hardware peripherals to use, and
 * how fast to run it. */
#define NRF24_SPI spi0
#define NRF24_SPI_CLOCK_HZ 4000000u

/* Which physical GPIO pins the nRF24L01+ chip is wired to. MISO/CSN/SCK/
 * MOSI are the four SPI wires described above; CE ("chip enable") is the
 * separate control pin explained in driver_nrf24l01_temperature.c. */
#define NRF24_MISO_PIN 16u
#define NRF24_CSN_PIN 17u
#define NRF24_SCK_PIN 18u
#define NRF24_MOSI_PIN 19u
#define NRF24_CE_PIN 20u

/**
 * @brief  interface spi bus init
 * @return status code
 *         - 0 success
 *         - 1 spi init failed
 * @note   none
 *
 * CSN ("chip select, active low") starts HIGH (deselected/idle) - see the
 * "what is SPI" explanation at the top of this file for why. The three
 * MISO/MOSI/SCK pins are handed over to the Pico's dedicated SPI hardware
 * peripheral (gpio_set_function(..., GPIO_FUNC_SPI)) rather than being
 * driven manually, which is both faster and simpler than "bit-banging" SPI
 * by hand in software. spi_set_format() configures the details the
 * nRF24L01+ chip's datasheet requires: 8 data bits per transfer, clock
 * idles low with data sampled on the leading edge (CPOL=0, CPHA=0 - one of
 * four standard SPI "modes"), and the most-significant bit of each byte
 * sent first.
 */
uint8_t nrf24l01_interface_spi_init(void)
{
    gpio_init(NRF24_CSN_PIN);
    gpio_set_dir(NRF24_CSN_PIN, GPIO_OUT);
    gpio_put(NRF24_CSN_PIN, 1);

    spi_init(NRF24_SPI, NRF24_SPI_CLOCK_HZ);

    gpio_set_function(NRF24_MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(NRF24_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(NRF24_SCK_PIN, GPIO_FUNC_SPI);

    spi_set_format(NRF24_SPI,
                   8,
                   SPI_CPOL_0,
                   SPI_CPHA_0,
                   SPI_MSB_FIRST);

    return 0;
}

/**
 * @brief  interface spi bus deinit
 * @return status code
 *         - 0 success
 *         - 1 spi deinit failed
 * @note   none
 */
uint8_t nrf24l01_interface_spi_deinit(void)
{
    gpio_put(NRF24_CSN_PIN, 1);

    spi_deinit(NRF24_SPI);

    gpio_deinit(NRF24_MISO_PIN);
    gpio_deinit(NRF24_SCK_PIN);
    gpio_deinit(NRF24_MOSI_PIN);
    gpio_deinit(NRF24_CSN_PIN);

    return 0;
}
/**
 * @brief      interface spi bus read
 * @param[in]  reg register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 *
 * Every single SPI conversation with the chip follows this same shape:
 * pull CSN LOW to say "listen to me now", send the one-byte command that
 * says which register we want, then send/receive the actual data, then
 * pull CSN back HIGH to say "done talking to you". This function is the
 * "read" version: after the command byte, it reads len bytes of data back
 * from the chip (sending 0xFF as filler while doing so, since SPI always
 * transfers data in both directions at once, even when only one direction
 * actually matters).
 */
uint8_t nrf24l01_interface_spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{

    int transferred;

    if (len > 0 && buf == NULL)
        return 1;
    gpio_put(NRF24_CSN_PIN, 0); // begin transaction

    // write the read command stored in reg
    transferred = spi_write_blocking(NRF24_SPI, &reg, 1);

    // if read command not transferred
    if (transferred != 1)
    {
        // end transaction, failure
        gpio_put(NRF24_CSN_PIN, 1);
        return 1;
    }
    // read into buf
    if (len > 0)
    {
        transferred = spi_read_blocking(NRF24_SPI, 0xFF, buf, len);
        // if not all was transferred
        if (transferred != len)
        {
            // end transaction, failure
            gpio_put(NRF24_CSN_PIN, 1);
            return 1;
        }
    }

    // success, end transaction
    gpio_put(NRF24_CSN_PIN, 1);

    return 0;
}

/**
 * @brief     interface spi bus write
 * @param[in] reg register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 *
 * The write counterpart to nrf24l01_interface_spi_read() above - same
 * CSN-low / command-byte / data / CSN-high shape, just sending buf's
 * contents to the chip instead of reading the chip's data back into it.
 */
uint8_t nrf24l01_interface_spi_write(uint8_t reg, uint8_t *buf, uint16_t len)
{

    int transferred;

    if (len > 0 && buf == NULL)
        return 1;
    // initiate transaction
    gpio_put(NRF24_CSN_PIN, 0);

    // transfer write command
    transferred = spi_write_blocking(NRF24_SPI, &reg, 1);

    if (transferred != 1)
    {
        gpio_put(NRF24_CSN_PIN, 1);
        return 1;
    }
    // write
    if (len > 0)
    {
        transferred = spi_write_blocking(NRF24_SPI, buf, len);
        if (transferred != len)
        {
            gpio_put(NRF24_CSN_PIN, 1);
            return 1;
        }
    }
    gpio_put(NRF24_CSN_PIN, 1);

    return 0;
}

/**
 * @brief  interface gpio init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 *
 * This is the CE ("chip enable") pin - see the "keep CE low" explanation
 * in driver_nrf24l01_temperature.c's nrf24l01_temperature_configure()
 * function for what it controls. It starts LOW (inactive) here at
 * startup, and is only raised once the chip has been fully configured.
 */
uint8_t nrf24l01_interface_gpio_init(void)
{
    gpio_init(NRF24_CE_PIN);
    gpio_set_dir(NRF24_CE_PIN, GPIO_OUT);
    gpio_put(NRF24_CE_PIN, 0);
    return 0;
}

/**
 * @brief  interface gpio deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t nrf24l01_interface_gpio_deinit(void)
{
    gpio_put(NRF24_CE_PIN, 0);
    gpio_deinit(NRF24_CE_PIN);
    return 0;
}

/**
 * @brief     interface gpio write
 * @param[in] data written data
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t nrf24l01_interface_gpio_write(uint8_t data)
{

    gpio_put(NRF24_CE_PIN, data != 0);
    return 0;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 *
 * vTaskDelay() is FreeRTOS's normal "pause this task" function - it works
 * in "ticks" (see configTICK_RATE_HZ in FreeRTOSConfig.h), not
 * milliseconds directly, so pdMS_TO_TICKS() converts the requested
 * millisecond count into the matching number of ticks. The small check
 * below guards against a subtle rounding trap: if the caller asked for a
 * very small delay that would round DOWN to 0 ticks, vTaskDelay(0) would
 * not actually pause at all - this bumps it up to a minimum of 1 tick
 * instead, so "delay a little bit" never accidentally means "don't delay
 * at all".
 */
void nrf24l01_interface_delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);

    if (ms > 0U && ticks == 0U)
    {
        ticks = 1U;
    }

    vTaskDelay(ticks);
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 *
 * A small wrapper around the standard C library's vprintf(), which prints
 * formatted text (the same "%s", "%d" style formatting as printf()) using
 * a va_list - the mechanism C uses for functions like this one that accept
 * a variable number of arguments (the "..." in this function's own
 * signature). This is the function every "...debug_print(...)" call
 * throughout this project's radio code ultimately goes through, over
 * whichever console stdio_init_all() set up (see app/main.c on either
 * board).
 */
void nrf24l01_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief     interface receive callback
 * @param[in] type receive callback type
 * @param[in] num pipe number
 * @param[in] *buf pointer to a data buffer
 * @param[in] len buffer length
 * @note      none
 *
 * IMPORTANT: despite its name, this particular function is NOT actually
 * used anywhere in this project's real operation - it is a leftover
 * default/example implementation from the vendored driver's own template
 * pattern (every board that uses this driver is expected to provide a
 * "receive callback" function with THIS exact shape; this happens to be
 * the generic example one). Both real boards instead provide their OWN
 * callback function and link it in via DRIVER_NRF24L01_LINK_RECEIVE_CALLBACK
 * inside driver_nrf24l01_temperature.c's nrf24l01_temperature_init() - see
 * temperature_receiver_callback() in
 * base_station/temperature/temperature_receiver.c, or
 * temperature_sender_callback() in
 * sensor/nrf24l01/temperature_sender.c, for the functions that ACTUALLY
 * run when the radio has something to report.
 */
void nrf24l01_interface_receive_callback(uint8_t type, uint8_t num, uint8_t *buf, uint8_t len)
{
    switch (type)
    {
    case NRF24L01_INTERRUPT_RX_DR:
    {
        uint8_t i;

        nrf24l01_interface_debug_print("nrf24l01: irq receive with pipe %d with %d.\n", num, len);
        for (i = 0; i < len; i++)
        {
            nrf24l01_interface_debug_print("0x%02X ", buf[i]);
        }
        nrf24l01_interface_debug_print(".\n");

        break;
    }
    case NRF24L01_INTERRUPT_TX_DS:
    {
        nrf24l01_interface_debug_print("nrf24l01: irq send ok.\n");

        break;
    }
    case NRF24L01_INTERRUPT_MAX_RT:
    {
        nrf24l01_interface_debug_print("nrf24l01: irq reach max retry times.\n");

        break;
    }
    case NRF24L01_INTERRUPT_TX_FULL:
    {
        nrf24l01_interface_debug_print("nrf24l01: irq tx full.\n");

        break;
    }
    default:
    {
        nrf24l01_interface_debug_print("nrf24l01: unknown code.\n");

        break;
    }
    }
}
