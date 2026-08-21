/*
 * ============================================================================
 * WI-FI STATION - IMPLEMENTATION
 * ============================================================================
 *
 * This file does three things, in order: turn on the Wi-Fi chip, tell it to
 * behave as a normal network client ("station mode"), and then try to
 * connect to the configured network. All of the actual Wi-Fi protocol
 * detail (scanning, authentication, etc.) is handled inside the Pico SDK's
 * "cyw43_arch" library - this file just calls that library with the right
 * settings and turns its results into simple debug print messages and a
 * 0/1 success code, the same convention used everywhere else in this
 * codebase.
 */

#include "wifi_station.h"

#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#include <stdio.h>

/*
 * How long to keep trying to connect before giving up. 30 seconds is
 * generous enough to cover a slow router or a Wi-Fi network that takes a
 * moment to hand out an address, without hanging forever if the password
 * is wrong or the network is out of range.
 */
#define WIFI_CONNECTION_TIMEOUT_MS 30000U

uint8_t wifi_station_connect(void)
{
    int result;
    struct netif *wifi_interface;

    /*
     * cyw43_arch_init() powers on and initializes the CYW43 Wi-Fi/
     * Bluetooth chip. Nothing Wi-Fi related can happen before this
     * succeeds.
     */
    result = cyw43_arch_init();

    if (result != 0)
    {
        printf(
            "network: CYW43 initialization failed: %d\n",
            result);

        return 1;
    }

    /*
     * "STA" is short for "station" - this tells the chip to behave like a
     * normal Wi-Fi client joining someone else's network, rather than
     * creating its own network for other devices to join.
     */
    cyw43_arch_enable_sta_mode();

    printf("network: connecting to \"%s\"...\n", WIFI_SSID);

    /*
     * WIFI_SSID and WIFI_PASSWORD are not written anywhere in this source
     * file - the build system (CMakeLists.txt) defines them as text
     * substituted in at compile time, from values you pass to cmake. This
     * keeps Wi-Fi credentials out of the source code itself. This call
     * blocks (pauses this function) until either the connection succeeds,
     * fails, or WIFI_CONNECTION_TIMEOUT_MS milliseconds pass, whichever
     * happens first. CYW43_AUTH_WPA2_AES_PSK just says which security
     * scheme to use when authenticating - WPA2, the common standard for
     * home and small-office Wi-Fi.
     */
    result = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        WIFI_CONNECTION_TIMEOUT_MS);

    if (result != 0)
    {
        printf("network: connection failed: %d\n", result);

        /* Undo cyw43_arch_init() above, so the chip is left in a clean
         * state rather than half set up if the caller wants to try again
         * later. */
        cyw43_arch_deinit();
        return 1;
    }

    /*
     * Now that we are connected, ask lwIP (the networking software this
     * project uses, described more in base_station_network.c) for our
     * "network interface" - the internal structure that holds our current
     * IP address, among other things - purely so we can print it out for
     * whoever is watching the debug log to see, for example, on a serial
     * terminal.
     */
    wifi_interface = &cyw43_state.netif[CYW43_ITF_STA];

    printf(
        "network: connected, IP address: %s\n",
        ip4addr_ntoa(netif_ip4_addr(wifi_interface)));

    return 0;
}
