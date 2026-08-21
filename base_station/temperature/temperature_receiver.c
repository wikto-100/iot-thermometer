/*
 * ============================================================================
 * TEMPERATURE RECEIVER - TALKING TO THE SENSOR OVER RADIO
 * ============================================================================
 *
 * This file controls the nRF24L01+ radio chip that lets the base station
 * talk wirelessly to the separate sensor board (the one with the DS18B20
 * temperature sensor and its own nRF24L01+ chip). This is the most
 * "hardware-y" file in the base station code, so this comment block
 * explains several background ideas before getting into the code itself.
 *
 * WHAT IS THE nRF24L01+ AND HOW DO WE TALK TO IT?
 * It is a small, cheap radio chip. It does not understand C code - the Pico
 * talks to it over a wire protocol called SPI (a simple "send some bytes,
 * receive some bytes" connection), sending it commands like "send this
 * data" or "tell me what you last received". All of that low-level SPI
 * detail is handled by code we do NOT need to touch here: the vendored
 * driver in external/nrf24l01/, wrapped by our own
 * common/nrf24l01/driver_nrf24l01_temperature.c, which adds the specific
 * settings this project needs (radio channel, packet format, etc.) This
 * file only calls the small set of functions that wrapper exposes, such as
 * nrf24l01_temperature_send() and nrf24l01_temperature_decode().
 *
 * HOW DOES THE CHIP TELL US SOMETHING HAPPENED?
 * The chip has a physical output pin called IRQ ("interrupt request"). It
 * pulls this pin LOW (0 volts) whenever something happens that we should
 * know about - most importantly, "a new packet has arrived". This is called
 * an "active-low" signal, because LOW means "something is going on" rather
 * than HIGH. The Pico's own GPIO (general purpose input/output) hardware
 * can watch a pin and immediately jump into our code the instant it
 * changes from HIGH to LOW (a "falling edge") - this is an "interrupt".
 * Interrupts let us react to the radio the moment something arrives,
 * instead of having to constantly ask "did anything happen yet?" in a loop
 * (which would waste both CPU time and battery).
 *
 * WHY DO WE NEED TWO SEPARATE FREERTOS TASKS FOR THIS?
 * This is the trickiest design decision in this file, so it deserves a
 * careful explanation.
 *
 * This radio can both LISTEN for packets (RX = "receive" mode) and SEND
 * packets (TX = "transmit" mode), but not both at once - the code has to
 * explicitly switch the chip between the two modes. Sending is not
 * instant: nrf24l01_temperature_send() has to wait for the chip to
 * actually finish transmitting over the air (confirmed by another
 * interrupt, called TX_DS, "transmit data sent") before it can return, so
 * it BLOCKS the task that calls it until that happens (this can be up to a
 * few tens of milliseconds).
 *
 * Now imagine, hypothetically, that ONE single task were responsible for
 * BOTH listening for radio interrupts AND sending requests. When that task
 * wants to send a request, it calls nrf24l01_temperature_send(), which
 * blocks, waiting for the TX_DS interrupt to confirm the send worked. But
 * that confirmation can only be noticed and processed by... the very same
 * task that is currently stuck waiting! It cannot be in two places at
 * once. The task would end up waiting forever for an event that it itself
 * is supposed to notice, and every single request would time out and fail.
 * This is a classic kind of bug called a "deadlock": two things are each
 * waiting on the other, so neither can ever finish.
 *
 * The fix is to split the work across TWO separate tasks that can run
 * independently:
 *
 *   1. nrf24l01_irq_task - always listening. It wakes up the instant the
 *      radio's IRQ pin goes low, and processes whatever event caused it -
 *      including noticing when a send has finished. This task is the ONLY
 *      one that ever reacts to the IRQ pin.
 *
 *   2. temperature_request_task - normally asleep, doing nothing. It only
 *      wakes up when somebody (the web server, via
 *      temperature_receiver_request()) asks the sensor for a fresh
 *      reading. It sends the request and then waits, but because
 *      nrf24l01_irq_task is a SEPARATE task, it is free to notice and
 *      process the TX_DS interrupt while temperature_request_task is
 *      blocked - so the send actually completes instead of hanging
 *      forever.
 *
 * This exact "one task listens, one task can also send" pattern is used on
 * the sensor board too (see sensor/nrf24l01/temperature_sender.c), for the
 * same underlying reason.
 *
 * WHAT ARE "TASK NOTIFICATIONS"?
 * FreeRTOS gives every task a built-in, extremely lightweight "mailbox"
 * that can hold a single number. A task can pause itself with
 * ulTaskNotifyTake(), which means "go to sleep until somebody notifies me".
 * Any other code - including code running inside a hardware interrupt,
 * using the special "FromISR" version of the function - can wake it back
 * up by calling (v/x)TaskNotifyGive(...). This is how the radio's IRQ pin
 * wakes nrf24l01_irq_task, and it is also how
 * temperature_receiver_request() wakes temperature_request_task: it is a
 * simple, fast "tap on the shoulder" between two independent tasks, using
 * almost no memory (unlike heavier tools like queues).
 *
 * THE REQUEST/RESPONSE RADIO PROTOCOL: REQUEST AND READING
 * Every radio packet exchanged between the two boards starts with one
 * "opcode" byte that says what kind of packet it is (this is defined in
 * common/nrf24l01/driver_nrf24l01_temperature.h):
 *
 *   REQUEST - sent base station -> sensor. Carries no temperature value; it
 *             just means "please send me a fresh reading".
 *   READING - sent sensor -> base station. Carries an actual temperature
 *             value. The sensor sends one of these either because its own
 *             physical button was pressed, OR because it just received a
 *             REQUEST - from the base station's point of view, a READING
 *             always means the same thing (a fresh temperature to store)
 *             no matter which of those two reasons caused it.
 *
 * A full "on demand" round trip looks like this:
 *   1. Someone clicks "Request temperature" on the web page.
 *   2. The web server calls temperature_receiver_request(), which wakes
 *      temperature_request_task().
 *   3. temperature_request_task switches the radio to TX mode, sends a
 *      REQUEST packet, then switches straight back to RX mode so the base
 *      station can hear the sensor's reply once it arrives.
 *   4. The sensor board (running its own, separate program) receives the
 *      REQUEST, takes a fresh temperature reading, and sends back a
 *      READING packet.
 *   5. nrf24l01_irq_task notices the READING arriving (same as it would
 *      for any unprompted reading) and saves it into the shared
 *      temperature_store.
 */

