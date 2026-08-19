#include "base_station_network.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include "lwip/apps/httpd.h"

#include "temperature_receiver.h"

#include <stdio.h>

#define BASE_STATION_NETWORK_TASK_STACK_DEPTH 1024U
#define BASE_STATION_NETWORK_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 1U)

#define WIFI_CONNECTION_TIMEOUT_MS 30000U

/*
 * The array position becomes the index passed to the handler.
 *
 * "temperature" is at position 0, so i_index will be 0.
 */
static const char *gs_ssi_tags[] =
{
    "temperature"
};

static u16_t base_station_ssi_handler(
    int tag_index,
    char *insert_buffer,
    int insert_buffer_length
#if LWIP_HTTPD_SSI_MULTIPART
    ,
    uint16_t current_tag_part,
    uint16_t *next_tag_part
#endif
)
{
    int printed;

#if LWIP_HTTPD_SSI_MULTIPART
    (void)current_tag_part;
    (void)next_tag_part;
#endif

    if (insert_buffer == NULL || insert_buffer_length <= 0)
    {
        return 0;
    }

    switch (tag_index)
    {
        case 0:
        {
            int16_t temperature;

            if (temperature_receiver_get_latest(
                    &temperature) != 0)
            {
                printed = snprintf(
                    insert_buffer,
                    (size_t)insert_buffer_length,
                    "--.--"
                );
            }
            else
            {
                int32_t signed_temperature = temperature;
                uint32_t magnitude;

                if (signed_temperature < 0)
                {
                    magnitude =
                        (uint32_t)(-signed_temperature);

                    printed = snprintf(
                        insert_buffer,
                        (size_t)insert_buffer_length,
                        "-%lu.%02lu",
                        (unsigned long)(magnitude / 100U),
                        (unsigned long)(magnitude % 100U)
                    );
                }
                else
                {
                    magnitude =
                        (uint32_t)signed_temperature;

                    printed = snprintf(
                        insert_buffer,
                        (size_t)insert_buffer_length,
                        "%lu.%02lu",
                        (unsigned long)(magnitude / 100U),
                        (unsigned long)(magnitude % 100U)
                    );
                }
            }

            break;
        }

        default:
        {
            printed = 0;
            break;
        }
    }

    if (printed < 0)
    {
        return 0;
    }

    if (printed >= insert_buffer_length)
    {
        return (u16_t)(insert_buffer_length - 1);
    }

    return (u16_t)printed;
}

static void base_station_network_task(void *parameter)
{
    int result;
    struct netif *wifi_interface;

    (void)parameter;
    result = cyw43_arch_init();

    if (result != 0)
    {
        printf(
            "network: CYW43 initialization failed: %d\n",
            result);

        vTaskDelete(NULL);
        return;
    }
    cyw43_arch_enable_sta_mode();

    printf("network: connecting to \"%s\"...\n", WIFI_SSID);

    result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        WIFI_CONNECTION_TIMEOUT_MS);

    if (result != 0)
    {
        printf(
            "network: connection failed: %d\n",
            result);

        cyw43_arch_deinit();

        vTaskDelete(NULL);
        return;
    }
    wifi_interface =
        &cyw43_state.netif[CYW43_ITF_STA];

    printf(
        "network: connected, IP address: %s\n",
        ip4addr_ntoa(
            netif_ip4_addr(wifi_interface)));


    httpd_init();
    http_set_ssi_handler(
        base_station_ssi_handler,
        gs_ssi_tags,
        LWIP_ARRAYSIZE(gs_ssi_tags));
    printf("network: HTTP server started.\n");
    /*
     * Keep the task alive. The next step will run the web server
     * from here or from a separate HTTP task.
     */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

uint8_t base_station_network_start(void)
{
    BaseType_t result;

    result = xTaskCreate(base_station_network_task,
                         "network",
                         BASE_STATION_NETWORK_TASK_STACK_DEPTH,
                         NULL,
                         BASE_STATION_NETWORK_TASK_PRIORITY,
                         NULL);

    if (result != pdPASS)
        return 1;

    return 0;
}