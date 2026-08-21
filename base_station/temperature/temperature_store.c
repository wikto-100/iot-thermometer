#include "temperature_store.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "pico/time.h"

static temperature_reading_t gs_readings[TEMPERATURE_STORE_CAPACITY];
static size_t gs_count = 0;
static size_t gs_next_write = 0;
static SemaphoreHandle_t gs_mutex = NULL;

uint8_t temperature_store_init(void)
{
    if (gs_mutex != NULL)
    {
        return 0;
    }

    gs_mutex = xSemaphoreCreateMutex();

    if (gs_mutex == NULL)
    {
        return 1;
    }

    return 0;
}

uint8_t temperature_store_set(int16_t temperature_centi_c)
{
    if (gs_mutex == NULL)
    {
        return 1;
    }

    if (xSemaphoreTake(gs_mutex, portMAX_DELAY) != pdTRUE)
    {
        return 1;
    }

    gs_readings[gs_next_write].temperature_centi_c = temperature_centi_c;
    gs_readings[gs_next_write].timestamp_ms =
        to_ms_since_boot(get_absolute_time());
    gs_next_write = (gs_next_write + 1U) % TEMPERATURE_STORE_CAPACITY;

    if (gs_count < TEMPERATURE_STORE_CAPACITY)
    {
        gs_count++;
    }

    xSemaphoreGive(gs_mutex);

    return 0;
}

size_t temperature_store_get_history(
    temperature_reading_t *out,
    size_t max_count)
{
    size_t oldest_index;
    size_t to_copy;

    if (out == NULL || max_count == 0U || gs_mutex == NULL)
    {
        return 0;
    }

    if (xSemaphoreTake(gs_mutex, portMAX_DELAY) != pdTRUE)
    {
        return 0;
    }

    to_copy = (gs_count < max_count) ? gs_count : max_count;

    oldest_index = (gs_count < TEMPERATURE_STORE_CAPACITY)
        ? 0U
        : gs_next_write;

    for (size_t i = 0; i < to_copy; i++)
    {
        out[i] = gs_readings[
            (oldest_index + i) % TEMPERATURE_STORE_CAPACITY];
    }

    xSemaphoreGive(gs_mutex);

    return to_copy;
}
