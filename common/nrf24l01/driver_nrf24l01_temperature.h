/*
 * ============================================================================
 * NRF24L01+ TEMPERATURE RADIO DRIVER - PUBLIC INTERFACE
 * ============================================================================
 *
 * This file is the shared "temperature radio protocol" layer used by BOTH
 * boards in this project. It sits on top of the vendored, generic
 * nRF24L01+ driver (in ../../external/nrf24l01/), and adds everything
 * specific to THIS project: the exact radio settings both boards must
 * agree on to talk to each other, and the small REQUEST/READING packet
 * format described further down. Neither board talks to the low-level
 * vendored driver directly - they only ever call the functions declared in
 * this file.
 *
 * This is a fairly deep file, covering several radio concepts that are
 * worth understanding up front:
 *
 * CHANNEL AND DATA RATE
 * A radio "channel" is really just a specific frequency to transmit and
 * listen on - like tuning a physical radio dial. Both boards must be set
 * to the EXACT same channel to hear each other at all; see
 * NRF24L01_TEMPERATURE_CHANNEL_FREQUENCY below. "Data rate" is how fast
 * bits are sent over the air; a slower rate (see
 * NRF24L01_TEMPERATURE_DATA_RATE) generally means the receiver can pick up
 * a weaker signal correctly, at the cost of taking a little longer to send
 * the same amount of data - a good trade for this project, since our
 * packets are tiny and speed does not matter, but reliability does.
 *
 * CRC (ERROR CHECKING)
 * A CRC ("cyclic redundancy check") is a small extra number tacked onto
 * the end of every packet, calculated from the packet's actual contents.
 * The receiver recalculates that same number itself from the bytes it
 * received and compares it - if they do not match, the data was corrupted
 * in transit (radio interference, being out of range, etc.) and the
 * packet is thrown away rather than trusted. This project uses a 2-byte
 * CRC, giving good protection against corruption for a packet this small.
 *
 * "ENHANCED SHOCKBURST" AND AUTOMATIC ACKNOWLEDGEMENT
 * This is a feature built into the nRF24L01+ chip itself (Nordic
 * Semiconductor calls it "Enhanced ShockBurst"): whenever the chip
 * successfully receives a packet, it can automatically, instantly send
 * back a tiny "I got it" acknowledgement packet - entirely in hardware,
 * without either side's software needing to do anything extra. The side
 * that sent the original packet is told whether that acknowledgement came
 * back (see the TX_DS and MAX_RT events described in
 * temperature_receiver.c / temperature_sender.c on each board) - this is
 * how a "send" can know for certain whether the other side actually
 * received it, rather than just hoping for the best. If no
 * acknowledgement comes back, the chip automatically retries sending the
 * packet on its own, up to NRF24L01_TEMPERATURE_RETRANSMIT_COUNT times,
 * waiting NRF24L01_TEMPERATURE_RETRANSMIT_DELAY_US between attempts -
 * again, all in hardware, without any of our own code needing to manage
 * those retries.
 *
 * "PIPES" AND ADDRESSES
 * The nRF24L01+ can listen for packets from up to 6 different senders at
 * once, each identified by its own numbered "pipe" (0 through 5) and its
 * own radio address (a bit like a house address, but for radio packets -
 * a 5-byte number both sides must agree on). This project is much
 * simpler: there are only ever two devices talking to each other, so only
 * pipe 0 is used, with one single, shared address that both boards use for
 * both sending and receiving (see NRF24L01_TEMPERATURE_ADDRESS below).
 *
 * DYNAMIC PAYLOAD LENGTH
 * Normally, a radio pipe is configured to expect packets of one single,
 * fixed size. This project's two packet types are DIFFERENT sizes (a
 * REQUEST is 1 byte, a READING is 3 bytes - see further down), so
 * "dynamic payload length" is turned on instead, letting pipe 0 accept
 * either size packet, with the chip figuring out how long each one is
 * automatically.
 */

#ifndef DRIVER_NRF24L01_TEMPERATURE_H
#define DRIVER_NRF24L01_TEMPERATURE_H

#include "driver_nrf24l01_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Temperature-radio configuration
 */

/*
 * GPIO wired to the nRF24L01+'s active-low IRQ output.
 * Both boards wire this to the same pin.
 *
 * "Active-low" means the chip pulls this pin down to 0 volts to signal
 * "something happened", rather than up to full voltage - see the big
 * comment at the top of temperature_receiver.c (base station) or
 * temperature_sender.c (sensor) for the full explanation of how this pin
 * is used to wake up our code.
 */
