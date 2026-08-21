/*
 * ============================================================================
 * TEMPERATURE SENDER - READING THE SENSOR AND TALKING OVER RADIO
 * ============================================================================
 *
 * This file controls the nRF24L01+ radio chip on the sensor board (a
 * Raspberry Pi Pico 2) and drives this board's entire job: reading the
 * DS18B20 temperature sensor and sending readings wirelessly.
 *
 * WHAT IS THE nRF24L01+ AND HOW DO WE TALK TO IT?
 * It is a small, cheap radio chip. It does not understand C code - this
 * board talks to it over a wire protocol called SPI (a simple "send some
 * bytes, receive some bytes" connection), sending it commands like "send
 * this data" or "tell me what you last received". All of that low-level
 * SPI detail is handled by code this file does not need to touch: the
 * vendored driver in external/nrf24l01/, wrapped by
 * common/nrf24l01/driver_nrf24l01_temperature.c, which adds the specific
 * settings this project needs (radio channel, packet format, etc.). This
 * file only calls the small set of functions that wrapper exposes, such as
 * nrf24l01_temperature_send() and nrf24l01_temperature_decode().
 *
 * HOW DOES THE CHIP TELL US SOMETHING HAPPENED?
 * The chip has a physical output pin called IRQ ("interrupt request"). It
 * pulls this pin LOW (0 volts) whenever something happens that we should
 * know about - most importantly, "a new packet has arrived". This is
 * called an "active-low" signal, because LOW means "something is going
 * on" rather than HIGH. The Pico's own GPIO hardware can watch a pin and
 * immediately jump into our code the instant it changes from HIGH to LOW
 * (a "falling edge") - this is an "interrupt". Interrupts let us react to
 * the radio the moment something arrives, instead of having to constantly
 * ask "did anything happen yet?" in a loop.
 *
 * WHAT THIS BOARD DOES
 * This board has a DS18B20 temperature sensor physically attached (see
 * ../ds18b20/), and sends a fresh reading over the radio for two
 * different reasons:
 *   1. Someone presses the physical button wired to this board.
 *   2. A REQUEST packet arrives over the radio, asking for one.
 * Both of these end up doing the EXACT SAME THING - read the sensor, send
 * a READING - so both paths wake the SAME task (temperature_sender_task
 * below) rather than needing two separate pieces of "read and send" code.
 *
 * WHY TWO FREERTOS TASKS?
 * This board splits its work across two FreeRTOS tasks: nrf24l01_irq_task,
 * which is always listening for and processing radio interrupt events, and
 * temperature_sender_task, which does the actual reading and sending. This
 * split avoids a subtle deadlock: nrf24l01_temperature_send() (called from
 * temperature_sender_task) BLOCKS until the chip confirms a packet was
 * sent, waiting for a TX_DS interrupt - and that confirmation can only be
 * noticed and processed by whichever task is actively watching the radio's
 * IRQ pin. If that were the SAME task doing the (blocking) sending, it
 * would be stuck waiting for an event that only it could notice, and could
 * never get back to noticing it - every send would simply hang until it
 * eventually times out and fails. Having a separate, always-listening task
 * means the send can safely block, because the other task remains free to
 * notice and process the interrupt that completes it.
 *
 * A SHARED GPIO CALLBACK
 * This board has TWO separate physical things that can trigger a hardware
 * interrupt: the radio's IRQ pin, and the board's own push-button. The
 * Raspberry Pi Pico SDK only allows ONE function to be registered as "the"
 * GPIO interrupt callback at a time (see gpio_set_irq_enabled_with_callback()
 * below) - registering a second one does not ADD a second handler, it
 * REPLACES the first. So instead of two separate callback functions, this
 * file has one shared gpio_irq_callback() function (further down) that
 * looks at WHICH pin triggered it and decides what to do from there.
 *
 * WHAT ARE "TASK NOTIFICATIONS"?
 * FreeRTOS gives every task a built-in, extremely lightweight "mailbox"
 * that can hold a single number. A task can pause itself with
 * ulTaskNotifyTake(), which means "go to sleep until somebody notifies
 * me". Any other code - including code running inside a hardware
 * interrupt, using the special "FromISR" version of the function - can
 * wake it back up by calling (v/x)TaskNotifyGive(...). This is how the
 * radio's IRQ pin wakes nrf24l01_irq_task, and it is also how the button
 * and an incoming REQUEST both wake temperature_sender_task: a simple,
 * fast "tap on the shoulder" between two independent tasks, using almost
 * no memory.
 *
 * THE REQUEST/RESPONSE RADIO PROTOCOL: REQUEST AND READING
 * Every radio packet exchanged over this link starts with one "opcode"
 * byte that says what kind of packet it is (see
 * common/nrf24l01/driver_nrf24l01_temperature.h):
 *
 *   REQUEST - received by this board. Carries no temperature value; it
 *             just means "please send a fresh reading".
 *   READING - sent by this board. Carries an actual temperature value.
 *             This board sends one either because its own physical button
 *             was pressed, or because it just received a REQUEST.
 */

