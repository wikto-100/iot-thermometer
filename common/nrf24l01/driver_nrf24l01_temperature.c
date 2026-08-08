#include "driver_nrf24l01_temperature.h"

#include "driver_nrf24l01.h"

static nrf24l01_handle_t gs_temperature_handle;

static uint8_t gs_temperature_address[
    NRF24L01_TEMPERATURE_ADDRESS_WIDTH_BYTES
] = NRF24L01_TEMPERATURE_ADDRESS;

/*
 * Execute one configuration operation and report the precise operation
 * that failed.
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
 */
static uint8_t nrf24l01_temperature_configure(
    nrf24l01_temperature_type_t type
)
{
    uint8_t retransmit_delay_register;

    /*
     * Keep CE low while changing the configuration.
     */
    NRF24L01_TEMPERATURE_CHECK(
        nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE
        )
    );

    /*
     * Power up the radio.
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

    if (nrf24l01_init(&gs_temperature_handle) != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 temperature: initialization failed.\n"
        );

        return 1;
    }

    if (nrf24l01_temperature_configure(type) != 0)
    {
        nrf24l01_interface_debug_print(
            "nrf24l01 temperature: configuration failed.\n"
        );

        (void)nrf24l01_deinit(&gs_temperature_handle);

        return 1;
    }

    return 0;
}

/**
 * @brief Deinitialize the temperature-radio driver
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
 * @brief Send one temperature payload
 */
uint8_t nrf24l01_temperature_send(
    const nrf24l01_temperature_payload_t *payload
)
{
    uint16_t encoded_temperature;
    uint8_t buffer[NRF24L01_TEMPERATURE_PAYLOAD_SIZE];

    if (payload == NULL)
    {
        return 1;
    }

    /*
     * Encode the signed temperature explicitly as little-endian.
     *
     * Casting to uint16_t preserves the two's-complement bit pattern
     * of negative int16_t values.
     */
    encoded_temperature = (uint16_t)payload->temperature_centi_c;

    buffer[0] = (uint8_t)(encoded_temperature & 0xFFU);
    buffer[1] = (uint8_t)((encoded_temperature >> 8) & 0xFFU);

    /*
     * CE must be low while changing TX_ADDR and RX_ADDR_P0.
     */
    if (nrf24l01_set_active(
            &gs_temperature_handle,
            NRF24L01_BOOL_FALSE) != 0)
    {
        return 1;
    }

    /*
     * Destination address.
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
     */
    if (nrf24l01_send(
            &gs_temperature_handle,
            buffer,
            NRF24L01_TEMPERATURE_PAYLOAD_SIZE) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Decode a received temperature payload
 */
uint8_t nrf24l01_temperature_decode(
    const uint8_t *buf,
    uint8_t len,
    nrf24l01_temperature_payload_t *payload
)
{
    uint16_t encoded_temperature;

    if (buf == NULL || payload == NULL)
    {
        return 1;
    }

    if (len != NRF24L01_TEMPERATURE_PAYLOAD_SIZE)
    {
        return 1;
    }

    /*
     * Reconstruct the signed temperature from the little-endian
     * two-byte radio representation.
     */
    encoded_temperature =
        (uint16_t)buf[0] |
        ((uint16_t)buf[1] << 8);

    payload->temperature_centi_c =
        (int16_t)encoded_temperature;

    return 0;
}

#undef NRF24L01_TEMPERATURE_CHECK