#define NRF24L01_TEMPERATURE_IRQ_PIN 21U

/* CRC enabled with a two-byte CRC. See the CRC explanation above. */
#define NRF24L01_TEMPERATURE_CRCO                           \
    NRF24L01_BOOL_TRUE

#define NRF24L01_TEMPERATURE_ENABLE_CRC                     \
    NRF24L01_BOOL_TRUE

/*
 * Enhanced ShockBurst automatic acknowledgement.
 * Only pipe 0 is used.
 *
 * See the "Enhanced ShockBurst" explanation above for what automatic
 * acknowledgement actually does. Only pipe 0's auto-acknowledgement is
 * turned on, matching the fact that only pipe 0 is used at all (see below).
 */
#define NRF24L01_TEMPERATURE_PIPE_0_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_TRUE

#define NRF24L01_TEMPERATURE_PIPE_1_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_2_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_3_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_4_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_5_AUTO_ACKNOWLEDGMENT     \
    NRF24L01_BOOL_FALSE

/*
 * Receive pipes.
 * Pipe 0 receives temperature packets and automatic acknowledgements.
 *
 * See the "pipes and addresses" explanation above. Only pipe 0 is enabled
 * for receiving; pipes 1 through 5 are simply unused by this project.
 */
#define NRF24L01_TEMPERATURE_RX_PIPE_0                      \
    NRF24L01_BOOL_TRUE

#define NRF24L01_TEMPERATURE_RX_PIPE_1                      \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_RX_PIPE_2                      \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_RX_PIPE_3                      \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_RX_PIPE_4                      \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_RX_PIPE_5                      \
    NRF24L01_BOOL_FALSE

/* Five-byte radio address. See the "pipes and addresses" explanation
 * above - this just says addresses on this project's pipes are 5 bytes
 * long (the maximum the chip supports, giving the most possible distinct
 * addresses, though we only ever use one). */
#define NRF24L01_TEMPERATURE_ADDRESS_WIDTH                  \
    NRF24L01_ADDRESS_WIDTH_5_BYTES

#define NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES            5U

/*
 * Automatic retransmission.
 *
 * 750 us gives the receiver ample time to process a short packet.
 * Ten retransmissions prioritise reliable delivery during bring-up.
 *
 * See the "Enhanced ShockBurst" explanation above: these two numbers
 * control the chip's own automatic retry behavior when an acknowledgement
 * does not come back in time. "us" here means "microseconds" (millionths
 * of a second).
 */
#define NRF24L01_TEMPERATURE_RETRANSMIT_DELAY_US            750U
#define NRF24L01_TEMPERATURE_RETRANSMIT_COUNT               10U

/*
 * RF channel 76 corresponds to 2476 MHz.
 * Both radios must use the same channel.
 *
 * See the "channel and data rate" explanation above. 2476 MHz sits in the
 * 2.4 GHz band that this chip (and Wi-Fi, and Bluetooth) operates in.
 */
#define NRF24L01_TEMPERATURE_CHANNEL_FREQUENCY              76U

/*
 * Temperature packets require very little bandwidth.
 * 1 Mbps provides better receiver sensitivity than 2 Mbps.
 *
 * See the "channel and data rate" explanation above. "Mbps" means
 * "megabits per second".
 */
#define NRF24L01_TEMPERATURE_DATA_RATE                      \
    NRF24L01_DATA_RATE_1M

/*
 * The modules will initially be separated by only a few centimetres.
 *
 * Radio "output power" is literally how strong a signal the chip
 * transmits - a lower power uses less energy and causes less interference
 * with other nearby radio devices, and is perfectly sufficient when the
 * two boards are close together (as they were expected to be during
 * development/testing).
 */
#define NRF24L01_TEMPERATURE_OUTPUT_POWER                   \
    NRF24L01_OUTPUT_POWER_NEGATIVE_18_DBM

/*
 * Wire payload: a one-byte opcode, optionally followed by a temperature
 * value encoded as signed hundredths of a degree Celsius.
 *
 * REQUEST packets carry only the opcode byte.
 * READING packets carry the opcode byte followed by the two-byte value.
 *
 * Example READING value:
 *     2345  means  23.45 °C
 *     -725  means  -7.25 °C
 *
 * This is this project's own tiny "wire protocol" - the exact byte-by-byte
 * layout both boards agree to use. Every packet starts with a single
 * "opcode" byte that says what KIND of packet it is (see
 * nrf24l01_temperature_opcode_t below): a REQUEST asks the other side to
 * send a reading, and carries nothing else. A READING carries an actual
 * temperature value, as two more bytes right after the opcode. This is
 * exactly what "dynamic payload length" (explained at the top of this
 * file) is for: these two packet types are genuinely different lengths,
 * and the chip is configured to accept either one automatically.
 */
