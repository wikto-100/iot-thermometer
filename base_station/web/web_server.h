/*
 * ============================================================================
 * WEB SERVER - PUBLIC INTERFACE
 * ============================================================================
 *
 * This module starts the actual web server that a browser talks to. It
 * uses "lwIP httpd", a small, simple HTTP server built into the networking
 * library this project already uses for Wi-Fi (lwIP = "light-weight IP").
 * It is not a general-purpose web server like the ones that run big
 * websites - it is designed to fit comfortably on a tiny microcontroller,
 * and can only serve files that were baked into the program at build time
 * (see network/http/ and the pico_set_lwip_httpd_content() line in
 * CMakeLists.txt).
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

/**
 * @brief Register HTTP handlers and start lwIP HTTPD
 *
 * Call this once, after Wi-Fi has connected (see
 * ../network/base_station_network.c). It registers the pieces of dynamic
 * behavior this project needs (the SSI template tag that fills in the
 * temperature history, and the CGI endpoint the "Request temperature"
 * button calls - see web_server.c for exactly what those two words mean),
 * and then starts the server itself so it begins accepting connections
 * from browsers.
 */
void web_server_start(void);

#endif
