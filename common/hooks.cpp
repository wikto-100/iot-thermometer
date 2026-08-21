/*
 * ============================================================================
 * FREERTOS HOOKS - IMPLEMENTATION
 * ============================================================================
 *
 * See hooks.h for what each of these functions is called for. This file is
 * written in C++ (hooks.cpp) even though almost everything else in this
 * project is plain C - it does not actually use any C++ features itself,
 * this is simply how the project happened to be set up, and it works fine
 * either way since "extern "C"" below makes these functions callable from
 * FreeRTOS's own C code without any C++-specific naming complications.
 *
 * Both of the "something has gone badly wrong" hooks below do the exact
 * same thing: turn off interrupts and then loop forever, doing nothing.
 * This might look unhelpful, but it is a deliberate, common embedded-
 * systems safety choice: if the microcontroller has run out of memory, or
 * a task has corrupted its own stack, the SAFEST thing to do is stop
 * completely rather than try to keep running in a state we can no longer
 * trust - a half-broken device could do something unpredictable (or, if it
 * is controlling something more consequential than a thermometer,
 * something dangerous). Turning off interrupts here also stops anything
 * else from continuing to run "underneath" this infinite loop. If this
 * device is connected to a serial console when this happens, this frozen
 * state is easy to notice and investigate; without it, a bug like this
 * could otherwise cause strange, hard-to-diagnose behavior far away from
 * where the actual problem happened.
 */

#include "hooks.h"
#include "pico/stdlib.h"

extern "C" void vApplicationMallocFailedHook(void)
{
    /* Stop responding to interrupts entirely - see the explanation above
     * for why. */
    taskDISABLE_INTERRUPTS();

    /* tight_loop_contents() is a Pico SDK helper that tells the compiler
     * "yes, this empty loop is intentional, do not warn about it or try to
     * optimize it away". */
    for (;;) {
        tight_loop_contents();
    }
}

extern "C" void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name
)
{
    /* We are not using these parameters (which task overflowed, and its
     * name) for anything - see the explanation above for why this simply
     * halts rather than trying to log or recover. (void)-casting an unused
     * parameter like this is a common way to tell both the compiler and a
     * human reader "this is deliberately unused, not forgotten". */
    (void)task;
    (void)task_name;

    taskDISABLE_INTERRUPTS();

    for (;;) {
        tight_loop_contents();
    }
}

/*
 * This one does nothing at all - it exists purely because
 * configUSE_TICK_HOOK is turned on in FreeRTOSConfig.h, which requires
 * some function with this exact name to exist for the program to link
 * successfully, even if there is nothing this project actually needs to do
 * on every tick.
 */
extern "C" void vApplicationTickHook(void){}
