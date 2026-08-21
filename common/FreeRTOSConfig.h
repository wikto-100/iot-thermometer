/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * ============================================================================
 * FREERTOS CONFIGURATION - SHARED BY BOTH BOARDS
 * ============================================================================
 *
 * WHAT IS THIS FILE?
 * FreeRTOS (see app/main.c on either board for a fuller explanation of what
 * an "RTOS" actually is) is designed to be dropped into all kinds of very
 * different projects, on very different chips, with very different needs -
 * so instead of hard-coding one set of behaviors, it expects every project
 * to provide a file named exactly "FreeRTOSConfig.h" that answers a long
 * list of "how should you behave?" questions. This is that file. It is
 * used by BOTH programs in this project (base_station and sensor - see
 * each board's CMakeLists.txt, which points COMMON_PATH at this shared
 * folder), since both need the same basic FreeRTOS behavior on the same
 * kind of chip.
 *
 * Most of the settings below are the standard starting point recommended
 * for a Pico/RP2350 project and are not specific to this codebase's own
 * logic - comments are added below on the settings most worth
 * understanding, and lighter notes on the rest.
 */

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

/* Scheduler Related */
/* configUSE_PREEMPTION=1 means FreeRTOS can interrupt a running task at
 * any moment (not just when that task voluntarily pauses itself) to give
 * the CPU to a higher-priority task that has become ready to run. This is
 * what makes task priorities (see, for example,
 * NRF24L01_IRQ_TASK_PRIORITY in temperature_receiver.c) actually mean
 * something. */
#define configUSE_PREEMPTION                    1
/* "Tickless idle" would let the chip go into a low-power sleep mode
 * between tasks instead of periodically waking up to check "is anything
 * ready to run yet?". Turned off here since this project is not
 * optimizing for battery life. */
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     1
/* How many times per second FreeRTOS's internal clock "ticks" - each tick
 * is an opportunity for it to check whether it should switch which task is
 * running. 1000 Hz means once every 1 millisecond, which is also the
 * smallest unit of time something like vTaskDelay() can wait for. */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
/* How many different priority "levels" a task can be created with - see
 * the *_TASK_PRIORITY definitions throughout this project's task files for
 * how a handful of these are actually used (mostly tskIDLE_PRIORITY + a
 * small number). */
#define configMAX_PRIORITIES                    32
#define configMINIMAL_STACK_SIZE                ( configSTACK_DEPTH_TYPE ) 256
#define configUSE_16_BIT_TICKS                  0

#define configIDLE_SHOULD_YIELD                 1

/* Synchronization Related */
/* configUSE_MUTEXES=1 turns on the mutex lock feature used by
 * temperature_store.c (base station) to protect its shared ring buffer -
 * see that file for a full explanation of what a mutex is and why it is
 * needed. */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
/* Makes a handful of older/renamed FreeRTOS function names still work,
 * for compatibility with code (including parts of the Pico SDK's own
 * FreeRTOS integration) written against slightly older FreeRTOS APIs. */
#define configENABLE_BACKWARD_COMPATIBILITY     1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* System */
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

/* Memory allocation related definitions. */
/* Tasks, queues, mutexes, etc. are all allocated dynamically (using
 * malloc-style memory allocation under the hood) rather than requiring the
 * programmer to provide pre-allocated static memory for each one -
 * simpler to work with, at the cost of a little bit of predictability
 * about exactly when memory is used. configTOTAL_HEAP_SIZE is how much
 * memory, in bytes, is set aside for all of that: 128 KB here, out of the
 * roughly 520 KB of RAM this chip has in total. */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (128*1024)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Hook function related definitions. */
/* configCHECK_FOR_STACK_OVERFLOW=2 turns on FreeRTOS's (fairly thorough)
 * check for a task having used more stack space than it was given -
 * usually caused by a bug (like infinite recursion) rather than genuinely
 * needing more memory. When it detects this, it calls
 * vApplicationStackOverflowHook() - see hooks.cpp in this same folder for
 * what this project does when that happens. */