#include "temperature_sender.h"

#include "driver_nrf24l01_interface.h"
#include "driver_nrf24l01_temperature.h"
#include "driver_ds18b20_temperature.h"

#include "task_utils.h"
#include "temperature_format.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

/* Which GPIO pin the physical push-button is wired to. */
#define SENDER_BUTTON_PIN 15U

/*
 * "Stack depth" is how much scratch memory (for local variables, function
 * call bookkeeping, etc.) FreeRTOS sets aside for each task. Too small and
 * the task could crash by running out of room; these values were chosen
 * generously enough for the small amount of work each task does. The unit
 * here is "words" (4 bytes each on this chip), not bytes.
 */
#define NRF24L01_IRQ_TASK_STACK_DEPTH 512U
#define TEMPERATURE_SENDER_TASK_STACK_DEPTH 512U

/*
 * The radio IRQ task is given a HIGHER priority than the task that sends
 * packets, so a real radio event always gets processed promptly even if
 * the sender task happens to be running at the same moment.
 */
#define NRF24L01_IRQ_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)

#define TEMPERATURE_SENDER_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

/* "Handles" are how FreeRTOS lets one piece of code refer to a specific
 * task, so it can be woken up later (for example, from inside a hardware
 * interrupt). This file uses BOTH styles of getting a handle (see the
 * difference explained at nrf24l01_irq_setup() and
 * temperature_sender_task() below): one self-registers, the other is
 * captured directly from task_utils_create(), because of a real ordering
 * requirement explained where it matters. */
static TaskHandle_t gs_nrf24l01_irq_task_handle = NULL;
static TaskHandle_t temperature_sender_task_handle = NULL;

/**
 * @brief Process an event reported by the radio driver
 *
 * This callback executes in the IRQ task because that task calls
 * nrf24l01_temperature_irq_handler(). A REQUEST wakes
 * temperature_sender_task the same way a button press does, so both
 * paths share the same read-and-send code - see the "what this board
 * does" explanation at the top of this file.
 */
static void temperature_sender_callback(
    uint8_t type,
    uint8_t pipe,
    uint8_t *buf,
    uint8_t len)
{
    switch (type)
    {
    /* "RX_DR" means "receive data ready" - a packet has arrived. This is
     * the only event type this board reacts to beyond just logging it. */
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

        /*
         * The only thing this board should ever RECEIVE is a REQUEST from
         * the base station (asking us to send a reading) - a READING
         * arriving here would be unexpected, since READINGs only ever
         * travel the other direction (sensor -> base station). If we see
         * a REQUEST, wake temperature_sender_task, exactly as if the
         * physical button had just been pressed - see the "what this
         * board does" explanation at the top of this file for why that is
         * enough.
         */
        if (payload.opcode == NRF24L01_TEMPERATURE_OPCODE_REQUEST)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: request received.\n");

            if (temperature_sender_task_handle != NULL)
            {
                xTaskNotifyGive(temperature_sender_task_handle);
            }
        }
        else
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: unexpected opcode %u.\n",
                (unsigned int)payload.opcode);
        }

        break;
    }

    /* "TX_DS" means "transmit data sent" - confirmation that a READING we
     * sent was successfully delivered and acknowledged by the base
     * station. */
    case NRF24L01_INTERRUPT_TX_DS:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: irq send ok.\n");

        break;
    }

    /* "MAX_RT" means "maximum retries reached" - we tried to send, and the
     * base station never acknowledged it, so the chip gave up retrying on
     * its own (see the "Enhanced ShockBurst" explanation in
     * ../../common/nrf24l01/driver_nrf24l01_temperature.h). */
    case NRF24L01_INTERRUPT_MAX_RT:
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 sender: irq reach max retry times.\n");

        break;
    }

    /* "TX_FULL" means the chip's outgoing-packet queue is full - should
     * not normally happen here since only one packet is ever in flight at
     * a time. */
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
 * @brief Wake the IRQ task or the sender task, depending on the pin
 *
 * IMPORTANT: this function runs in "hardware-interrupt context", not as a
 * normal FreeRTOS task - it interrupts whatever the chip happened to be
 * doing, runs very briefly, then lets the interrupted code continue. Code
 * running here must be fast and must NOT do slow things like talking to
 * the radio over SPI - that is why this function does almost nothing
 * except figure out WHICH task to wake up, and then wake it; the real work
 * happens afterwards, back in normal task context.
 *
 * Radio-driver calls and SPI communication are deferred to
 * nrf24l01_irq_task(). Both GPIO sources share this one callback because
 * the SDK only allows a single GPIO interrupt callback to be registered at
 * a time - see the explanation at the top of this file.
 */