#include "temperature_receiver.h"

#include "temperature_store.h"

#include "driver_nrf24l01_interface.h"
#include "driver_nrf24l01_temperature.h"

#include "task_utils.h"
#include "temperature_format.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

/*
 * "Stack depth" is how much scratch memory (for local variables, function
 * call bookkeeping, etc.) FreeRTOS sets aside for each task. Too small and
 * the task could crash by running out of room; these values were chosen
 * generously enough for the small amount of work each task does. The unit
 * here is "words" (4 bytes each on this 32-bit chip), not bytes.
 */
#define NRF24L01_IRQ_TASK_STACK_DEPTH 512U
#define TEMPERATURE_REQUEST_TASK_STACK_DEPTH 512U

/*
 * FreeRTOS "priority" decides which task gets to run first when more than
 * one task is ready to run at the same moment. Higher numbers mean higher
 * priority. We deliberately give nrf24l01_irq_task a HIGHER priority than
 * temperature_request_task, so that if a real radio event and an outgoing
 * request both become ready to handle at the same time, the radio event
 * always gets processed first. tskIDLE_PRIORITY is the lowest possible
 * priority (used by FreeRTOS's own background "idle" task); we build our
 * priorities up from there. This matches the same priority ordering used
 * on the sensor board, for consistency.
 */
