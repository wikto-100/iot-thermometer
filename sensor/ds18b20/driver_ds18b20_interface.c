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

#include "driver_ds18b20_interface.h"
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#define DS18B20_OW_PIN 14

#include "hardware/sync.h"

static uint32_t gs_ds18b20_irq_state;
/**
 * @brief  interface bus init
 * @return status code
 *         - 0 success
 *         - 1 bus init failed
 * @note   none
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
 */
void ds18b20_interface_delay_us(uint32_t us)
{
    busy_wait_us_32(us);
}

/**
 * @brief interface enable the interrupt
 * @note  none
 */
void ds18b20_interface_enable_irq(void)
{
    restore_interrupts(gs_ds18b20_irq_state);
}

/**
 * @brief interface disable the interrupt
 * @note  none
 */
void ds18b20_interface_disable_irq(void)
{
    gs_ds18b20_irq_state = save_and_disable_interrupts();
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void ds18b20_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
