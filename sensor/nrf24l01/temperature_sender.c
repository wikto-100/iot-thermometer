#include "temperature_sender.h"

#include "driver_nrf24l01_interface.h"
#include "driver_nrf24l01_temperature.h"
#include "driver_ds18b20_temperature.h"
#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#define NRF24L01_IRQ_PIN 21U

#define SENDER_BUTTON_PIN 15U

#define NRF24L01_IRQ_TASK_STACK_DEPTH 512U
#define TEMPERATURE_SENDER_TASK_STACK_DEPTH 512U

#define NRF24L01_IRQ_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)

#define TEMPERATURE_SENDER_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

/*
 * Temporary test temperature:
 * 2345 represents 23.45 degrees Celsius.
 */
#define TEST_TEMPERATURE_CENTI_C 2345

static TaskHandle_t gs_nrf24l01_irq_task_handle = NULL;
static TaskHandle_t temperature_sender_task_handle = NULL;
/**
 * @brief Return the positive fractional component for printing
 */
static unsigned int temperature_fraction(
    int16_t temperature_centi_c)
{
    int16_t fraction = temperature_centi_c % 100;

    if (fraction < 0)
    {
        fraction = -fraction;
    }

    return (unsigned int)fraction;
}

/**
 * @brief Process an event reported by the radio driver
 *
 * This callback executes in the IRQ task because that task calls
 * nrf24l01_temperature_irq_handler(). RX_DR is not expected today
 * since the base station only acknowledges sends, but decoding it
 * here means the sender is ready once the base station also
 * transmits back to it.
 */
static void temperature_sender_callback(
    uint8_t type,
    uint8_t pipe,
    uint8_t *buf,
    uint8_t len)
{
    switch (type)
    {
    case NRF24L01_INTERRUPT_RX_DR:
    {
        nrf24l01_temperature_payload_t payload;

        if (pipe != 0U)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: unexpected pipe %u.\n",
                pipe);

            break;
        }

        if (nrf24l01_temperature_decode(
                buf,
                len,
                &payload) != 0)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: invalid payload length %u.\n",
                len);

            break;
        }

        nrf24l01_interface_debug_print(
            "nrf24l01 sender: received %d.%02u C.\n",
            payload.temperature_centi_c / 100,
            temperature_fraction(
                payload.temperature_centi_c));

        break;
    }

    case NRF24L01_INTERRUPT_TX_DS:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: irq send ok.\n");

        break;
    }

    case NRF24L01_INTERRUPT_MAX_RT:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: irq reach max retry times.\n");

        break;
    }

    case NRF24L01_INTERRUPT_TX_FULL:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: irq tx full.\n");

        break;
    }

    default:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: unknown code.\n");

        break;
    }
    }
}

/**
 * @brief Wake the IRQ task when the IRQ pin falls/rises
 *
 * This executes in hardware-interrupt context. Radio-driver calls
 * and SPI communication are deferred to nrf24l01_irq_task().
 */
static void gpio_irq_callback(
    uint gpio,
    uint32_t events)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    TaskHandle_t task_to_notify = NULL;
    uint32_t expected_edge;

    if (gpio == NRF24L01_IRQ_PIN)
    {
        task_to_notify = gs_nrf24l01_irq_task_handle;
        expected_edge = GPIO_IRQ_EDGE_FALL;
    }
    else if (gpio == SENDER_BUTTON_PIN)
    {
        task_to_notify = temperature_sender_task_handle;
        expected_edge = GPIO_IRQ_EDGE_RISE;
    }
    if (task_to_notify == NULL)
    {
        return;
    }
    if ((events & expected_edge) == 0)
    {
        return;
    }

    vTaskNotifyGiveFromISR(
        task_to_notify,
        &higher_priority_task_woken);

    portYIELD_FROM_ISR(
        higher_priority_task_woken);
    return;
}

/**
 * @brief Process nRF24L01 events in FreeRTOS task context
 *
 * nrf24l01_temperature_irq_handler() reads and clears the radio's
 * interrupt status. The core driver then invokes
 * temperature_sender_callback().
 */
