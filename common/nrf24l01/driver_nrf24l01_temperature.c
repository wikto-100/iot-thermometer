/*
 * ============================================================================
 * NRF24L01+ TEMPERATURE RADIO DRIVER - IMPLEMENTATION
 * ============================================================================
 *
 * See driver_nrf24l01_temperature.h for the "why" behind every setting this
 * file applies to the radio chip, and for the general background on how
 * the chip works. This file is the "how": it takes those settings and, one
 * small step at a time, tells the vendored low-level driver
 * (../../external/nrf24l01/) to actually write them to the chip over SPI.
 *
 * HOW THIS FILE IS ORGANIZED
 * Almost the whole file is really just ONE long sequence of setup steps,
 * done by nrf24l01_temperature_configure() near the middle of the file -
 * it has just been split into a few smaller helper functions
 * (nrf24l01_temperature_configure_pipes(),
 * _configure_payload_widths(), _configure_dynamic_payloads(),
 * _clear_interrupts()) purely to keep any one function from becoming
 * unmanageably long, since setting up a radio chip like this genuinely
 * does require touching many individual settings.
 *
 * THE "LINK" PATTERN USED IN nrf24l01_temperature_init()
 * The vendored low-level driver is written to work on ANY microcontroller,
 * not just the Raspberry Pi Pico - so it does not call Pico-specific
 * functions directly. Instead, it works through a "handle" structure that
 * gets filled in with FUNCTION POINTERS: instead of the driver's code
 * saying "call the Pico's SPI-read function", it says "call whatever
 * SPI-read function this project's own DRIVER_NRF24L01_LINK_SPI_READ() call
 * plugged in". This project happens to always plug in its
 * Raspberry-Pi-Pico-specific functions (see
 * ../../base_station/../common/nrf24l01/driver_nrf24l01_interface.c, in
 * this same folder), but the vendored driver itself never needs to know
 * that - it would work identically on a completely different chip if a
 * different project linked in different functions here instead. This
 * general technique (a piece of code depending only on FUNCTION POINTERS
 * it is handed, rather than on specific hardware/functions it calls
 * directly) is sometimes called "dependency injection".
 */

#include "driver_nrf24l01_temperature.h"

#include "driver_nrf24l01.h"

#include "pico/stdlib.h"

/* The one shared "handle" structure the vendored driver uses to track
 * this radio's state - see the "LINK pattern" explanation above. There is
 * only ever one nRF24L01+ chip per board, so this can simply be one global
 * variable rather than something callers have to create and pass around
 * themselves. */
static nrf24l01_handle_t gs_temperature_handle;

/* A working, mutable copy of NRF24L01_TEMPERATURE_ADDRESS (from the header
 * file) - the vendored driver's functions want a pointer to a byte array,
 * not a compile-time constant, so this copies the address into normal
 * memory once, here, for those functions to point at. */
static uint8_t gs_temperature_address[
    NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES
] = NRF24L01_TEMPERATURE_ADDRESS;

/*
 * Execute one configuration operation and report the precise operation
 * that failed.
 *
 * This is a small "macro" - a piece of code the C preprocessor pastes in
 * literally, wherever NRF24L01_TEMPERATURE_CHECK(...) is written - used to
 * avoid repeating the same "did this call succeed? if not, print which one
 * failed and give up" pattern by hand after every single one of the many
 * configuration calls below. The "#operation" trick turns the C code
 * itself (like "nrf24l01_set_mode(...)") into a printable text string, so
 * the debug message can say exactly which specific call failed, without
 * having to type that out separately for each one.
 */
#define NRF24L01_TEMPERATURE_CHECK(operation)                         \
    do                                                                \
    {                                                                 \
        uint8_t check_result = (operation);                           \
                                                                      \
        if (check_result != 0)                                        \
        {                                                             \
            nrf24l01_interface_debug_print(                           \
                "nrf24l01 temperature: %s failed with %u.\n",         \
                #operation,                                           \
                check_result                                          \
            );                                                        \
                                                                      \
            return 1;                                                 \
        }                                                             \
    } while (0)

