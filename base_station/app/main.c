#include "base_station_network.h"
#include "temperature_receiver.h"
#include "temperature_store.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

int main(void)
{
    stdio_init_all();

    if (temperature_store_init() != 0)
    {
        return 1;
    }

    if (temperature_receiver_start() != 0)
    {
        return 1;
    }

    if (base_station_network_start() != 0)
    {
        return 1;
    }

    vTaskStartScheduler();

    /* The scheduler returns only when it could not be started. */
    for (;;)
    {
        tight_loop_contents();
    }
}
