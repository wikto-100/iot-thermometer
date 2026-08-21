#include "web_server.h"

#include "temperature_ssi.h"

#include "lwip/apps/httpd.h"

void web_server_start(void)
{
    temperature_ssi_register();
    httpd_init();
}