/**
 * @brief Configure automatic acknowledgement and enabled RX pipes
 *
 * This just applies the NRF24L01_TEMPERATURE_PIPE_*_AUTO_ACKNOWLEDGMENT and
 * NRF24L01_TEMPERATURE_RX_PIPE_* settings from the header file, one call
 * per pipe (0 through 5) for each. Only pipe 0 is actually turned on for
 * either setting - the calls for pipes 1-5 are only here to explicitly
 * turn those OFF, since this project never uses them (see the "pipes and
 * addresses" explanation in the header file).
 */
static uint8_t nrf24l01_temperature_configure_pipes(void)
{
    /*
     * Automatic acknowledgement configuration.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_0,
            NRF24L01_TEMPERATURE_PIPE_0_AUTO_ACKNOWLEDGMENT
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_1,
            NRF24L01_TEMPERATURE_PIPE_1_AUTO_ACKNOWLEDGMENT
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_2,
            NRF24L01_TEMPERATURE_PIPE_2_AUTO_ACKNOWLEDGMENT
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_3,
            NRF24L01_TEMPERATURE_PIPE_3_AUTO_ACKNOWLEDGMENT
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_4,
            NRF24L01_TEMPERATURE_PIPE_4_AUTO_ACKNOWLEDGMENT
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_acknowledgment(
            &gs_temperature_handle,
            NRF24L01_PIPE_5,
            NRF24L01_TEMPERATURE_PIPE_5_AUTO_ACKNOWLEDGMENT
        )
    );

    /*
     * RX pipe configuration.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_0,
            NRF24L01_TEMPERATURE_RX_PIPE_0
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_1,
            NRF24L01_TEMPERATURE_RX_PIPE_1
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_2,
            NRF24L01_TEMPERATURE_RX_PIPE_2
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_3,
            NRF24L01_TEMPERATURE_RX_PIPE_3
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_4,
            NRF24L01_TEMPERATURE_RX_PIPE_4
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe(
            &gs_temperature_handle,
            NRF24L01_PIPE_5,
            NRF24L01_TEMPERATURE_RX_PIPE_5
        )
    );

    return 0;
}

/**
 * @brief Configure static payload widths
 *
 * As explained in the header file, pipe 0's "dynamic payload length"
 * setting (turned on in nrf24l01_temperature_configure_dynamic_payloads()
 * below) means this static width does not actually end up mattering for
 * pipe 0 in practice - but the vendored driver still expects a value here
 * regardless, so it is set to the largest possible packet size for
 * completeness. Pipes 1-5 are set to 0, since they are unused.
 */
static uint8_t nrf24l01_temperature_configure_payload_widths(void)
{
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_0_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_0_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_1_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_1_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_2_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_2_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_3_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_3_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_4_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_4_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_5_payload_number(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PIPE_5_PAYLOAD
        )
    );

    return 0;
}

/**
 * @brief Configure dynamic-payload support
 *
 * Turns on "dynamic payload length" for pipe 0 (see the header file's
 * explanation of what that means and why this project needs it), the
 * chip-wide switch that setting depends on, and two related
 * acknowledgement-payload options this project does not use and so leaves
 * turned off (see NRF24L01_TEMPERATURE_PAYLOAD_WITH_ACK and
 * NRF24L01_TEMPERATURE_TX_PAYLOAD_WITH_NO_ACK in the header file).
 */
