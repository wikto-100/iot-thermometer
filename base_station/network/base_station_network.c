#include "base_station_network.h"

#include "web_server.h"
#include "wifi_station.h"

#include "task_utils.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define BASE_STATION_NETWORK_TASK_STACK_DEPTH 1024U
#define BASE_STATION_NETWORK_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 1U)

static void base_station_network_task(void *parameter)
{
    (void)parameter;

    if (wifi_station_connect() != 0)
    {
        printf("network: initialization failed.\n");
        vTaskDelete(NULL);
        return;
    }

    web_server_start();
    printf("network: HTTP server started.\n");

    /* lwIP and CYW43 have their own execution context in the FreeRTOS port. */
    vTaskDelete(NULL);
}

uint8_t base_station_network_start(void)
{
    return task_utils_create(
        base_station_network_task,
        "network_init",
        BASE_STATION_NETWORK_TASK_STACK_DEPTH,
        NULL,
        BASE_STATION_NETWORK_TASK_PRIORITY);
}
