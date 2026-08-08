#ifndef HOOKS_H
#define HOOKS_H

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

void vApplicationMallocFailedHook(void);

void vApplicationStackOverflowHook(
    TaskHandle_t task,
    char *task_name
);
void vApplicationTickHook(void);

#ifdef __cplusplus
}
#endif

#endif
