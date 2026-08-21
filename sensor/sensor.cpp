/*
 * ============================================================================
 * SENSOR - PROGRAM ENTRY POINT
 * ============================================================================
 *
 * This is the very first code that runs on the sensor board (a Raspberry
 * Pi Pico 2). Its only job is to start up the rest of the program's
 * pieces in the right order, then hand control to FreeRTOS - the small
 * "real-time operating system" that lets several independent tasks run on
 * one chip by rapidly switching between them, each running its own
 * infinite-loop function as if it had the CPU to itself.
 *
 * This board's job is straightforward: read the DS18B20 temperature
 * sensor, and send readings over the nRF24L01+ radio - either when the
 * physical button is pressed, or when asked to over the radio. All of
 * that lives in ONE task, created by temperature_sender_start() below.
 */

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
     *
     * This turns on the USB/serial console, so debug messages printed
     * elsewhere (see, for example, temperature_sender.c) actually reach
     * somewhere a person can see them, such as a serial terminal connected
     * to this board's USB port.
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
     *
     * This function does not return under normal circumstances - from
     * this point on, FreeRTOS is in charge, switching between whichever
     * tasks exist (here: the sender task, and the radio IRQ task it
     * creates - see temperature_sender.c) forever.
     */
    vTaskStartScheduler();

    /*
     * The scheduler only returns if it cannot start.
     *
     * This would only happen if the microcontroller ran out of memory
     * before FreeRTOS could even get going - there is nothing useful left
     * to do at that point, so this simply reports failure by returning 1.
     */
    return 1;
}
