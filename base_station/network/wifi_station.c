#include "wifi_station.h"

#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include <stdio.h>

#define WIFI_CONNECTION_TIMEOUT_MS 30000U

uint8_t wifi_station_connect(void)
{
    int result;
    struct netif *wifi_interface;

    result = cyw43_arch_init();

    if (result != 0)
    {
        printf(
            "network: CYW43 initialization failed: %d\n",
            result);

        return 1;
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
        printf("network: connection failed: %d\n", result);

        cyw43_arch_deinit();
        return 1;
    }

    wifi_interface = &cyw43_state.netif[CYW43_ITF_STA];

    printf(
        "network: connected, IP address: %s\n",
        ip4addr_ntoa(netif_ip4_addr(wifi_interface)));

    return 0;
}
