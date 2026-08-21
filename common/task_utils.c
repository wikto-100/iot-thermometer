/*
 * ============================================================================
 * TASK UTILS - IMPLEMENTATION
 * ============================================================================
 *
 * See task_utils.h for the full explanation of why this tiny wrapper
 * exists. The implementation itself is almost entirely just a direct call
 * through to FreeRTOS's own xTaskCreate().
 */

#include "task_utils.h"

uint8_t task_utils_create(
    TaskFunction_t task_function,
    const char *name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *parameters,
    UBaseType_t priority,
    TaskHandle_t *out_handle)
{
    /*
     * This is the real FreeRTOS function that does the actual work of
     * creating a task: allocating memory for its stack and internal
     * bookkeeping, and registering it with the scheduler so it becomes
     * eligible to run. It returns "pdPASS" (a FreeRTOS-specific success
     * value, not simply 1 or 0) if it worked, or an error code if it
     * could not (almost always because the chip ran out of memory).
     */
    BaseType_t result = xTaskCreate(
        task_function,
        name,
        stack_depth,
        parameters,
        priority,
        out_handle);

    /* Translate FreeRTOS's pdPASS/error-code convention into the simple
     * 0 = success / 1 = failure convention used everywhere else in this
     * project. */
    return (result == pdPASS) ? 0 : 1;
}