#define NRF24L01_IRQ_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 3U)

#define TEMPERATURE_REQUEST_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 2U)

/*
 * "Handles" are how FreeRTOS lets one piece of code refer to a specific
 * task, so it can be woken up later (for example, from inside a hardware
 * interrupt). Each task fills in its own handle as the very first thing it
 * does when it starts running (see xTaskGetCurrentTaskHandle() below), so
 * these start out NULL and only become valid once that task has actually
 * begun executing.
 */
static TaskHandle_t gs_nrf24l01_irq_task_handle = NULL;
static TaskHandle_t gs_temperature_request_task_handle = NULL;

/**
 * @brief Print a temperature represented in hundredths of a degree Celsius
 *
 * This is just for the debug log (visible over USB/serial) - it turns a
 * value like 2345 into the human-readable text "23.45" and prints it. See
 * common/temperature_format.c for how that number-to-text conversion
 * works; it is shared with the sensor board and the web page code so this
 * formatting logic only exists in one place.
 */
static void temperature_receiver_print(int16_t temperature_centi_c)
{
    char formatted[16];

    (void)temperature_format_centi_c(
        temperature_centi_c,
        formatted,
        sizeof(formatted));

    nrf24l01_interface_debug_print(
        "temperature receiver: %s C.\n",
        formatted);
}

/**
 * @brief Process an event reported by the radio driver
 *
 * This function is called BY the radio driver code, not by us directly -
 * we hand a pointer to this function to nrf24l01_temperature_init() below,
 * and the driver calls it back whenever something happens on the radio
 * (this pattern is called a "callback"). It always runs on
 * nrf24l01_irq_task, because that is the task that calls
 * nrf24l01_temperature_irq_pin_drain(), which is what triggers this
 * callback deep inside the driver.
 *
 * A READING is published to the shared temperature_store whether it
 * arrived on its own (the sensor's button was pressed) or as the reply to
 * a REQUEST we sent - either way, it is simply the latest known
 * temperature, and the rest of the program (the web page) does not need to
 * know or care which of those two reasons caused it.
 *
 * @param type Which kind of radio event this is (see the switch below).
 * @param pipe Which "receive pipe" the data came in on. This radio chip
 *             supports listening to up to 6 different senders at once,
 *             each on its own numbered pipe; we only ever use pipe 0, so
 *             any other value would mean something unexpected happened.
 * @param buf  The raw bytes received, when type is "a packet arrived".
 * @param len  How many bytes are in buf.
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
        /*
         * "RX_DR" means "receive data ready" - a new packet has arrived
         * and is waiting for us to read it. This is the event we actually
         * care about; everything else below is just informational logging.
         */
        case NRF24L01_INTERRUPT_RX_DR:
        {
            if (pipe != 0U)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: unexpected pipe %u.\n",
                    pipe);

                return;
            }

            /*
             * Turn the raw bytes (buf/len) into a proper payload structure
             * with named fields (opcode, temperature_centi_c). This also
             * checks that the bytes are actually valid for one of our two
             * known packet types (REQUEST or READING) - see
             * common/nrf24l01/driver_nrf24l01_temperature.c for exactly
             * how that decoding works.
             */
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

            /*
             * The base station should only ever RECEIVE a READING (a
             * temperature value coming FROM the sensor). If we somehow
             * received a REQUEST instead, something is wrong - REQUEST
             * packets are only meant to travel the other direction, from
             * us to the sensor - so we ignore it rather than trying to
             * store a temperature that was never actually sent.
             */
            if (payload.opcode != NRF24L01_TEMPERATURE_OPCODE_READING)
            {
                nrf24l01_interface_debug_print(
                    "temperature receiver: unexpected opcode %u.\n",
                    (unsigned int)payload.opcode);

                return;
            }

            /* Save the new reading into the shared history (see
             * temperature_store.c) so the web page can show it. */
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

        /*
         * "TX_DS" means "transmit data sent" - confirmation that a packet
         * WE sent (a REQUEST) was successfully delivered and acknowledged
         * by the sensor. See the big comment at the top of this file for
         * why this event needs to be processed by THIS task rather than
         * the one that called nrf24l01_temperature_send().
         */
        case NRF24L01_INTERRUPT_TX_DS:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: TX_DS event.\n");

            break;
        }

        /*
         * "MAX_RT" means "maximum retries reached" - we tried to send a
         * packet, the sensor never acknowledged receiving it (perhaps it
         * is out of range, or powered off), and the radio chip gave up
         * automatically after retrying several times on its own.
         */
        case NRF24L01_INTERRUPT_MAX_RT:
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: MAX_RT event.\n");

            break;
        }

        /*
         * "TX_FULL" means the chip's outgoing-packet queue is full. We
         * only ever have one packet in flight at a time, so this should
         * not normally happen; it is here mainly so nothing is silently
         * dropped without at least a log message.
         */
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
 * IMPORTANT: this function runs in "hardware-interrupt context", not as a
 * normal FreeRTOS task. That means it interrupts whatever the chip
 * happened to be doing at that exact instant, runs very briefly, and then
 * lets the interrupted code continue. Code running here must be extremely
 * fast and must NOT do slow things like talking to the radio over SPI -
 * that is exactly why this function does almost nothing except wake up
 * nrf24l01_irq_task, which then does the real work afterwards, back in
 * normal task context where it is safe to take longer.
 */
