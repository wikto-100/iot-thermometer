#include "temperature_receiver.h"

#include "temperature_store.h"

#include "driver_nrf24l01_interface.h"
#include "driver_nrf24l01_temperature.h"

#include "task_utils.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#define NRF24L01_IRQ_PIN 21U
#define TEMPERATURE_RECEIVER_TASK_STACK_DEPTH 512U
#define TEMPERATURE_RECEIVER_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

static TaskHandle_t gs_temperature_receiver_task_handle = NULL;

/**
 * @brief Print a temperature represented in hundredths of a degree Celsius
 */
static void temperature_receiver_print(int16_t temperature_centi_c)
{
    int32_t value = temperature_centi_c;

    if (value < 0)
    {
        value = -value;

        nrf24l01_interface_debug_print(
            "temperature receiver: -%ld.%02ld C.\n",
            (long)(value / 100),
            (long)(value % 100));
    }
    else
    {
        nrf24l01_interface_debug_print(
            "temperature receiver: %ld.%02ld C.\n",
            (long)(value / 100),
            (long)(value % 100));
    }
}

/**
 * @brief Process an event reported by the radio driver
 *
 * This callback executes in the receiver task because that task calls
 * nrf24l01_temperature_irq_handler().
 */
static void temperature_receiver_callback(
    uint8_t type,
    uint8_t pipe,
    uint8_t *buf,
    uint8_t len)
{
    nrf24l01_temperature_payload_t payload;

    switch (type)
    {
        case NRF24L01_INTERRUPT_RX_DR:
        {
            if (pipe != 0U)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: unexpected pipe %u.\n",
                    pipe);

                return;
            }

            if (nrf24l01_temperature_decode(
                    buf,
                    len,
                    &payload) != 0)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: invalid payload length %u.\n",
                    len);

                return;
            }

            if (temperature_store_set(
                    payload.temperature_centi_c) != 0)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: could not publish reading.\n");

                return;
            }

            temperature_receiver_print(
                payload.temperature_centi_c);

            break;
        }

        case NRF24L01_INTERRUPT_TX_DS:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: TX_DS event.\n");

            break;
        }

        case NRF24L01_INTERRUPT_MAX_RT:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: MAX_RT event.\n");

            break;
        }

        case NRF24L01_INTERRUPT_TX_FULL:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: TX FIFO full.\n");

            break;
        }

        default:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: unknown event %u.\n",
                type);

            break;
        }
    }
}

/**
 * @brief Wake the receiver task when the active-low radio IRQ pin falls
 *
 * This callback executes in hardware-interrupt context.
 */
static void temperature_receiver_gpio_irq_callback(
    uint gpio,
    uint32_t events)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (gpio != NRF24L01_IRQ_PIN)
    {
        return;
    }

    if ((events & GPIO_IRQ_EDGE_FALL) == 0U)
    {
        return;
    }

    if (gs_temperature_receiver_task_handle == NULL)
    {
        return;
    }

    vTaskNotifyGiveFromISR(
        gs_temperature_receiver_task_handle,
        &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/**
 * @brief Configure the active-low radio IRQ input
 */
static void temperature_receiver_irq_setup(void)
{
    gpio_init(NRF24L01_IRQ_PIN);
    gpio_set_dir(NRF24L01_IRQ_PIN, GPIO_IN);
    gpio_pull_up(NRF24L01_IRQ_PIN);

    gpio_set_irq_enabled_with_callback(
        NRF24L01_IRQ_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        temperature_receiver_gpio_irq_callback);

    /* Catch an interrupt that became active before edge detection. */
    if (gpio_get(NRF24L01_IRQ_PIN) == 0)
    {
        xTaskNotifyGive(gs_temperature_receiver_task_handle);
    }
}

/**
 * @brief Initialize the radio receiver and process its interrupts
 */
static void temperature_receiver_task(void *parameter)
{
    (void)parameter;

    gs_temperature_receiver_task_handle =
        xTaskGetCurrentTaskHandle();

    if (nrf24l01_temperature_init(
            NRF24L01_TEMPERATURE_TYPE_RX,
            temperature_receiver_callback) != 0)
    {
        nrf24l01_interface_debug_print(
            "temperature receiver: radio initialization failed.\n");

        gs_temperature_receiver_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    temperature_receiver_irq_setup();

    nrf24l01_interface_debug_print(
        "temperature receiver: initialized and listening.\n");

    for (;;)
    {
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        while (gpio_get(NRF24L01_IRQ_PIN) == 0)
        {
            if (nrf24l01_temperature_irq_handler() != 0)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: IRQ handler failed.\n");

                vTaskDelay(pdMS_TO_TICKS(1U));
            }
        }
    }
}

uint8_t temperature_receiver_start(void)
{
    return task_utils_create(
        temperature_receiver_task,
        "temperature_rx",
        TEMPERATURE_RECEIVER_TASK_STACK_DEPTH,
        NULL,
        TEMPERATURE_RECEIVER_TASK_PRIORITY);
}