#define NRF24L01_TEMPERATURE_OPCODE_SIZE                    1U
#define NRF24L01_TEMPERATURE_VALUE_SIZE                     2U
#define NRF24L01_TEMPERATURE_REQUEST_PAYLOAD_SIZE            \
    NRF24L01_TEMPERATURE_OPCODE_SIZE
#define NRF24L01_TEMPERATURE_READING_PAYLOAD_SIZE            \
    (NRF24L01_TEMPERATURE_OPCODE_SIZE + NRF24L01_TEMPERATURE_VALUE_SIZE)

/*
 * Largest payload this protocol ever sends or receives. Pipe widths and
 * intermediate buffers are sized from this; dynamic payload length
 * (below) is what actually lets REQUEST and READING differ in size.
 */
#define NRF24L01_TEMPERATURE_PAYLOAD_SIZE                    \
    NRF24L01_TEMPERATURE_READING_PAYLOAD_SIZE

/*
 * Static payload widths.
 * Only pipe 0 is enabled.
 *
 * These "static" widths are what the chip would use if dynamic payload
 * length (below) were turned off - since it IS turned on for pipe 0, this
 * particular number ends up not actually mattering in practice, but it is
 * still set to a sensible value (the largest packet size) for
 * completeness, since the vendored driver expects some value here
 * regardless.
 */
#define NRF24L01_TEMPERATURE_PIPE_0_PAYLOAD                 \
    NRF24L01_TEMPERATURE_PAYLOAD_SIZE

#define NRF24L01_TEMPERATURE_PIPE_1_PAYLOAD                 0U
#define NRF24L01_TEMPERATURE_PIPE_2_PAYLOAD                 0U
#define NRF24L01_TEMPERATURE_PIPE_3_PAYLOAD                 0U
#define NRF24L01_TEMPERATURE_PIPE_4_PAYLOAD                 0U
#define NRF24L01_TEMPERATURE_PIPE_5_PAYLOAD                 0U

/*
 * Fixed-size payloads are used.
 *
 * (Despite the comment inherited here, "dynamic" - not fixed-size -
 * payloads are actually what pipe 0 uses: see the "dynamic payload
 * length" explanation at the top of this file, and
 * NRF24L01_TEMPERATURE_PIPE_0_DYNAMIC_PAYLOAD immediately below, which is
 * TRUE.)
 */
#define NRF24L01_TEMPERATURE_PIPE_0_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_TRUE

#define NRF24L01_TEMPERATURE_PIPE_1_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_2_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_3_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_4_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_FALSE

#define NRF24L01_TEMPERATURE_PIPE_5_DYNAMIC_PAYLOAD         \
    NRF24L01_BOOL_FALSE

/* The chip-wide switch that must also be turned on for any per-pipe
 * dynamic payload setting above to actually take effect. */
#define NRF24L01_TEMPERATURE_DYNAMIC_PAYLOAD                \
    NRF24L01_BOOL_TRUE

/*
 * ACK packets contain no application payload.
 *
 * The automatic acknowledgement packets described above (in the
 * "Enhanced ShockBurst" section) can optionally carry a small amount of
 * extra data of their own, piggybacked onto the "I got it" reply - this
 * project does not use that extra feature, so acknowledgements here are
 * always empty, just a plain "received" signal.
 */
#define NRF24L01_TEMPERATURE_PAYLOAD_WITH_ACK               \
    NRF24L01_BOOL_FALSE

/*
 * Every transmitted packet requests an acknowledgement.
 *
 * The chip supports sending a packet WITHOUT asking for an
 * acknowledgement at all (useful for pure "fire and forget" broadcasts) -
 * this project always wants to know whether a packet actually arrived, so
 * this is turned off, meaning every packet DOES request one.
 */
#define NRF24L01_TEMPERATURE_TX_PAYLOAD_WITH_NO_ACK         \
    NRF24L01_BOOL_FALSE

/*
 * Shared address used by the temperature transmitter and receiver.
 *
 * TX side:
 *     TX_ADDR    = this address
 *     RX_ADDR_P0 = this address for automatic acknowledgements
 *
 * RX side:
 *     RX_ADDR_P0 = this address
 *
 * See the "pipes and addresses" explanation above. This one 5-byte value
 * (spelling out, in ASCII, "TEMP" followed by a version byte 0x01) is used
 * by BOTH boards, in BOTH directions - there is no separate "sensor's
 * address" versus "base station's address"; whichever side is currently
 * sending uses this as its destination, and whichever side is currently
 * listening uses this as the address it listens for.
 */
