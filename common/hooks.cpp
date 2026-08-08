#include "hooks.h"
#include "pico/stdlib.h"

extern "C" void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;) {
        tight_loop_contents();
    }
}

extern "C" void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name
)
{
    (void)task;
    (void)task_name;

    taskDISABLE_INTERRUPTS();

    for (;;) {
        tight_loop_contents();
    }
}
extern "C" void vApplicationTickHook(void){}
