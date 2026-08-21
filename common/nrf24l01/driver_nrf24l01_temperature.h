#ifndef DRIVER_NRF24L01_TEMPERATURE_H
#define DRIVER_NRF24L01_TEMPERATURE_H

#include "driver_nrf24l01_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Temperature-radio configuration
 */

/* CRC enabled with a two-byte CRC. */
#define NRF24L01_TEMPERATURE_CRCO                           \
    NRF24L01_BOOL_TRUE

#define NRF24L01_TEMPERATURE_ENABLE_CRC                     \
    NRF24L01_BOOL_TRUE

/*
 * Enhanced ShockBurst automatic acknowledgement.
 * Only pipe 0 is used.
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

/* Five-byte radio address. */
#define NRF24L01_TEMPERATURE_ADDRESS_WIDTH                  \
    NRF24L01_ADDRESS_WIDTH_5_BYTES

#define NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES            5U

/*
 * Automatic retransmission.
 *
 * 750 us gives the receiver ample time to process a short packet.
 * Ten retransmissions prioritise reliable delivery during bring-up.
 */
#define NRF24L01_TEMPERATURE_RETRANSMIT_DELAY_US            750U
#define NRF24L01_TEMPERATURE_RETRANSMIT_COUNT               10U

/*
 * RF channel 76 corresponds to 2476 MHz.
 * Both radios must use the same channel.
 */
#define NRF24L01_TEMPERATURE_CHANNEL_FREQUENCY              76U

/*
 * Temperature packets require very little bandwidth.
 * 1 Mbps provides better receiver sensitivity than 2 Mbps.
 */
#define NRF24L01_TEMPERATURE_DATA_RATE                      \
    NRF24L01_DATA_RATE_1M

/*
 * The modules will initially be separated by only a few centimetres.
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

#define NRF24L01_TEMPERATURE_DYNAMIC_PAYLOAD                \
    NRF24L01_BOOL_TRUE

/*
 * ACK packets contain no application payload.
 */
#define NRF24L01_TEMPERATURE_PAYLOAD_WITH_ACK               \
    NRF24L01_BOOL_FALSE

/*
 * Every transmitted packet requests an acknowledgement.
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
 */
#define NRF24L01_TEMPERATURE_ADDRESS                        \
    {0x54, 0x45, 0x4D, 0x50, 0x01}

/**
 * @brief Temperature-radio operating mode
 */
typedef enum
{
    NRF24L01_TEMPERATURE_TYPE_TX = 0x00,
    NRF24L01_TEMPERATURE_TYPE_RX = 0x01
} nrf24l01_temperature_type_t;

/**
 * @brief Temperature payload opcode
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
 */
typedef struct
{
    nrf24l01_temperature_opcode_t opcode;

    /* Only meaningful when opcode is NRF24L01_TEMPERATURE_OPCODE_READING. */
    int16_t temperature_centi_c;
} nrf24l01_temperature_payload_t;

/**
 * @brief Driver event callback type
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
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_irq_handler(void);

/**
 * @brief Initialize the temperature-radio driver
 *
 * @param[in] type TX or RX operating mode
 * @param[in] callback application event callback
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
 * @return 0 on success, 1 on failure
 */
uint8_t nrf24l01_temperature_deinit(void);

/**
 * @brief Switch the already-initialized radio between TX and RX at runtime
 *
 * Unlike nrf24l01_temperature_init(), this only flips the PRIM_RX bit
 * (with CE held low while doing so) and leaves every other setting
 * untouched, so it is cheap enough to call for every request/response
 * round trip.
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
 * opcode byte and validates the length that opcode implies.
 *
 * @param[in]  buf raw payload bytes
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