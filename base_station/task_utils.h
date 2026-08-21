#ifndef TASK_UTILS_H
#define TASK_UTILS_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Create a single FreeRTOS task and normalize the result
 *
 * @return 0 on success, 1 when task creation fails
 */
uint8_t task_utils_create(
    TaskFunction_t task_function,
    const char *name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *parameters,
    UBaseType_t priority);

#endif