static void gpio_irq_callback(
    uint gpio,
    uint32_t events)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    TaskHandle_t task_to_notify = NULL;
    uint32_t expected_edge;

    /*
     * Work out which of the two possible sources triggered this
     * interrupt, and which task cares about it. The radio's IRQ pin is
     * active-low, so we watch for a FALLING edge (see
     * ../../common/nrf24l01/driver_nrf24l01_temperature.h); the button is
     * wired the opposite way (idle low, pressed = high), so we watch for
     * a RISING edge instead.
     */
    if (gpio == NRF24L01_TEMPERATURE_IRQ_PIN)
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

    /* The "FromISR" suffix marks this as the special, interrupt-safe
     * version of "wake up a task" - regular FreeRTOS functions are not
     * safe to call from inside a hardware interrupt, so FreeRTOS provides
     * matching *_FromISR versions for the handful of functions that
     * are. */
    vTaskNotifyGiveFromISR(
        task_to_notify,
        &higher_priority_task_woken);

    /* If the task we just woke should now run immediately (because it
     * outranks whatever was interrupted), switch to it right away instead
     * of waiting for the next scheduled check. */
    portYIELD_FROM_ISR(
        higher_priority_task_woken);
    return;
}

/**
 * @brief Process nRF24L01 events in FreeRTOS task context
 *
 * nrf24l01_temperature_irq_handler() reads and clears the radio's
 * interrupt status. The core driver then invokes
 * temperature_sender_callback(). This task's entire job is: sleep until
 * woken, process whatever the radio has to report, go back to sleep,
 * forever - the actual "what happened, what do we do about it" logic
 * lives inside nrf24l01_temperature_irq_pin_drain() (in the shared radio
 * driver module) and the callback function above that it calls into.
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

        /* Process every event currently pending, looping until the
         * radio's IRQ pin genuinely has nothing left to report - see this
         * function's comment in the shared radio driver module for why a
         * loop is needed here rather than handling just one event. */
        nrf24l01_temperature_irq_pin_drain();
    }
}

/**
 * @brief Create the IRQ task and enable the GP21 interrupt
 *
 * @return 0 on success, 1 on failure
 */
static uint8_t nrf24l01_irq_setup(void)
{
    /*
     * Create the IRQ task before enabling the hardware interrupt.
     * The GPIO callback will therefore always have a valid task
     * handle. Its handle is needed here (not self-registered) because
     * the pending-interrupt check below runs on THIS, DIFFERENT, task
     * (temperature_sender_task, which is what calls this function)
     * immediately after creation - unlike most other tasks in this
     * project, nrf24l01_irq_task cannot simply record its own handle via
     * xTaskGetCurrentTaskHandle() as its first action, because something
     * else needs that handle before the new task has necessarily had a
     * chance to run even once. Passing &gs_nrf24l01_irq_task_handle into
     * task_utils_create() gets us the handle back immediately, the moment
     * the task is created, regardless of whether it has started running
     * yet.
     */
    if (task_utils_create(
            nrf24l01_irq_task,
            "nrf24_irq",
            NRF24L01_IRQ_TASK_STACK_DEPTH,
            NULL,
            NRF24L01_IRQ_TASK_PRIORITY,
            &gs_nrf24l01_irq_task_handle) != 0)
    {
        gs_nrf24l01_irq_task_handle = NULL;

        return 1;
    }

    nrf24l01_temperature_irq_pin_init();

    /*
     * This is the ONE call in this whole program that registers a GPIO
     * interrupt callback - see the "shared GPIO callback" explanation at
     * the top of this file for why sender_button_setup() below does NOT
     * make its own separate call like this one.
     */
    gpio_set_irq_enabled_with_callback(
        NRF24L01_TEMPERATURE_IRQ_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        gpio_irq_callback);

    /*
     * Edge case: what if the radio chip already had something to report
     * (pin already LOW) in the brief moment BEFORE the line above turned
     * on edge-detection? A "falling edge" can only be detected while it is
     * actively being watched - if the pin was already low before that,
     * there will be no NEW falling edge to catch it, and we would miss the
     * event entirely and get stuck waiting forever. This check catches
     * that by manually checking the pin's current state right after
     * turning on detection, and waking the task ourselves if it is already
     * asserted.
     */
    if (nrf24l01_temperature_irq_pin_is_asserted())
    {
        xTaskNotifyGive(
            gs_nrf24l01_irq_task_handle);
    }

    return 0;
}

