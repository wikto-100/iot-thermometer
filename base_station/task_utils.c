#include "task_utils.h"

uint8_t task_utils_create(
    TaskFunction_t task_function,
    const char *name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *parameters,
    UBaseType_t priority)
{
    BaseType_t result = xTaskCreate(
        task_function,
        name,
        stack_depth,
        parameters,
        priority,
        NULL);

    return (result == pdPASS) ? 0 : 1;
}
