#include "web_server.h"

#include "temperature_receiver.h"
#include "temperature_ssi.h"

#include "lwip/apps/httpd.h"
#include "lwip/def.h"

/**
 * @brief Trigger an on-demand reading and acknowledge immediately
 *
 * Does not wait for the sensor's reply; the request/response round
 * trip happens asynchronously and any reply lands in the regular
 * temperature history the next time the page polls it.
 */
static const char *web_server_request_cgi_handler(
    int index,
    int num_params,
    char *params[],
    char *values[])
{
    (void)index;
    (void)num_params;
    (void)params;
    (void)values;

    (void)temperature_receiver_request();

    return "/ack.json";
}

static const tCGI gs_cgi_handlers[] =
{
    {"/request-temperature", web_server_request_cgi_handler}
};

void web_server_start(void)
{
    temperature_ssi_register();

    http_set_cgi_handlers(
        gs_cgi_handlers,
        LWIP_ARRAYSIZE(gs_cgi_handlers));

    httpd_init();
}
