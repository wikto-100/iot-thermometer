/*
 * ============================================================================
 * BASE STATION - PROGRAM ENTRY POINT
 * ============================================================================
 *
 * This is the very first C code that runs on the base station board (a
 * Raspberry Pi Pico 2 W). Think of this file as the "table of contents" for
 * the whole program: it does not do any real work itself, it just starts up
 * the pieces that DO the real work, in the right order, and then hands
 * control over to FreeRTOS.
 *
 * FreeRTOS is a "real-time operating system" (RTOS). Normally a tiny
 * microcontroller like this one can only run one piece of code at a time,
 * one instruction after another. FreeRTOS is a small program that sits
 * underneath everything else and rapidly switches between several
 * independent "tasks" (small programs that each think they have the CPU to
 * themselves), so it FEELS like several things are happening at once. Each
 * task is just a normal C function with an infinite loop inside it.
 *
 * The base station needs to do several things "at the same time":
 *   - Listen for radio packets from the sensor, at any moment.
 *   - Serve a web page to anyone who opens a browser and asks for it.
 *   - Keep a history of temperature readings.
 * FreeRTOS is what makes it possible for one small chip to do all of that
 * without the pieces getting in each other's way.
 *
 * The overall startup order below matters:
 *   1. Set up the storage for temperature readings FIRST, because the next
 *      steps create tasks that will immediately try to read or write to it.
 *   2. Start listening for the sensor's radio packets.
 *   3. Start Wi-Fi and the web server.
 *   4. Only once everything is ready, tell FreeRTOS to start running tasks.
 */

#include "base_station_network.h"
#include "temperature_receiver.h"
#include "temperature_store.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

int main(void)
{
    /*
     * stdio_init_all() turns on the USB/serial console, so that calls to
     * printf() elsewhere in the program (used for debug messages like
     * "temperature receiver: 21.30 C.") actually go somewhere a person can
     * see them - for example, in a serial terminal connected to the Pico's
     * USB port.
     */
    stdio_init_all();

    /*
     * The "temperature store" is a small piece of shared memory that holds
     * the last 48 temperature readings, along with when each one arrived.
     * Multiple tasks touch this later (one writes new readings into it, the
     * web server reads out of it), so it must exist and be ready BEFORE any
     * of those tasks are created. See temperature_store.c for the details
     * of how it is protected against two tasks touching it at the exact
     * same moment.
     */
    if (temperature_store_init() != 0)
    {
        return 1;
    }

    /*
     * This creates the background tasks that talk to the nRF24L01 radio
     * chip: one task listens for incoming packets from the sensor board,
     * and a second task can send a "please send me a reading" request to
     * the sensor on demand (used by the "Request temperature" button on
     * the web page). See temperature_receiver.c for the full explanation.
     */
    if (temperature_receiver_start() != 0)
    {
        return 1;
    }

    /*
     * This creates the background task that connects to Wi-Fi and then
     * starts the web server, so a phone or laptop on the same network can
     * open a web page and see the temperature history. See
     * base_station_network.c for the details.
     */
    if (base_station_network_start() != 0)
    {
        return 1;
    }

    /*
     * Everything needed by the tasks above has been created. Now hand
     * control over to FreeRTOS so it can actually start running those
     * tasks. This function does not return under normal circumstances -
     * from this point on, FreeRTOS is "in charge", switching between tasks
     * forever.
     */
    vTaskStartScheduler();

    /*
     * We only ever reach this point if vTaskStartScheduler() itself failed
     * (for example, if the microcontroller ran out of memory before it
     * could even start). There is nothing useful left to do, so just sit
     * here forever instead of falling off the end of main() and running
     * into undefined/garbage memory. tight_loop_contents() is a Pico SDK
     * helper that tells the compiler "yes, this empty loop is intentional".
     */
    for (;;)
    {
        tight_loop_contents();
    }
}