static void temperature_receiver_gpio_irq_callback(
    uint gpio,
    uint32_t events)
{
    /*
     * FreeRTOS functions called from an interrupt need to know if waking
     * this task should cause an IMMEDIATE task switch (if the task we just
     * woke has higher priority than whatever was running before the
     * interrupt happened). This starts as "no", and
     * vTaskNotifyGiveFromISR() below will set it to "yes" if that is the
     * case.
     */
    BaseType_t higher_priority_task_woken = pdFALSE;

    /* The Pico can raise this same callback for other GPIO pins too (the
     * SDK only allows one callback function to be registered at a time);
     * make sure this event is actually about the pin the radio's IRQ line
     * is wired to before doing anything. */
    if (gpio != NRF24L01_TEMPERATURE_IRQ_PIN)
    {
        return;
    }

    /* We only asked to be told about "falling edges" (pin going from HIGH
     * to LOW) - double check that is what this event actually is. */
    if ((events & GPIO_IRQ_EDGE_FALL) == 0U)
    {
        return;
    }

    /* It is technically possible for this interrupt to fire before
     * nrf24l01_irq_task has finished starting up and recorded its own
     * handle below - guard against notifying a handle that does not exist
     * yet. */
    if (gs_nrf24l01_irq_task_handle == NULL)
    {
        return;
    }

    /*
     * The "FromISR" suffix marks this as the special, interrupt-safe
     * version of "wake up a task" - regular FreeRTOS functions are not
     * safe to call from inside a hardware interrupt, so FreeRTOS provides
     * matching *_FromISR versions for the handful of functions that are.
     */
    vTaskNotifyGiveFromISR(
        gs_nrf24l01_irq_task_handle,
        &higher_priority_task_woken);

    /*
     * If waking nrf24l01_irq_task means it should now run immediately
     * (because it outranks whatever was interrupted), this tells FreeRTOS
     * to switch to it right away instead of waiting for the next
     * scheduled check. This keeps the reaction to a radio event as fast as
     * possible.
     */
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/**
 * @brief Configure the active-low radio IRQ input
 *
 * This wires up the GPIO interrupt described above: it configures the pin,
 * tells the Pico SDK which function to call when the pin falls (our
 * temperature_receiver_gpio_irq_callback above), and then checks for one
 * tricky edge case explained in the comment further down.
 */
static void temperature_receiver_irq_setup(void)
{
    /* Basic pin setup (input, pulled up, etc.) - shared with the sensor
     * board's identical need, so it lives in the radio driver module. See
     * common/nrf24l01/driver_nrf24l01_temperature.c. */
    nrf24l01_temperature_irq_pin_init();

    /*
     * This is a Pico SDK function that says: "watch this pin, and the
     * moment it falls from HIGH to LOW, call this function". From this
     * point on, temperature_receiver_gpio_irq_callback() above will run
     * automatically whenever the radio has something to tell us.
     */
    gpio_set_irq_enabled_with_callback(
        NRF24L01_TEMPERATURE_IRQ_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        temperature_receiver_gpio_irq_callback);

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
        xTaskNotifyGive(gs_nrf24l01_irq_task_handle);
    }
}