/**
 * @brief Configure the physical push-button's GPIO pin
 *
 * Unlike nrf24l01_irq_setup() above, this does NOT call
 * gpio_set_irq_enabled_with_callback() - only ONE call to that function is
 * allowed to "win" and actually register the callback function (see the
 * "shared GPIO callback" explanation at the top of this file), and
 * nrf24l01_irq_setup() already made that call. This function only needs
 * to turn ON interrupt detection for the button's OWN pin, using the
 * plain gpio_set_irq_enabled() (no "_with_callback"), which adds this pin
 * to the set the ALREADY-registered callback (gpio_irq_callback above)
 * will be told about.
 */
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
 *
 * This is the task described in the "what this board does" explanation at
 * the top of this file: it sets everything up once, then loops forever,
 * waiting to be woken (by either the button or an incoming REQUEST) and
 * responding by reading the sensor and sending a READING.
 */
static void temperature_sender_task(void *parameter)
{
    nrf24l01_temperature_payload_t payload;

    (void)parameter;

    /*
     * Record this task's own handle, so temperature_sender_callback()
     * (running later, on the SEPARATE IRQ task, when a REQUEST arrives)
     * and gpio_irq_callback() (running later, from hardware-interrupt
     * context, when the button is pressed) both know which task to wake
     * up. Unlike nrf24l01_irq_task above, this task CAN safely
     * self-register its own handle here as its very first action, because
     * nothing else needs this particular handle until well after this
     * line has already run (both of those later triggers only become
     * possible once setup further down in this same function - the radio
     * and the button - has completed).
     */
    temperature_sender_task_handle =
        xTaskGetCurrentTaskHandle();

    /* Bring up the DS18B20 temperature sensor - see
     * ../ds18b20/driver_ds18b20_temperature.c for what this actually
     * does. */
    if (ds18b20_temperature_init() != 0U)
    {
        nrf24l01_interface_debug_print(
            "temperature sender: sensor initialization failed.\n");

        vTaskDelete(NULL);
        return;
    }
    /*
     * The radio listens by default so it can receive a REQUEST from
     * the base station at any time; sending (button press or REQUEST
     * reply) briefly switches to TX and back.
     *
     * temperature_sender_callback() reports TX_DS, MAX_RT, RX_DR,
     * and TX_FULL events, decoding RX_DR as a temperature payload.
     */
    if (nrf24l01_temperature_init(
            NRF24L01_TEMPERATURE_TYPE_RX,
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
     * nrf24l01_temperature_send() call - see the big comment at the top
     * of this file for why a send would otherwise hang forever without
     * nrf24l01_irq_task running and able to process its completion.
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
        /*
         * Woken either by the local button or by a REQUEST decoded in
         * temperature_sender_callback() - both want the same action, so
         * this loop does not even need to know or care which one actually
         * happened.
         */
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

        payload.opcode = NRF24L01_TEMPERATURE_OPCODE_READING;

        /*
         * The radio defaults to listening (RX); we have to explicitly
         * flip it to TX (sending) mode before we can transmit anything -
         * see nrf24l01_temperature_set_mode() in the shared radio driver
         * module.
         */
        if (nrf24l01_temperature_set_mode(
                NRF24L01_TEMPERATURE_TYPE_TX) != 0)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: switch to TX failed.\n");

            continue;
        }

        /*
         * Actually transmit the READING. This call BLOCKS until the radio
         * chip confirms the packet was sent (or gives up trying) - see
         * the important explanation of why that is safe here (thanks to
         * nrf24l01_irq_task running separately) in the big comment at the
         * top of this file.
         */
        if (nrf24l01_temperature_send(&payload) == 0)
        {
            /* Turn the raw number back into readable text ("23.45") just
             * for this debug log line - see
             * ../../common/temperature_format.c. */
            char formatted[16];

            (void)temperature_format_centi_c(
                payload.temperature_centi_c,
                formatted,
                sizeof(formatted));

            nrf24l01_interface_debug_print(
                "nrf24l01 sender: sent %s C.\n",
                formatted);
        }
        else
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: send failed.\n");
        }

        /*
         * Always try to resume listening, even after a failed send,
         * so a single lost packet doesn't strand the sensor deaf to
         * future requests. If we left the radio sitting in TX mode after
         * a failure, this board would never hear another REQUEST until it
         * was reset.
         */
        if (nrf24l01_temperature_set_mode(
                NRF24L01_TEMPERATURE_TYPE_RX) != 0)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 sender: switch to RX failed.\n");
        }
    }
}

/**
 * @brief Create the sender application task
 */
uint8_t temperature_sender_start(void)
{
    /* See task_utils_create() in ../../common/task_utils.c for what this
     * wraps. NULL for the last argument, since temperature_sender_task
     * self-registers its own handle (see the comment inside that function
     * for why that is safe here). */
    return task_utils_create(
        temperature_sender_task,
        "temperature_tx",
        TEMPERATURE_SENDER_TASK_STACK_DEPTH,
        NULL,
        TEMPERATURE_SENDER_TASK_PRIORITY,
        NULL);
}