static void nrf24l01_irq_task(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        /*
         * Sleep until the GPIO ISR gives this task a notification.
         */
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        /*
         * The nRF24L01+ holds IRQ low until its pending status flags
         * are cleared.
         *
         * Loop in case another event becomes pending while the first
         * event is being processed.
         */
        while (gpio_get(NRF24L01_IRQ_PIN) == 0)
        {
            if (nrf24l01_temperature_irq_handler() != 0)
            {
                nrf24l01_interface_debug_print(
                    "nrf24l01 sender: IRQ handler failed.\n");

                /*
                 * Prevent a tight retry loop after a persistent
                 * communication failure.
                 */
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

/**
 * @brief Create the IRQ task and enable the GP21 interrupt
 *
 * @return 0 on success, 1 on failure
 */
static uint8_t nrf24l01_irq_setup(void)
{
    BaseType_t result;

    /*
     * Create the IRQ task before enabling the hardware interrupt.
     * The GPIO callback will therefore always have a valid task
     * handle.
     */
    result = xTaskCreate(
        nrf24l01_irq_task,
        "nrf24_irq",
        NRF24L01_IRQ_TASK_STACK_DEPTH,
        NULL,
        NRF24L01_IRQ_TASK_PRIORITY,
        &gs_nrf24l01_irq_task_handle);

    if (result != pdPASS)
    {
        gs_nrf24l01_irq_task_handle = NULL;

        return 1;
    }

    /*
     * The nRF24L01+ IRQ signal is active low.
     */
    gpio_init(NRF24L01_IRQ_PIN);
    gpio_set_dir(NRF24L01_IRQ_PIN, GPIO_IN);
    gpio_pull_up(NRF24L01_IRQ_PIN);

    gpio_set_irq_enabled_with_callback(
        NRF24L01_IRQ_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        gpio_irq_callback);

    /*
     * An event could become pending immediately before falling-edge
     * detection is enabled. In that case the pin is already low and
     * there will be no new falling edge.
     */
    if (gpio_get(NRF24L01_IRQ_PIN) == 0)
    {
        xTaskNotifyGive(
            gs_nrf24l01_irq_task_handle);
    }

    return 0;
}

static void sender_button_setup(void)
{
    gpio_init(SENDER_BUTTON_PIN);
    gpio_set_dir(SENDER_BUTTON_PIN, GPIO_IN);
    gpio_set_irq_enabled(
        SENDER_BUTTON_PIN,
        GPIO_IRQ_EDGE_RISE,
        true);
}

/**
 * @brief Initialize the transmitter and periodically send temperature
 */
static void temperature_sender_task(void *parameter)
{
    nrf24l01_temperature_payload_t payload;

    (void)parameter;

    if (ds18b20_temperature_init() != 0U)
    {
        nrf24l01_interface_debug_print(
            "temperature sender: sensor initialization failed.\n");

        vTaskDelete(NULL);
        return;
    }
    /*
     * Configure the radio as the temperature transmitter.
     *
     * temperature_sender_callback() reports TX_DS, MAX_RT, RX_DR,
     * and TX_FULL events, decoding RX_DR as a temperature payload.
     */
    if (nrf24l01_temperature_init(
            NRF24L01_TEMPERATURE_TYPE_TX,
            temperature_sender_callback) != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: radio initialization failed.\n");

        (void)ds18b20_temperature_deinit();
        vTaskDelete(NULL);
        return;
    }

    /*
     * The IRQ path must be operational before the first blocking
     * nrf24l01_temperature_send() call.
     */
    if (nrf24l01_irq_setup() != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: IRQ setup failed.\n");

        (void)nrf24l01_temperature_deinit();

        vTaskDelete(NULL);
        return;
    }

    sender_button_setup();


    for (;;)
    {
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        if (ds18b20_temperature_read(
                &payload.temperature_centi_c) != 0U)
        {
            nrf24l01_interface_debug_print(
                "temperature sender: sensor read failed.\n");

            continue;
        }

        if (nrf24l01_temperature_send(&payload) == 0)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: sent %d.%02u C.\n",
                payload.temperature_centi_c / 100,
                temperature_fraction(
                    payload.temperature_centi_c));
        }
        else
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: send failed.\n");
        }
    }
}

/**
 * @brief Create the sender application task
 */
uint8_t temperature_sender_start(void)
{
    BaseType_t result;

    result = xTaskCreate(
        temperature_sender_task,
        "temperature_tx",
        TEMPERATURE_SENDER_TASK_STACK_DEPTH,
        NULL,
        TEMPERATURE_SENDER_TASK_PRIORITY,
        &temperature_sender_task_handle);

    if (result != pdPASS)
    {
        return 1;
    }

    return 0;
}