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
#include <stdarg.h>
#include <stdio.h>
#include "driver_nrf24l01_interface.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "FreeRTOS.h"
#include "task.h"

#define NRF24_SPI spi0
#define NRF24_SPI_CLOCK_HZ 4000000u

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
