/*
 * ============================================================================
 * FREERTOS HOOKS - PUBLIC INTERFACE
 * ============================================================================
 *
 * A "hook" is a function that FreeRTOS itself will call automatically at
 * certain moments, IF the project provides one - it is a way for FreeRTOS
 * to say "let me know if this ever happens" without needing to know
 * anything about what this specific project wants to do about it. This
 * file declares the three hook functions FreeRTOSConfig.h (in this same
 * shared folder) turns on; hooks.cpp provides the actual code that runs.
 * Both boards (base_station and sensor) compile and use this same file, so
 * both react to these events the same way.
 */

#ifndef HOOKS_H
#define HOOKS_H

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Called by FreeRTOS if it ever runs out of heap memory
 *
 * See configUSE_MALLOC_FAILED_HOOK and configTOTAL_HEAP_SIZE in
 * FreeRTOSConfig.h.
 */
void vApplicationMallocFailedHook(void);

/**
 * @brief Called by FreeRTOS if it detects a task has overflowed its stack
 *
 * See configCHECK_FOR_STACK_OVERFLOW in FreeRTOSConfig.h. A "stack
 * overflow" means a task used more of its own private scratch memory (see
 * the *_TASK_STACK_DEPTH definitions throughout this project) than it was
 * given, which is almost always a sign of a bug rather than something to
 * recover from gracefully.
 *
 * @param task      Which task overflowed.
 * @param task_name That task's name, as given to xTaskCreate() (see, for
 *                  example, task_utils_create() in this same shared
 *                  folder).
 */
void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name
);

/**
 * @brief Called by FreeRTOS on every single "tick" (see configTICK_RATE_HZ
 *        in FreeRTOSConfig.h - by default, once every millisecond)
 *
 * This runs very frequently and from a timer-interrupt-like context, so it
 * must do very little work, very quickly. See hooks.cpp - this project
 * currently does nothing at all here; it only exists because
 * configUSE_TICK_HOOK is turned on and FreeRTOS therefore requires SOME
 * function with this exact name to exist.
 */
void vApplicationTickHook(void);

#ifdef __cplusplus
}
#endif

#endif
