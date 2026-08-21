#ifndef TEMPERATURE_STORE_H
#define TEMPERATURE_STORE_H

#include <stddef.h>
#include <stdint.h>

#define TEMPERATURE_STORE_CAPACITY 48U

typedef struct
{
    int16_t temperature_centi_c;
    uint32_t timestamp_ms;
} temperature_reading_t;

/**
 * @brief Create the shared temperature history ring buffer
 *
 * Call this once before starting the scheduler or any component that accesses
 * temperature data.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t temperature_store_init(void);

/**
 * @brief Append a temperature reading, evicting the oldest once full
 *
 * The reading is timestamped with milliseconds since boot.
 *
 * @param temperature_centi_c Temperature in hundredths of a degree Celsius
 *
 * @return 0 on success, 1 when the store has not been initialized
 */
uint8_t temperature_store_set(int16_t temperature_centi_c);

/**
 * @brief Copy the stored readings, oldest first
 *
 * @param[out] out Destination array
 * @param max_count Capacity of @p out, in elements
 *
 * @return Number of readings copied, from 0 up to
 *         min(max_count, TEMPERATURE_STORE_CAPACITY)
 */
size_t temperature_store_get_history(
    temperature_reading_t *out,
    size_t max_count);

#endif