#define configCHECK_FOR_STACK_OVERFLOW          2
/* Similarly, calls vApplicationMallocFailedHook() (also in hooks.cpp) if
 * the chip ever runs out of the heap memory described above. */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Co-routine related definitions. */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1

/* Software timer related definitions. */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            1024

/* Interrupt nesting behaviour configuration. */
/*
#define configKERNEL_INTERRUPT_PRIORITY         [dependent of processor]
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    [dependent on processor and application]
#define configMAX_API_CALL_INTERRUPT_PRIORITY   [dependent on processor and application]
*/

/* SMP port only */
/*
 * The RP2350 chip (used in both the Pico 2 and Pico 2 W) actually has TWO
 * processor cores, and this particular FreeRTOS build ("SMP", short for
 * "symmetric multiprocessing") is able to run tasks on both of them
 * genuinely AT THE SAME TIME, not just quickly switching between them on
 * one core. configNUMBER_OF_CORES=1 deliberately turns that off and keeps
 * this project running on a single core, like a "normal" (non-SMP)
 * FreeRTOS setup. This matters for reasoning about the code: with only one
 * core, we know for certain that only ONE task's code is ever actually
 * executing at any single instant, and switches from one task to another
 * only happen at well-defined points (see configUSE_PREEMPTION above) -
 * this is what makes it safe for two tasks to share the same nRF24L01+
 * radio chip without an explicit lock protecting every single access to
 * it, relying instead on careful ordering (see the big comment at the top
 * of temperature_receiver.c, or temperature_sender.c on the sensor board,
 * for exactly how that ordering is arranged).
 */
#define configNUMBER_OF_CORES                   1
#define configTICK_CORE                         0
#define configRUN_MULTIPLE_PRIORITIES           0

/* RP2040 specific */
#define configSUPPORT_PICO_SYNC_INTEROP         1
#define configSUPPORT_PICO_TIME_INTEROP         1

#include <assert.h>
/* Define to trap errors during development. */
/* configASSERT(x) is a safety-check macro FreeRTOS's own internal code
 * calls in many places with a condition that should always be true (for
 * example, "this pointer should never be NULL here"). Defining it as
 * assert(x) means that if one of those conditions is ever violated - which
 * would indicate a real bug - the program stops immediately with an error,
 * rather than silently continuing to run in a corrupted state that would
 * be much harder to diagnose later. */
#define configASSERT(x)                         assert(x)

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
/* FreeRTOS can be built without some of its less commonly used functions,
 * to save a little bit of program size. Everything below is turned ON
 * (1), giving this project access to the full set of these optional
 * FreeRTOS functions - vTaskDelete() (used throughout this project to end
 * a task that hit an unrecoverable setup error), xTaskGetCurrentTaskHandle()
 * (used by tasks to record their own handle, as explained in
 * temperature_receiver.c), and so on. */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskResumeFromISR              1
#define INCLUDE_xQueueGetMutexHolder            1

/* A header file that defines trace macro can be included here. */
/*
 * PICO_RP2350 is automatically defined by the Pico SDK's build system when
 * targeting the RP2350 chip (used by both the Pico 2 and Pico 2 W - the
 * two boards this project's two programs run on), so everything in this
 * block only applies on this project's actual hardware. It turns off two
 * RP2350 security features this project does not use (the MPU - "memory
 * protection unit" - and TrustZone, both aimed at isolating untrusted
 * code, which is not a concern here), and turns on the FPU (floating-point
 * unit) so hardware-accelerated math on fractional numbers is available if
 * needed.
 */
#if PICO_RP2350

#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1
#define configENABLE_FPU                        1

#define configMAX_SYSCALL_INTERRUPT_PRIORITY    16

#endif
#endif /* FREERTOS_CONFIG_H */
