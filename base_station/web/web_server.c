/*
 * ============================================================================
 * WEB SERVER - IMPLEMENTATION
 * ============================================================================
 *
 * WHAT IS "CGI" HERE?
 * CGI (Common Gateway Interface) is an old, simple standard for making a
 * web server run a bit of code when a particular URL is visited, instead
 * of just returning a fixed file. lwIP's tiny httpd server supports a very
 * small version of this idea: you give it a table mapping a URL (like
 * "/request-temperature") to a C function, and any time a browser asks for
 * that exact URL, your function runs. Your function then returns the path
 * of the file that should actually be sent back to the browser as the
 * response. This is exactly how the "Request temperature" button on the
 * web page works: clicking it makes the browser ask for
 * "/request-temperature", which runs web_server_request_cgi_handler()
 * below.
 *
 * (The other kind of dynamic content this project uses, "SSI", is a
 * different mechanism - filling in a placeholder INSIDE an existing file -
 * explained in temperature_ssi.c.)
 */

#include "web_server.h"

#include "temperature_receiver.h"
#include "temperature_ssi.h"

#include "lwip/apps/httpd.h"
#include "lwip/def.h"

/**
 * @brief Trigger an on-demand reading and acknowledge immediately
 *
 * This function runs whenever a browser visits "/request-temperature" -
 * which is exactly what the JavaScript on the web page does when the
 * "Request temperature" button is clicked (see
 * network/http/index.shtml). All it does is call
 * temperature_receiver_request(), which wakes up the background task that
 * talks to the radio (see ../temperature/temperature_receiver.c), and then
 * returns straight away.
 *
 * This function deliberately does NOT wait to see whether the sensor
 * actually replies - that radio round trip can take up to roughly a
 * second, and this function runs as part of the web server itself, which
 * needs to stay responsive for other requests too. Whatever reply does
 * eventually arrive shows up automatically in the temperature history the
 * next time the web page polls for it (about once a second), with no
 * further help needed from this function.
 *
 * @param index      Which entry in gs_cgi_handlers below matched (unused,
 *                    since we only ever register one).
 * @param num_params  How many "?name=value" query parameters were in the
 *                    URL (unused - this endpoint does not need any).
 * @param params      The parameter names (unused).
 * @param values      The parameter values (unused).
 *
 * @return The path of a real file to send back as the HTTP response. We
 *         point this at a tiny, fixed "{"ok":true}" file - its contents do
 *         not actually matter, since the web page's JavaScript ignores
 *         them; it only needed SOMETHING valid to send back.
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

/*
 * The table lwIP's CGI mechanism uses to decide which URL runs which
 * function. Each entry pairs an exact URL path with the function to call
 * when a browser asks for it. We only need one entry.
 */
static const tCGI gs_cgi_handlers[] =
{
    {"/request-temperature", web_server_request_cgi_handler}
};

void web_server_start(void)
{
    /* Turn on the "fill in {{temperature history}}" behavior used by
     * network/http/temphist.json - see temperature_ssi.c for the details. */
    temperature_ssi_register();

    /*
     * Register the CGI table above with lwIP, so it knows to run
     * web_server_request_cgi_handler() when "/request-temperature" is
     * visited. LWIP_ARRAYSIZE is a small helper macro that works out how
     * many entries are in the gs_cgi_handlers array automatically, so we
     * do not have to keep a separate count in sync by hand.
     */
    http_set_cgi_handlers(
        gs_cgi_handlers,
        LWIP_ARRAYSIZE(gs_cgi_handlers));

    /* Finally, actually start the web server so it begins accepting
     * connections from browsers. */
    httpd_init();
}
