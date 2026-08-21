/*
 * ============================================================================
 * BASE STATION NETWORK - IMPLEMENTATION
 * ============================================================================
 *
 * This file does two things in order, on a background task: connect to
 * Wi-Fi, then start the web server. Once both of those have happened, this
 * task's job is completely done and it deletes itself - see the big comment
 * near the bottom of this file for why that is safe to do, and why the web
 * server keeps running afterwards even though the task that started it no
 * longer exists.
 */

#include "base_station_network.h"

#include "web_server.h"
#include "wifi_station.h"

#include "task_utils.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

#define BASE_STATION_NETWORK_TASK_STACK_DEPTH 1024U

/*
 * The lowest priority of any task we create - Wi-Fi/web setup only needs
 * to happen once, is not time-critical the way reacting to a radio
 * interrupt is, and this task only runs briefly before deleting itself
 * anyway.
 */
#define BASE_STATION_NETWORK_TASK_PRIORITY \
    (tskIDLE_PRIORITY + 1U)

static void base_station_network_task(void *parameter)
{
    (void)parameter;

    /* See wifi_station.c for what this actually does; here we only care
     * about "did it work or not". */
    if (wifi_station_connect() != 0)
    {
        printf("network: initialization failed.\n");
        vTaskDelete(NULL);
        return;
    }

    /* See ../web/web_server.c for what this sets up (the web page and the
     * "request temperature" button endpoint). */
    web_server_start();
    printf("network: HTTP server started.\n");

    /*
     * At this point, both Wi-Fi and the web server are fully set up and
     * running - but they do NOT run as part of THIS task. Underneath, the
     * Pico SDK's Wi-Fi and web-server libraries (lwIP and CYW43) are built
     * on top of FreeRTOS in a way that gives them their own dedicated
     * background processing (their own task, essentially, managed
     * entirely by that library code) once they are started. This task's
     * only job was to run the two setup steps above, in order, once, at
     * startup - now that both are done, there is nothing left for it to
     * do, so it deletes itself with vTaskDelete(NULL) ("NULL" here means
     * "delete myself, the currently running task"). The web server keeps
     * working perfectly fine afterwards; it was never actually depending
     * on this particular task still being alive.
     */
    vTaskDelete(NULL);
}

uint8_t base_station_network_start(void)
{
    return task_utils_create(
        base_station_network_task,
        "network_init",
        BASE_STATION_NETWORK_TASK_STACK_DEPTH,
        NULL,
        BASE_STATION_NETWORK_TASK_PRIORITY,
        NULL);
}