#define NRF24L01_TEMPERATURE_ADDRESS                        \
    {0x54, 0x45, 0x4D, 0x50, 0x01}

/**
 * @brief Temperature-radio operating mode
 *
 * The chip can either be listening for packets (RX) or sending one (TX),
 * never both at the same instant - see nrf24l01_temperature_set_mode()
 * below for switching between them, and the big comment at the top of
 * temperature_receiver.c / temperature_sender.c on each board for why
 * that switching needs careful handling.
 */
typedef enum
{
    NRF24L01_TEMPERATURE_TYPE_TX = 0x00,
    NRF24L01_TEMPERATURE_TYPE_RX = 0x01
} nrf24l01_temperature_type_t;

/**
 * @brief Temperature payload opcode
 *
 * The first byte of every packet exchanged between the two boards - see
 * the "wire payload" explanation further up this file for the full
 * picture of what each of these means and what a full request/response
 * exchange looks like.
 */
typedef enum
{
    /* No reading attached; asks the peer to reply with a READING. */
    NRF24L01_TEMPERATURE_OPCODE_REQUEST = 0x00,

    /* Carries a temperature reading. */
    NRF24L01_TEMPERATURE_OPCODE_READING = 0x01
} nrf24l01_temperature_opcode_t;

/**
 * @brief Temperature payload transmitted over the radio
 *
 * This is the "unpacked", easy-to-work-with C version of a packet - after
 * the raw bytes that actually travel over the air have been decoded (see
 * nrf24l01_temperature_decode() below) into named fields a normal C
 * function can just read directly, instead of having to pick individual
 * bytes out of a raw buffer by hand.
 */
typedef struct
{
    nrf24l01_temperature_opcode_t opcode;

    /* Only meaningful when opcode is NRF24L01_TEMPERATURE_OPCODE_READING. */
    int16_t temperature_centi_c;
} nrf24l01_temperature_payload_t;

/**
 * @brief Driver event callback type
 *
 * The shape of the function each board provides to
 * nrf24l01_temperature_init() below, so this driver can call back into
 * that board's own code whenever something happens on the radio (a packet
 * arrived, a send finished, etc.) - see
 * base_station/temperature/temperature_receiver.c's
 * temperature_receiver_callback(), or
 * sensor/nrf24l01/temperature_sender.c's temperature_sender_callback(),
 * for the two actual implementations of this shape.
 *
 * @param type Which event this is - one of the NRF24L01_INTERRUPT_* values
 *             defined in the vendored driver's interface header (RX_DR = a
 *             packet arrived, TX_DS = a send was acknowledged, MAX_RT = a
 *             send's retries were all exhausted, TX_FULL = the outgoing
 *             queue is full).
 * @param pipe Which receive pipe the event relates to (see the "pipes and
 *             addresses" explanation above - this project only ever uses
 *             pipe 0).
 * @param buf  The raw packet bytes, when type indicates a packet arrived.
 * @param len  How many bytes are in buf.
 */
typedef void (*nrf24l01_temperature_callback_t)(
    uint8_t type,
    uint8_t pipe,
    uint8_t *buf,
    uint8_t len
);

/**
 * @brief Process an nRF24L01+ interrupt
 *
 * Reads and clears the chip's pending interrupt status, and calls back
 * into whichever function was registered via nrf24l01_temperature_init()
 * to report what happened. This does the real work behind
 * nrf24l01_temperature_irq_pin_drain() below, which is what most code
 * should actually call - see that function's comment for why.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_irq_handler(void);

/**
 * @brief Configure the active-low IRQ pin as a plain GPIO input
 *
 * Does not register an interrupt callback - the caller does that via
 * gpio_set_irq_enabled_with_callback(), since a board may need to
 * multiplex other GPIO interrupt sources through the same SDK-wide
 * callback slot (this is exactly what the sensor board does, sharing one
 * callback between the radio's IRQ pin and its own physical button pin -
 * see sensor/nrf24l01/temperature_sender.c).
 */
void nrf24l01_temperature_irq_pin_init(void);

/**
 * @brief Check whether the active-low IRQ pin is currently asserted
 *
 * Useful right after enabling falling-edge detection, to catch an
 * event that became pending immediately beforehand - see
 * temperature_receiver_irq_setup() in base_station/temperature/
 * temperature_receiver.c for a worked example, and the comment there
 * explaining exactly why this check is needed.
 *
 * @return 1 when asserted (active), 0 otherwise
 */