static uint8_t nrf24l01_temperature_configure_dynamic_payloads(void)
{
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_0,
            NRF24L01_TEMPERATURE_PIPE_0_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_1,
            NRF24L01_TEMPERATURE_PIPE_1_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_2,
            NRF24L01_TEMPERATURE_PIPE_2_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_3,
            NRF24L01_TEMPERATURE_PIPE_3_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_4,
            NRF24L01_TEMPERATURE_PIPE_4_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_pipe_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_PIPE_5,
            NRF24L01_TEMPERATURE_PIPE_5_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_dynamic_payload(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_DYNAMIC_PAYLOAD
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_payload_with_ack(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_PAYLOAD_WITH_ACK
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_tx_payload_with_no_ack(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_TX_PAYLOAD_WITH_NO_ACK
        )
    );

    return 0;
}

/**
 * @brief Clear pending radio interrupt flags
 *
 * Right after powering on, the chip could already have leftover interrupt
 * flags set from before (for example, if power was cycled mid-operation) -
 * this clears them out, so our code starts from a clean, known state
 * rather than possibly reacting to a stale event that has nothing to do
 * with anything that has happened since startup.
 */
static uint8_t nrf24l01_temperature_clear_interrupts(void)
{
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_clear_interrupt(
            &gs_temperature_handle,
            NRF24L01_INTERRUPT_RX_DR
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_clear_interrupt(
            &gs_temperature_handle,
            NRF24L01_INTERRUPT_TX_DS
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_clear_interrupt(
            &gs_temperature_handle,
            NRF24L01_INTERRUPT_MAX_RT
        )
    );

    // NRF24L01_TEMPERATURE_CHECK(
    //     nrf24l01_clear_interrupt(
    //         &gs_temperature_handle,
    //         NRF24L01_INTERRUPT_TX_FULL
    //     )
    // );

    return 0;
}

/**
 * @brief Configure the nRF24L01+ for temperature packets
 *
 * This is the main "apply every setting" function, called once by
 * nrf24l01_temperature_init() below. It runs through the chip's settings
 * roughly in the order the chip's own datasheet recommends: power up
 * first, then CRC and interrupt behavior, then TX/RX mode, then pipes and
 * addressing, then retransmission, then the RF (radio-frequency) settings
 * like channel and data rate, then payload widths, and finally start the
 * chip actively running.
 */
static uint8_t nrf24l01_temperature_configure(
    nrf24l01_temperature_type_t type
)
{
    uint8_t retransmit_delay_register;

    /*
     * Keep CE low while changing the configuration.
     *
     * "CE" ("chip enable") is a physical pin, separate from the SPI wires
     * used to send it commands, that controls whether the chip is
     * ACTIVELY transmitting/listening right now. The chip's datasheet
     * requires CE to be held low (inactive) while changing certain
     * settings - nrf24l01_set_active(handle, FALSE) here is what pulls
     * that pin low.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE
        )
    );

    /*
     * Power up the radio.
     *
     * "CONFIG" is the name of one specific configuration register (a small
     * piece of memory) inside the chip that several unrelated on/off
     * settings are packed into, one bit each - PWR_UP (power up) is one of
     * those bits. The chip needs a brief 2 millisecond pause after being
     * powered up before it is ready to accept further commands, which is
     * exactly what the delay right below this is for.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_PWR_UP,
            NRF24L01_BOOL_TRUE
        )
    );
    nrf24l01_interface_delay_ms(2U);
    /*
     * Configure the two-byte CRC.
     *
     * See the "CRC (error checking)" explanation in the header file.
     * CRCO selects HOW MANY bytes the CRC is (here, configured for 2, via
     * NRF24L01_TEMPERATURE_CRCO); EN_CRC is the separate on/off switch
     * for whether CRC checking happens at all.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_CRCO,
            NRF24L01_TEMPERATURE_CRCO
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_EN_CRC,
            NRF24L01_TEMPERATURE_ENABLE_CRC
        )
    );

    /*
     * A false mask value permits the event to drive IRQ low.
     *
     * Each of these three "MASK" bits controls whether ONE particular kind
     * of event (MAX_RT, TX_DS, or RX_DR - see
     * nrf24l01_temperature_callback_t in the header file for what each
     * one means) is allowed to actually pull the chip's IRQ pin low when
     * it happens. Setting all three to FALSE ("not masked") means every
     * one of these events DOES trigger the IRQ pin, and therefore our
     * code's interrupt handling (see nrf24l01_temperature_irq_pin_drain()
     * further down, and the big comment at the top of
     * base_station/temperature/temperature_receiver.c or
     * sensor/nrf24l01/temperature_sender.c) - if any of these were
     * "masked" (TRUE) instead, that particular event would happen
     * silently, with our code never being told about it via the IRQ pin.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_MASK_MAX_RT,
            NRF24L01_BOOL_FALSE
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_MASK_TX_DS,
            NRF24L01_BOOL_FALSE
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_config(
            &gs_temperature_handle,
            NRF24L01_CONFIG_MASK_RX_DR,
            NRF24L01_BOOL_FALSE
        )
    );

    /*
     * Select transmitter or receiver mode.
     *
     * This sets the chip's "PRIM_RX" bit for the very first time, based on
     * whichever mode the caller asked nrf24l01_temperature_init() to start
     * in. After this initial setup, nrf24l01_temperature_set_mode() (much
     * further down this file) is the lightweight way to flip this same
     * bit again later, without redoing everything else in this function.
     */
    if (type == NRF24L01_TEMPERATURE_TYPE_TX)
    {
        NRF24L01_TEMPERATURE_CHECK(
            nrf24l01_set_mode(
                &gs_temperature_handle,
                NRF24L01_MODE_TX
            )
        );
    }
    else
    {
        NRF24L01_TEMPERATURE_CHECK(
            nrf24l01_set_mode(
                &gs_temperature_handle,
                NRF24L01_MODE_RX
            )
        );
    }

    if (nrf24l01_temperature_configure_pipes() != 0)
    {
        return 1;
    }

    /*
     * Configure the five-byte address width before writing addresses.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_address_width(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_ADDRESS_WIDTH
        )
    );

    /*
     * Convert 750 us into the SETUP_RETR register encoding.
     *
     * The chip's actual hardware register does not store the delay
     * directly in microseconds - it stores a small encoded number that
     * represents a delay in fixed steps (per the chip's own datasheet).
     * This helper function from the vendored driver does that unit
     * conversion for us, so this file can keep working in the more
     * meaningful unit (microseconds, from
     * NRF24L01_TEMPERATURE_RETRANSMIT_DELAY_US in the header file) rather
     * than the chip's raw encoding.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_auto_retransmit_delay_convert_to_register(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_RETRANSMIT_DELAY_US,
            &retransmit_delay_register
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_retransmit_delay(
            &gs_temperature_handle,
            retransmit_delay_register
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_auto_retransmit_count(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_RETRANSMIT_COUNT
        )
    );

    /*
     * RF configuration.
     *
     * "RF" means "radio frequency" - this section is the actual "tune the
     * radio" settings. Channel and data rate are explained in the header
     * file. "Continuous carrier transmit" and "force PLL lock signal" are
     * special test/diagnostic modes built into the chip (used for things
     * like verifying transmit power with a spectrum analyzer during
     * hardware bring-up) that have nothing to do with normal operation -
     * both are turned off here so the chip behaves normally.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_channel_frequency(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_CHANNEL_FREQUENCY
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_continuous_carrier_transmit(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_force_pll_lock_signal(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_data_rate(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_DATA_RATE
        )
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_output_power(
            &gs_temperature_handle,
            NRF24L01_TEMPERATURE_OUTPUT_POWER
        )
    );

    if (nrf24l01_temperature_clear_interrupts() != 0)
    {
        return 1;
    }

    if (nrf24l01_temperature_configure_payload_widths() != 0)
    {
        return 1;
    }

    if (nrf24l01_temperature_configure_dynamic_payloads() != 0)
    {
        return 1;
    }

    /*
     * Both devices use pipe 0 and the same five-byte address.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_rx_pipe_0_address(
            &gs_temperature_handle,
            gs_temperature_address,
            NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES
        )
    );

    /*
     * Begin with empty FIFOs.
     *
     * A "FIFO" ("first in, first out") is a small internal queue built
     * into the chip - one for outgoing packets waiting to be sent, one for
     * incoming packets waiting to be read. This makes sure both start
     * completely empty, so nothing left over from a previous run (if the
     * chip was already powered and configured before this function ran)
     * gets sent or processed unexpectedly.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_flush_tx(&gs_temperature_handle)
    );

    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_flush_rx(&gs_temperature_handle)
    );

    /*
     * In RX mode this places the radio into active receive mode.
     * In TX mode the send function lowers CE before loading a payload.
     *
     * This is the final step: raising CE (see the "keep CE low" comment
     * near the top of this function for what CE is) actually activates the
     * chip in whichever mode was selected above. In RX mode, that means
     * the chip starts genuinely listening right away. In TX mode, the
     * chip does not immediately transmit anything just from this (there is
     * nothing queued up to send yet) - CE will simply be lowered again,
     * then raised, by nrf24l01_temperature_send() further down, the next
     * time there is an actual packet ready to go.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_TRUE
        )
    );

    return 0;
}

/**
 * @brief Process an nRF24L01+ interrupt
 *
 * This is a thin wrapper around the vendored driver's own
 * nrf24l01_irq_handler(): it reads the chip's status register over SPI,
 * clears whichever flags were set, and calls back into whichever function
 * was registered via nrf24l01_temperature_init() (see
 * nrf24l01_temperature_callback_t in the header file) to report what
 * happened. Prefer calling nrf24l01_temperature_irq_pin_drain() below
 * instead of this directly - it wraps this function in the loop needed to
 * make sure nothing gets missed (see its own comment).
 */
uint8_t nrf24l01_temperature_irq_handler(void)
{
    if (nrf24l01_irq_handler(&gs_temperature_handle) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Configure the active-low IRQ pin as a plain GPIO input
 *
 * "Pulled up" means the pin is held at a HIGH voltage by default whenever
 * nothing else is actively driving it - matching the chip's own
 * active-low IRQ behavior (see the header file), where the chip pulls the
 * pin DOWN specifically to signal an event, and otherwise leaves it alone.
 */
void nrf24l01_temperature_irq_pin_init(void)
{
    gpio_init(NRF24L01_TEMPERATURE_IRQ_PIN);
    gpio_set_dir(NRF24L01_TEMPERATURE_IRQ_PIN, GPIO_IN);
    gpio_pull_up(NRF24L01_TEMPERATURE_IRQ_PIN);
}

/**
 * @brief Check whether the active-low IRQ pin is currently asserted
 *
 * gpio_get() reads the pin's CURRENT voltage level right now (as opposed
 * to reacting to it CHANGING, which is what an interrupt does) - since
 * this is "active-low", a value of 0 (LOW) means the chip currently has
 * something pending to report.
 */
uint8_t nrf24l01_temperature_irq_pin_is_asserted(void)
{
    return (gpio_get(NRF24L01_TEMPERATURE_IRQ_PIN) == 0) ? 1U : 0U;
}

/**
 * @brief Process every event pending on the IRQ pin
 *
 * A single hardware interrupt only tells us "something happened" once,
 * but the chip can have MULTIPLE things to report, and can even add a new
 * one while we are still busy processing the first. Looping here, and
 * re-checking the pin after every single event, makes sure we keep
 * processing until the chip genuinely has nothing left pending (pin back
 * HIGH) rather than potentially leaving a second event unprocessed until
 * some later, unrelated interrupt happens to prompt us to check again.
 */
void nrf24l01_temperature_irq_pin_drain(void)
{
    while (nrf24l01_temperature_irq_pin_is_asserted())
    {
        if (nrf24l01_temperature_irq_handler() != 0)
        {
            nrf24l01_interface_debug_print(
                "nrf24l01 temperature: IRQ handler failed.\n");

            /* Briefly pause before trying again, so a persistent
             * communication problem (rather than a one-off glitch) turns
             * into a slow, harmless retry loop instead of a tight one that
             * would otherwise burn CPU time doing nothing useful. */
            nrf24l01_interface_delay_ms(1U);
        }
    }
}

/**
 * @brief Initialize the temperature-radio driver
 */
uint8_t nrf24l01_temperature_init(
    nrf24l01_temperature_type_t type,
    nrf24l01_temperature_callback_t callback
)
{
    if (type != NRF24L01_TEMPERATURE_TYPE_TX &&
        type != NRF24L01_TEMPERATURE_TYPE_RX)
    {
        return 1;
    }

    if (callback == NULL)
    {
        return 1;
    }

    /*
     * Link our RP2350 interface functions directly to the core driver.
     *
     * This whole block is the "dependency injection" wiring described in
     * the big comment at the top of this file: each
     * DRIVER_NRF24L01_LINK_*() call plugs one specific
     * Raspberry-Pi-Pico-specific function (defined in
     * driver_nrf24l01_interface.c, in this same folder) into the vendored
     * driver's handle, as a function pointer, for things like "how do I
     * read over SPI", "how do I write a GPIO pin", "how do I wait", "how
     * do I print a debug message", and - the last one below - "which
     * function should I call when something happens" (our own
     * temperature-protocol callback, passed in as the "callback"
     * parameter to THIS function).
     */
    DRIVER_NRF24L01_LINK_INIT(
        &gs_temperature_handle,
        nrf24l01_handle_t
    );

    DRIVER_NRF24L01_LINK_SPI_INIT(
        &gs_temperature_handle,
        nrf24l01_interface_spi_init
    );

    DRIVER_NRF24L01_LINK_SPI_DEINIT(
        &gs_temperature_handle,
        nrf24l01_interface_spi_deinit
    );

    DRIVER_NRF24L01_LINK_SPI_READ(
        &gs_temperature_handle,
        nrf24l01_interface_spi_read
    );

    DRIVER_NRF24L01_LINK_SPI_WRITE(
        &gs_temperature_handle,
        nrf24l01_interface_spi_write
    );

    DRIVER_NRF24L01_LINK_GPIO_INIT(
        &gs_temperature_handle,
        nrf24l01_interface_gpio_init
    );

    DRIVER_NRF24L01_LINK_GPIO_DEINIT(
        &gs_temperature_handle,
        nrf24l01_interface_gpio_deinit
    );

    DRIVER_NRF24L01_LINK_GPIO_WRITE(
        &gs_temperature_handle,
        nrf24l01_interface_gpio_write
    );

    DRIVER_NRF24L01_LINK_DELAY_MS(
        &gs_temperature_handle,
        nrf24l01_interface_delay_ms
    );

    DRIVER_NRF24L01_LINK_DEBUG_PRINT(
        &gs_temperature_handle,
        nrf24l01_interface_debug_print
    );

    DRIVER_NRF24L01_LINK_RECEIVE_CALLBACK(
        &gs_temperature_handle,
        callback
    );

    /* With everything wired up above, this hands control to the vendored
     * driver to actually bring the chip up over SPI - the first real
     * communication with the physical chip. */
    if (nrf24l01_init(&gs_temperature_handle) != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 temperature: initialization failed.\n"
        );

        return 1;
    }

    /* Now that the chip is up and talking, apply every setting described
     * in the header file - see nrf24l01_temperature_configure() above. */
    if (nrf24l01_temperature_configure(type) != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 temperature: configuration failed.\n"
        );

        /* Configuration failed partway through - undo the successful
         * nrf24l01_init() above, so we do not leave the chip half set up
         * if the caller decides to try initializing again later. */
        (void)nrf24l01_deinit(&gs_temperature_handle);

        return 1;
    }

    return 0;
}

/**
 * @brief Deinitialize the temperature-radio driver
 *
 * Simply hands off to the vendored driver's own cleanup function, which
 * powers the chip down.
 */
uint8_t nrf24l01_temperature_deinit(void)
{
    if (nrf24l01_deinit(&gs_temperature_handle) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Switch the already-initialized radio between TX and RX at runtime
 *
 * See this function's much fuller explanation in the header file - in
 * short, this is the cheap alternative to re-running the whole
 * nrf24l01_temperature_configure() sequence above just to flip between
 * sending and listening.
 */
uint8_t nrf24l01_temperature_set_mode(
    nrf24l01_temperature_type_t type
)
{
    if (type != NRF24L01_TEMPERATURE_TYPE_TX &&
        type != NRF24L01_TEMPERATURE_TYPE_RX)
    {
        return 1;
    }

    /*
     * CE must be low while changing PRIM_RX (see the "keep CE low" comment
     * inside nrf24l01_temperature_configure() above for what CE and
     * PRIM_RX are).
     */
    if (nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE) != 0)
    {
        return 1;
    }

    if (nrf24l01_set_mode(
            &gs_temperature_handle,
            (type == NRF24L01_TEMPERATURE_TYPE_TX)
                ? NRF24L01_MODE_TX
                : NRF24L01_MODE_RX) != 0)
    {
        return 1;
    }

    /*
     * In RX mode this resumes listening immediately. In TX mode,
     * nrf24l01_temperature_send() lowers CE again before loading a
     * payload, matching the behavior at initial configuration.
     */
    if (nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_TRUE) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Send one temperature payload
 *
 * See this function's much fuller explanation - including the important
 * warning about it BLOCKING, and why that is safe - in the header file.
 */
uint8_t nrf24l01_temperature_send(
    const nrf24l01_temperature_payload_t *payload
)
{
    uint8_t buffer[NRF24L01_TEMPERATURE_PAYLOAD_SIZE];
    uint8_t length;

    if (payload == NULL)
    {
        return 1;
    }

    /* Every packet starts with the one-byte opcode - see the "wire
     * payload" explanation in the header file. */
    buffer[0] = (uint8_t)payload->opcode;

    if (payload->opcode == NRF24L01_TEMPERATURE_OPCODE_READING)
    {
        /*
         * Encode the signed temperature explicitly as little-endian.
         *
         * "Little-endian" is one of two common ways to lay out a
         * multi-byte number as individual bytes: it means the LEAST
         * significant byte (the one representing the smallest part of the
         * number) comes FIRST. We choose this explicitly, byte by byte,
         * rather than just copying the int16_t's raw memory directly,
         * because different kinds of computers can disagree on which byte
         * order they use internally - being explicit here guarantees both
         * boards agree on the wire format regardless.
         *
         * Casting to uint16_t preserves the two's-complement bit
         * pattern of negative int16_t values. ("Two's complement" is the
         * standard way computers represent negative numbers in binary -
         * casting to the unsigned type here just lets us safely pick
         * apart the individual bits with & and >> below, without
         * accidentally triggering sign-related surprises that come with
         * bit operations on signed types.)
         */
        uint16_t encoded_temperature =
            (uint16_t)payload->temperature_centi_c;

        /* The low 8 bits (the smaller half of the number) first... */
        buffer[1] = (uint8_t)(encoded_temperature & 0xFFU);
        /* ...then the high 8 bits (the larger half), shifted down into
         * the low position before being stored. */
        buffer[2] = (uint8_t)((encoded_temperature >> 8) & 0xFFU);

        length = NRF24L01_TEMPERATURE_READING_PAYLOAD_SIZE;
    }
    else
    {
        /* A REQUEST carries only the opcode byte - nothing more to fill
         * in. */
        length = NRF24L01_TEMPERATURE_REQUEST_PAYLOAD_SIZE;
    }

    /*
     * CE must be low while changing TX_ADDR and RX_ADDR_P0 (see the "keep
     * CE low" comment inside nrf24l01_temperature_configure() for what CE
     * is).
     */
    if (nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE) != 0)
    {
        return 1;
    }

    /*
     * Destination address.
     *
     * TX_ADDR is the chip register that says WHERE an outgoing packet
     * should be sent - see the "pipes and addresses" explanation in the
     * header file. Since both boards share the exact same address, this
     * is always the same value regardless of which board is currently
     * sending.
     */
    if (nrf24l01_set_tx_address(
            &gs_temperature_handle,
            gs_temperature_address,
            NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES) != 0)
    {
        return 1;
    }

    /*
     * With automatic acknowledgement enabled, RX_ADDR_P0 must equal
     * TX_ADDR so this transmitter can receive the ACK packet.
     *
     * This is a quirk of how the chip's automatic acknowledgement
     * (explained in the header file) actually works: after sending, the
     * chip briefly listens for the other side's "I got it" reply on pipe
     * 0 - so pipe 0's receive address has to match the address we just
     * sent TO, or we would not recognize the reply as being addressed to
     * us.
     */
    if (nrf24l01_set_rx_pipe_0_address(
            &gs_temperature_handle,
            gs_temperature_address,
            NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES) != 0)
    {
        return 1;
    }

    /*
     * nrf24l01_send() loads the TX FIFO, raises CE, and waits for the
     * IRQ handler to report TX_DS or MAX_RT.
     *
     * This is the actual transmission. See the important explanation of
     * why this call blocks, and how that is made safe, in this function's
     * comment in the header file.
     */
    if (nrf24l01_send(
            &gs_temperature_handle,
            buffer,
            length) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Decode a received temperature payload
 *
 * This is the reverse of nrf24l01_temperature_send() above: it turns raw
 * bytes that just arrived over the radio back into a proper
 * nrf24l01_temperature_payload_t structure with named fields, checking
 * along the way that the bytes actually look like a valid packet of one
 * of our two known kinds rather than blindly trusting them.
 */
uint8_t nrf24l01_temperature_decode(
    const uint8_t *buf,
    uint8_t len,
    nrf24l01_temperature_payload_t *payload
)
{
    if (buf == NULL || payload == NULL)
    {
        return 1;
    }

    /* Every valid packet has at least the one opcode byte. */
    if (len < NRF24L01_TEMPERATURE_OPCODE_SIZE)
    {
        return 1;
    }

    payload->opcode = (nrf24l01_temperature_opcode_t)buf[0];

    switch (payload->opcode)
    {
        case NRF24L01_TEMPERATURE_OPCODE_REQUEST:
        {
            /* A REQUEST must be EXACTLY the opcode byte and nothing
             * more - any other length means this is not really a valid
             * REQUEST packet, even though its first byte happened to
             * match. */
            if (len != NRF24L01_TEMPERATURE_REQUEST_PAYLOAD_SIZE)
            {
                return 1;
            }

            /* REQUEST carries no real temperature value - see the note on
             * nrf24l01_temperature_payload_t in the header file. */
            payload->temperature_centi_c = 0;

            return 0;
        }

        case NRF24L01_TEMPERATURE_OPCODE_READING:
        {
            uint16_t encoded_temperature;

            if (len != NRF24L01_TEMPERATURE_READING_PAYLOAD_SIZE)
            {
                return 1;
            }

            /*
             * Reconstruct the signed temperature from the
             * little-endian two-byte radio representation.
             *
             * This undoes exactly what nrf24l01_temperature_send() did
             * above: read the low byte back as the lower 8 bits, then
             * read the high byte and shift it back up into the upper 8
             * bits, and combine them (with "|", bitwise OR) into one
             * 16-bit number again.
             */
            encoded_temperature =
                (uint16_t)buf[1] |
                ((uint16_t)buf[2] << 8);

            /* Reinterpret those same bits as a SIGNED 16-bit number
             * again, undoing the uint16_t cast nrf24l01_temperature_send()
             * used to encode it - this correctly recovers negative
             * temperatures too, thanks to how two's complement (mentioned
             * in nrf24l01_temperature_send() above) works. */
            payload->temperature_centi_c =
                (int16_t)encoded_temperature;

            return 0;
        }

        /* Some other byte value entirely - not one of our two known
         * opcodes, so this is not a packet from this project's own
         * protocol at all (interference, a stray packet from something
         * else, etc.) - reject it rather than guessing. */
        default:
        {
            return 1;
        }
    }
}

#undef NRF24L01_TEMPERATURE_CHECK