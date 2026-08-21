/*
 * ============================================================================
 * TEMPERATURE STORE - THE SHARED "MEMORY" OF RECENT READINGS
 * ============================================================================
 *
 * This file describes a small piece of shared memory that remembers the
 * last 48 temperature readings the base station has received, along with
 * roughly when each one arrived.
 *
 * WHY DO WE NEED THIS AT ALL?
 * The radio code (temperature_receiver.c) receives one new reading at a
 * time, whenever the sensor board decides to send one. The web page code
 * (temperature_ssi.c) wants to show a CHART of recent readings whenever a
 * browser asks for the page. Those two things happen completely
 * independently of each other, on different FreeRTOS tasks, at
 * unpredictable times. This "store" is the middleman: the radio code
 * writes new readings into it, and the web page code reads out of it,
 * without either side needing to know anything about the other.
 *
 * WHAT KIND OF STORAGE IS THIS?
 * It is called a "ring buffer" (also sometimes a "circular buffer"). Picture
 * 48 numbered parking spaces arranged in a circle. Every time a new reading
 * comes in, it is written into the next free space. Once all 48 spaces are
 * full, the NEXT new reading overwrites the OLDEST one - the one that has
 * been sitting there longest - because we only ever care about the most
 * recent 48 readings, not the full history since the device was powered on.
 * This is a very memory-efficient way to keep "the last N of something"
 * without ever needing to shift or copy the whole list around.
 *
 * WHY DOES THIS NEED SPECIAL PROTECTION (A "MUTEX")?
 * Because two different FreeRTOS tasks touch this same memory at times
 * neither one can predict: the radio task might be in the middle of writing
 * a new reading in RAM at the exact same instant the web server task is in
 * the middle of reading them out, to build the chart data. If that happened
 * with no protection, the web server could read a reading that is "half
 * old, half new" - for example, the correct temperature value but paired
 * with the WRONG timestamp - producing garbage output. To prevent that, this
 * module uses a "mutex" (short for "mutual exclusion"): a kind of lock that
 * only one task can hold at a time. Before touching the shared data, a task
 * must "take" the mutex; when it is done, it "gives" the mutex back. If a
 * second task tries to take the mutex while the first task is still holding
 * it, FreeRTOS makes the second task wait until the first task gives it
 * back. This guarantees the two tasks never touch the data at the exact
 * same moment.
 */

#ifndef TEMPERATURE_STORE_H
#define TEMPERATURE_STORE_H

#include <stddef.h>
#include <stdint.h>

/*
 * How many readings we remember at once. 48 was chosen so the web page's
 * chart shows a reasonably long history without using much memory - each
 * reading only takes a few bytes, so 48 of them is tiny.
 */
#define TEMPERATURE_STORE_CAPACITY 48U

/*
 * One single reading, as kept in the store: the temperature itself, plus
 * the time it arrived.
 *
 * temperature_centi_c: the temperature in HUNDREDTHS of a degree Celsius,
 *     stored as a whole number instead of a fraction (a "float"). For
 *     example, 2345 means 23.45 degrees C, and -725 means -7.25 degrees C.
 *     Microcontrollers like this one are much faster and more predictable
 *     at doing math with whole numbers than with fractional numbers, so the
 *     whole codebase (both boards) represents temperatures this way and
 *     only turns them into a human-friendly "23.45" string right at the
 *     very end, when formatting text for a person to read.
 *
 * timestamp_ms: how many milliseconds had passed since the base station
 *     was powered on, at the moment this reading arrived. This is NOT a
 *     real wall-clock time (like "3:45 PM") - the Pico has no
 *     battery-backed clock and no internet time sync, so it only knows how
 *     long it has been running, not what the actual date and time is. The
 *     web page uses this to space out the bars on its chart and to label
 *     roughly "how long ago" each reading was taken.
 */
typedef struct
{
    int16_t temperature_centi_c;
    uint32_t timestamp_ms;
} temperature_reading_t;

/**
 * @brief Create the shared temperature history ring buffer
 *
 * This must run once, right at the start of the program, before any task
 * that might call temperature_store_set() or temperature_store_get_history()
 * is created. It creates the mutex lock described above. Calling it more
 * than once is safe and simply does nothing the second time.
 *
 * @return 0 on success, 1 on failure
 */
uint8_t temperature_store_init(void);

/**
 * @brief Append a temperature reading, evicting the oldest once full
 *
 * This is called by the radio-receiving code every time a fresh reading
 * arrives from the sensor board. The reading is stamped with "how many
 * milliseconds since boot" automatically, using the Pico's internal clock.
 * If the store already holds 48 readings, the single oldest one is quietly
 * thrown away to make room for this new one - the caller does not need to
 * do anything special for that to happen.
 *
 * @param temperature_centi_c Temperature in hundredths of a degree Celsius
 *
 * @return 0 on success, 1 when the store has not been initialized
 */
uint8_t temperature_store_set(int16_t temperature_centi_c);

/**
 * @brief Copy the stored readings, oldest first
 *
 * This is called by the web page code whenever a browser asks for the
 * temperature history. It copies every reading currently held in the store
 * into the array the caller provides, starting with the OLDEST reading and
 * ending with the NEWEST one - this "oldest first" order is exactly what
 * the web page's chart wants, since it draws left-to-right in the order
 * time passed.
 *
 * @param[out] out Destination array the readings are copied into. The
 *                 caller owns this memory; this function only writes to it,
 *                 it never keeps a reference to it afterwards.
 * @param max_count How many elements @p out can hold. This function will
 *                  never write more than this many readings, even if more
 *                  are available, so it can never overflow the caller's
 *                  array.
 *
 * @return How many readings were actually copied. This will be the smaller
 *         of @p max_count and however many readings are currently stored
 *         (which starts at 0 right after boot and grows up to
 *         TEMPERATURE_STORE_CAPACITY as readings arrive).
 */
size_t temperature_store_get_history(
    temperature_reading_t *out,
    size_t max_count);

#endif