/**
 * @brief Initialize the radio and process its interrupts
 *
 * This is the "always listening" task described in the big comment at the
 * top of this file. It owns every single SPI transaction that is driven by
 * a radio EVENT: unsolicited READINGs arriving from the sensor, and also
 * the TX_DS/MAX_RT completion of a REQUEST that temperature_request_task()
 * sent. Keeping all of that on one task is exactly what lets that other
 * task's call to nrf24l01_temperature_send() block safely without
 * deadlocking - this task is always free to notice and process the
 * resulting interrupt while the other one waits.
 */
static void nrf24l01_irq_task(void *parameter)
{
    (void)parameter;

    /*
     * Record this task's own handle, so that
     * temperature_receiver_gpio_irq_callback() (running later, from inside
     * a hardware interrupt) knows which task to wake up. This must happen
     * as the very first thing in the task, before anything else below sets
     * up the interrupt that will reference it.
     */
    gs_nrf24l01_irq_task_handle =
        xTaskGetCurrentTaskHandle();

    /*
     * Turn on the radio chip and put it into RX (listening) mode. This is
     * the base station's normal, "resting" state - it always defaults
     * back to listening. Sending a REQUEST briefly switches to TX mode and
     * immediately switches back (see temperature_request_task() below);
     * it never STAYS in TX mode.
     *
     * temperature_receiver_callback (defined above) is registered here as
     * the function the radio driver should call whenever something
     * happens.
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

    /*
     * The task's main loop, and it really is this simple: sleep until
     * woken by a radio interrupt, then process whatever caused it, then go
     * back to sleep. Repeat forever. All of the actual "what happened, and
     * what should we do about it" logic lives inside
     * nrf24l01_temperature_irq_pin_drain() (in the radio driver module)
     * and the temperature_receiver_callback() function above that it
     * calls into.
     */
    for (;;)
    {
        /*
         * pdTRUE here means "clear my notification count back to zero
         * after waking up", and portMAX_DELAY means "wait however long it
         * takes, with no timeout" - this task does nothing at all until it
         * is woken.
         */
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        /*
         * The radio chip holds its IRQ pin LOW for as long as it still has
         * something pending to report, and can even report a second event
         * while we are handling the first. This function (in the radio
         * driver module) loops, processing one event at a time, until the
         * pin goes back HIGH and there is truly nothing left to do.
         */
        nrf24l01_temperature_irq_pin_drain();
    }
}

/**
 * @brief Send a REQUEST and return to listening
 *
 * This is the second, normally-sleeping task described in the big comment
 * at the top of this file. It is woken up by temperature_receiver_request()
 * whenever someone wants the sensor to send a fresh reading right now (for
 * example, clicking the button on the web page).
 *
 * This task does NOT wait around for a reply after sending the request.
 * Any READING that comes back is picked up and saved by nrf24l01_irq_task()
 * above, exactly the same way as any other, unprompted reading - as far as
 * that code is concerned, there is no difference between "the sensor felt
 * like sending one" and "we asked for one".
 */
