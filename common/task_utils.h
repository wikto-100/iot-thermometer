/*
 * ============================================================================
 * TASK UTILS - PUBLIC INTERFACE
 * ============================================================================
 *
 * A very small helper shared by both boards, that wraps FreeRTOS's own
 * xTaskCreate() function - the function that actually creates a new
 * background task. Every single task created anywhere in this project (see
 * temperature_receiver.c and base_station_network.c on the base station,
 * and temperature_sender.c on the sensor) goes through this one helper,
 * for two small but genuinely useful reasons:
 *
 *   1. xTaskCreate() returns a somewhat unusual "pdPASS" value on success
 *      (rather than the plain 0-means-success convention used everywhere
 *      else in this codebase) - this helper translates that into the same
 *      simple "0 = success, 1 = failure" convention the rest of the
 *      project already uses everywhere else, so callers do not need to
 *      remember a second, different convention just for this one function.
 *
 *   2. It gives every task creation call the exact same shape, which makes
 *      the code that creates tasks throughout this project easy to
 *      recognize and compare at a glance.
 */

#ifndef TASK_UTILS_H
#define TASK_UTILS_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Create a single FreeRTOS task and normalize the result
 *
 * @param task_function The function that will run as the task's body -
 *                       always written as an infinite loop that starts by
 *                       waiting for something to happen (see any of the
 *                       *_task() functions in this project for examples).
 * @param name           A short, human-readable name for the task, useful
 *                       when debugging (for example, with FreeRTOS's own
 *                       task-listing tools).
 * @param stack_depth    How much scratch memory (for local variables etc.)
 *                       to set aside for this task. Measured in "words"
 *                       (4 bytes each on this chip), not bytes.
 * @param parameters     A value passed through to task_function when it
 *                       starts - most tasks in this project do not need
 *                       one and simply pass NULL.
 * @param priority       How important this task is relative to others -
 *                       higher numbers run first when more than one task
 *                       is ready to run at the same moment. See
 *                       FreeRTOSConfig.h's configMAX_PRIORITIES comment.
 * @param[out] out_handle optional; receives the created task's handle
 *                        immediately, for callers that need it before
 *                        the task has had a chance to run (for
 *                        example, to arm an interrupt against it).
 *                        Pass NULL when the task instead registers its
 *                        own handle via xTaskGetCurrentTaskHandle() as
 *                        the first thing it does when it starts running -
 *                        which is what most tasks in this project do,
 *                        since it is simpler and there is usually no need
 *                        for anyone else to have the handle any earlier
 *                        than that.
 *
 * @return 0 on success, 1 when task creation fails (most commonly because
 *         the chip does not have enough free memory left to create it)
 */
uint8_t task_utils_create(
    TaskFunction_t task_function,
    const char *name,
    configSTACK_DEPTH_TYPE stack_depth,
    void *parameters,
    UBaseType_t priority,
    TaskHandle_t *out_handle);

#endif
