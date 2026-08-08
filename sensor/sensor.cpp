#include "temperature_sender.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"
#include "hooks.h"
int main(void)
{
    /*
     * Initialize the configured Pico SDK console backend,
     * such as USB CDC or UART.
     */
    stdio_init_all();

    /*
     * Create the sender task before starting the scheduler.
     */
    if (temperature_sender_start() != 0)
    {
        return 1;
    }

    /*
     * FreeRTOS now takes control. The sender task will initialize
     * the radio, create the IRQ task, and start transmitting.
     */
    vTaskStartScheduler();

    /*
     * The scheduler only returns if it cannot start.
     */
    return 1;
}