static void temperature_request_task(void *parameter)
{
    nrf24l01_temperature_payload_t payload;

    (void)parameter;

    /* Same idea as in nrf24l01_irq_task above: record our own handle so
     * temperature_receiver_request() (called from a totally different
     * task) knows which task to wake up. */
    gs_temperature_request_task_handle =
        xTaskGetCurrentTaskHandle();

    /*
     * The REQUEST packet is always exactly the same, so we can build it
     * once here, outside the loop, instead of rebuilding it every time we
     * send one. It only carries the opcode; the temperature field is
     * meaningless for a REQUEST (see
     * common/nrf24l01/driver_nrf24l01_temperature.h for the wire format),
     * so it is simply left as 0.
     */
    payload.opcode = NRF24L01_TEMPERATURE_OPCODE_REQUEST;
    payload.temperature_centi_c = 0;

    for (;;)
    {
        /* Sleep until temperature_receiver_request() wakes us up. */
        (void)ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);

        /*
         * The radio can only either listen or send at any one moment, and
         * it currently defaults to listening (RX). Before we can send
         * anything, we have to explicitly flip it into TX (sending) mode.
         * See nrf24l01_temperature_set_mode() in the radio driver module
         * for exactly what this does at the hardware level - it is a
         * quick operation, not a full reconfiguration.
         */
        if (nrf24l01_temperature_set_mode(
                NRF24L01_TEMPERATURE_TYPE_TX) != 0)
        {
            nrf24l01_interface_debug_print(
                "temperature receiver: request: switch to TX failed.\n");

            continue;
        }

        /*
         * Actually transmit the REQUEST packet over the air. As explained
         * at length in the big comment at the top of this file, this call
         * BLOCKS (pauses this task) until the radio chip confirms the
         * packet was sent (or gives up trying) - and it is safe to block
         * here specifically because nrf24l01_irq_task is a separate task
         * that remains free to notice and process that confirmation while
         * we wait.
         */
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
         * Always try to resume listening, even after a failed send, so
         * one lost REQUEST doesn't strand the base station unable to hear
         * the sensor's own unprompted readings (for example, if someone
         * presses the sensor's physical button right afterwards). If we
         * left the radio sitting in TX mode after a failure, we would
         * simply never hear anything again until the board was reset.
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
    /*
     * task_utils_create() is a small shared helper (see
     * common/task_utils.c) that wraps FreeRTOS's own xTaskCreate()
     * function and turns its result into a simple 0 = success / 1 =
     * failure value. The last argument here is an optional "give me the
     * new task's handle right away" output - we pass NULL for it because
     * both tasks below record their own handle themselves, as the first
     * thing they do when they start running (see
     * xTaskGetCurrentTaskHandle() inside each task function above).
     */
    if (task_utils_create(
            nrf24l01_irq_task,
            "temperature_rx",
            NRF24L01_IRQ_TASK_STACK_DEPTH,
            NULL,
            NRF24L01_IRQ_TASK_PRIORITY,
            NULL) != 0)
    {
        return 1;
    }

    if (task_utils_create(
            temperature_request_task,
            "temperature_req",
            TEMPERATURE_REQUEST_TASK_STACK_DEPTH,
            NULL,
            TEMPERATURE_REQUEST_TASK_PRIORITY,
            NULL) != 0)
    {
        return 1;
    }

    return 0;
}

uint8_t temperature_receiver_request(void)
{
    /* If temperature_request_task has not started yet (should not
     * normally happen - see the header file for why), there is nobody to
     * wake up, so report failure instead of doing nothing silently. */
    if (gs_temperature_request_task_handle == NULL)
    {
        return 1;
    }

    /*
     * This is the "tap on the shoulder" mentioned in the big comment at
     * the top of the file: it simply wakes up temperature_request_task and
     * returns immediately, without waiting to see what that task does next
     * or whether the sensor ever replies. This function can be called from
     * any task - here, it is called from the web server's task, which is
     * a completely different one than temperature_request_task itself.
     */
    xTaskNotifyGive(gs_temperature_request_task_handle);

    return 0;
}