uint8_t nrf24l01_temperature_irq_pin_is_asserted(void);

/**
 * @brief Process every event pending on the IRQ pin
 *
 * Call this after being woken by the notification given from the
 * pin's falling-edge interrupt. Loops, processing one event via
 * nrf24l01_temperature_irq_handler() per iteration, until the
 * active-low pin returns high (the chip holds the pin low for as long as
 * it still has something pending to report, and can raise a fresh event
 * while an earlier one is still being handled - this loop makes sure
 * nothing gets missed).
 */
void nrf24l01_temperature_irq_pin_drain(void);

/**
 * @brief Initialize the temperature-radio driver
 *
 * This is the "full setup" function: it powers on the chip and applies
 * every single one of the settings described at the top of this file
 * (CRC, auto-acknowledgement, address, retransmission, channel, data
 * rate, output power, dynamic payload, and so on), then leaves the radio
 * actively running in the requested mode. Call this once, early in a
 * board's startup - after this, nrf24l01_temperature_set_mode() below is
 * the much cheaper way to switch between TX and RX as needed, without
 * redoing all of this setup work every time.
 *
 * @param[in] type TX or RX operating mode to start in
 * @param[in] callback application event callback - see
 *                     nrf24l01_temperature_callback_t above
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_init(
    nrf24l01_temperature_type_t type,
    nrf24l01_temperature_callback_t callback
);

/**
 * @brief Deinitialize the temperature-radio driver
 *
 * Powers the chip back down. Neither board currently calls this during
 * normal operation - it exists mainly as a cleanup path for when
 * nrf24l01_temperature_init() itself fails partway through.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_deinit(void);

/**
 * @brief Switch the already-initialized radio between TX and RX at runtime
 *
 * Unlike nrf24l01_temperature_init(), this only flips the PRIM_RX bit
 * (with CE held low while doing so) and leaves every other setting
 * untouched, so it is cheap enough to call for every request/response
 * round trip. ("PRIM_RX" and "CE" are names of actual hardware
 * registers/pins on the nRF24L01+ chip itself - PRIM_RX is the single bit
 * that decides "am I currently the receiver or the transmitter?", and CE
 * ["chip enable"] is a physical pin that must briefly be held low while
 * changing certain settings, including this one, per the chip's own
 * datasheet.)
 *
 * @param[in] type TX or RX operating mode
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_set_mode(
    nrf24l01_temperature_type_t type
);

/**
 * @brief Send one temperature payload
 *
 * Sends a REQUEST (opcode only) or READING (opcode plus value)
 * depending on payload->opcode. The radio must already be in TX mode
 * (see nrf24l01_temperature_set_mode()).
 *
 * IMPORTANT: this function BLOCKS (pauses the calling task) until the
 * chip either confirms the packet was successfully sent and acknowledged,
 * or gives up after exhausting its automatic retries (see
 * NRF24L01_TEMPERATURE_RETRANSMIT_COUNT above) - this can take anywhere
 * from a few milliseconds up to a few tens of milliseconds. That
 * confirmation can only be noticed by whichever task is actively
 * processing radio interrupts (see nrf24l01_temperature_irq_pin_drain()
 * above) - if that happened to be the SAME task calling this function,
 * it would never be free to notice its own send completing, and this call
 * would simply hang until it eventually times out and fails. Both boards
 * in this project are structured with a SEPARATE task always dedicated to
 * draining IRQ events specifically so this can never happen - see the big
 * comment at the top of temperature_receiver.c or temperature_sender.c
 * for the full explanation.
 *
 * @param[in] payload temperature payload
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_send(
    const nrf24l01_temperature_payload_t *payload
);

/**
 * @brief Decode a received temperature payload
 *
 * Recognizes both REQUEST and READING wire formats via the leading
 * opcode byte and validates the length that opcode implies (a REQUEST
 * must be exactly NRF24L01_TEMPERATURE_REQUEST_PAYLOAD_SIZE bytes, a
 * READING exactly NRF24L01_TEMPERATURE_READING_PAYLOAD_SIZE bytes - any
 * other length, or an unrecognized opcode byte, is treated as an error
 * rather than guessed at).
 *
 * @param[in]  buf raw payload bytes, as received from the radio
 * @param[in]  len raw payload length
 * @param[out] payload decoded temperature payload
 *
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_decode(
    const uint8_t *buf,
    uint8_t len,
    nrf24l01_temperature_payload_t *payload
);

#ifdef __cplusplus
}
#endif

#endif
