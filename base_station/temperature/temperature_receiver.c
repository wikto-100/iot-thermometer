#include "temperature_receiver.h"

#include "temperature_store.h"

#include "driver_nrf24l01_interface.h"
#include "driver_nrf24l01_temperature.h"

#include "task_utils.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#define NRF24L01_IRQ_PIN 21U

#define NRF24L01_IRQ_TASK_STACK_DEPTH 512U
#define TEMPERATURE_REQUEST_TASK_STACK_DEPTH 512U

/*
 * Higher than the request task so a real radio event always preempts
 * an in-flight request/response round trip, matching the sensor's
 * nrf24l01_irq_task / temperature_sender_task priority ordering.
 */
#define NRF24L01_IRQ_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)

#define TEMPERATURE_REQUEST_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

static TaskHandle_t gs_nrf24l01_irq_task_handle = NULL;
static TaskHandle_t gs_temperature_request_task_handle = NULL;

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
 * This callback executes in the IRQ task because that task calls
 * nrf24l01_temperature_irq_handler(). A READING is published whether it
 * arrived unprompted or as the reply to a REQUEST - either way it is
 * simply the latest known temperature.
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

            if (payload.opcode != NRF24L01_TEMPERATURE_OPCODE_READING)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: unexpected opcode %u.\n",
                    (unsigned int)payload.opcode);

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
 * @brief Wake the IRQ task when the active-low radio IRQ pin falls
 *
 * This executes in hardware-interrupt context.
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

    if (gs_nrf24l01_irq_task_handle == NULL)
    {
        return;
    }

    vTaskNotifyGiveFromISR(
        gs_nrf24l01_irq_task_handle,
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
        xTaskNotifyGive(gs_nrf24l01_irq_task_handle);
    }
}

/**
 * @brief Initialize the radio and process its interrupts
 *
 * Owns every SPI transaction driven by a radio event: unsolicited
 * READINGs, and the TX_DS/MAX_RT completion of a REQUEST sent by
 * temperature_request_task(). Keeping all of that on one task is what
 * lets that task's nrf24l01_temperature_send() block safely - this
 * task is free to process the resulting interrupt while it waits.
 */
static void nrf24l01_irq_task(void *parameter)
{
    (void)parameter;

    gs_nrf24l01_irq_task_handle =
        xTaskGetCurrentTaskHandle();

    /*
     * The radio listens by default. Sending a REQUEST briefly
     * switches to TX and back (see temperature_request_task()).
     */
    if (nrf24l01_temperature_init(
            NRF24L01_TEMPERATURE_TYPE_RX,
            temperature_receiver_callback) != 0)
    {
        nrf24l01_interface_debug_print(
            "temperature receiver: radio initialization failed.\n");

        gs_nrf24l01_irq_task_handle = NULL;
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

/**
 * @brief Send a REQUEST and return to listening
 *
 * Woken by temperature_receiver_request(). Any reply is decoded and
 * published by nrf24l01_irq_task() the same way an unprompted READING
 * is, so this task does not wait for one.
 */
static void temperature_request_task(void *parameter)
{
    nrf24l01_temperature_payload_t payload;

    (void)parameter;

    gs_temperature_request_task_handle =
        xTaskGetCurrentTaskHandle();

    payload.opcode = NRF24L01_TEMPERATURE_OPCODE_REQUEST;
    payload.temperature_centi_c = 0;

    for (;;)
    {
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        if (nrf24l01_temperature_set_mode(
                NRF24L01_TEMPERATURE_TYPE_TX) != 0)
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: request: switch to TX failed.\n");

            continue;
        }

        if (nrf24l01_temperature_send(&payload) == 0)
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: request sent.\n");
        }
        else
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: request send failed.\n");
        }

        /*
         * Always try to resume listening, even after a failed send,
         * so one lost REQUEST doesn't strand the base station unable
         * to hear the sensor's own unprompted readings.
         */
        if (nrf24l01_temperature_set_mode(
                NRF24L01_TEMPERATURE_TYPE_RX) != 0)
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: request: switch to RX failed.\n");
        }
    }
}

uint8_t temperature_receiver_start(void)
{
    if (task_utils_create(
            nrf24l01_irq_task,
            "temperature_rx",
            NRF24L01_IRQ_TASK_STACK_DEPTH,
            NULL,
            NRF24L01_IRQ_TASK_PRIORITY) != 0)
    {
        return 1;
    }

    if (task_utils_create(
            temperature_request_task,
            "temperature_req",
            TEMPERATURE_REQUEST_TASK_STACK_DEPTH,
            NULL,
            TEMPERATURE_REQUEST_TASK_PRIORITY) != 0)
    {
        return 1;
    }

    return 0;
}

uint8_t temperature_receiver_request(void)
{
    if (gs_temperature_request_task_handle == NULL)
    {
        return 1;
    }

    xTaskNotifyGive(gs_temperature_request_task_handle);

    return 0;
}
