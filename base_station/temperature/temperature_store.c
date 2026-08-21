/*
 * ============================================================================
 * TEMPERATURE STORE - IMPLEMENTATION
 * ============================================================================
 *
 * See temperature_store.h for the "why" behind this file (the ring buffer
 * idea, and why a mutex lock is needed). This file is the "how": the actual
 * array, and the bookkeeping needed to make it wrap around correctly.
 *
 * HOW THE RING BUFFER BOOKKEEPING WORKS
 * We keep three pieces of state alongside the array of readings itself:
 *
 *   gs_next_write - the index (0 to 47) where the NEXT new reading will be
 *       written. It starts at 0 and moves forward by one every time a
 *       reading is written, wrapping back to 0 after it passes 47 (that
 *       wrap-around is what makes this a "ring": index 47 is followed by
 *       index 0 again, like the hands on a clock).
 *
 *   gs_count - how many readings have been written in TOTAL since startup,
 *       but capped at TEMPERATURE_STORE_CAPACITY (48). While the store is
 *       still filling up for the first time, gs_count tells us how many of
 *       the 48 array slots actually contain real data (the rest are just
 *       leftover zeroed memory we should not read yet). Once gs_count
 *       reaches 48, it stays at 48 forever - from that point on, EVERY
 *       slot contains a real reading, and gs_next_write tells us which one
 *       is about to be overwritten next (which is also the OLDEST one).
 *
 *   gs_mutex - the lock described in temperature_store.h, making sure only
 *       one task touches gs_readings / gs_count / gs_next_write at a time.
 *
 * A WORKED EXAMPLE
 * Suppose the capacity were 4 instead of 48, to make the numbers easier to
 * follow, and four readings A, B, C, D arrive in that order:
 *
 *     write A -> array is [A _ _ _], gs_next_write=1, gs_count=1
 *     write B -> array is [A B _ _], gs_next_write=2, gs_count=2
 *     write C -> array is [A B C _], gs_next_write=3, gs_count=3
 *     write D -> array is [A B C D], gs_next_write=0, gs_count=4 (full!)
 *
 * Now a fifth reading E arrives. Because the store is full, E overwrites
 * whatever sits at gs_next_write (index 0, which holds A - the oldest one):
 *
 *     write E -> array is [E B C D], gs_next_write=1, gs_count stays 4
 *
 * The array now holds B, C, D, E in time order, but they are NOT laid out
 * in that order in memory - B happens to be the "oldest" one now, and it
 * sits at index 1, which happens to be where gs_next_write is pointing.
 * That is exactly the pattern used below: once full, whatever index
 * gs_next_write points at IS the oldest reading, because it is the next
 * one due to be overwritten.
 */

#include "temperature_store.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "pico/time.h"

/* The ring buffer itself: 48 reading "slots" in a plain C array. */
static temperature_reading_t gs_readings[TEMPERATURE_STORE_CAPACITY];

/* How many of those slots currently hold a real reading (see the comment
 * block above), capped at TEMPERATURE_STORE_CAPACITY once the buffer fills
 * up for the first time. */
static size_t gs_count = 0;

/* The array index the NEXT incoming reading will be written to. */
static size_t gs_next_write = 0;

/* The lock. NULL until temperature_store_init() has run successfully. */
static SemaphoreHandle_t gs_mutex = NULL;

uint8_t temperature_store_init(void)
{
    /* Already initialized - do nothing and report success, so callers do
     * not need to worry about calling this more than once by accident. */
    if (gs_mutex != NULL)
    {
        return 0;
    }

    /*
     * xSemaphoreCreateMutex() is a FreeRTOS function that creates a mutex
     * lock and hands us a "handle" (a reference/pointer we use to refer to
     * it later). This allocates a small amount of memory internally; if the
     * microcontroller is out of memory it returns NULL instead.
     */
    gs_mutex = xSemaphoreCreateMutex();

    if (gs_mutex == NULL)
    {
        return 1;
    }

    return 0;
}

uint8_t temperature_store_set(int16_t temperature_centi_c)
{
    /* If temperature_store_init() was never called (or failed), refuse to
     * touch the shared array - we would have no protection against another
     * task reading it at the same time. */
    if (gs_mutex == NULL)
    {
        return 1;
    }

    /*
     * "Take" the lock before touching any shared data below. portMAX_DELAY
     * means "wait as long as it takes" - if another task currently holds
     * the lock, this task simply pauses here (without wasting CPU time)
     * until that other task gives it back. Because both tasks that use
     * this store only ever hold the lock for a few, very fast lines of
     * code, that wait is always extremely short in practice.
     */
    if (xSemaphoreTake(gs_mutex, portMAX_DELAY) != pdTRUE)
    {
        return 1;
    }

    /* Write the new reading into the next free (or "about to be reused")
     * slot, and record when it arrived. */
    gs_readings[gs_next_write].temperature_centi_c = temperature_centi_c;
    gs_readings[gs_next_write].timestamp_ms =
        to_ms_since_boot(get_absolute_time());

    /* Move the write pointer forward by one slot, wrapping back to 0 once
     * it goes past the last slot (index 47 -> 0). The "%" (modulo/
     * remainder) operator is what makes this wrap-around happen: any
     * number modulo TEMPERATURE_STORE_CAPACITY always comes out between 0
     * and TEMPERATURE_STORE_CAPACITY - 1. */
    gs_next_write = (gs_next_write + 1U) % TEMPERATURE_STORE_CAPACITY;

    /* Once we have written TEMPERATURE_STORE_CAPACITY readings, every slot
     * is full and gs_count should stop growing - it stays pinned at the
     * capacity forever after that. */
    if (gs_count < TEMPERATURE_STORE_CAPACITY)
    {
        gs_count++;
    }

    /* "Give" the lock back so any other task waiting for it can proceed. */
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

    /* Same locking idea as in temperature_store_set() above: this function
     * reads the shared array, so it needs the same protection against a
     * write happening at the same time. */
    if (xSemaphoreTake(gs_mutex, portMAX_DELAY) != pdTRUE)
    {
        return 0;
    }

    /* Never try to copy out more readings than either (a) actually exist
     * yet, or (b) fit in the caller's array. */
    to_copy = (gs_count < max_count) ? gs_count : max_count;

    /*
     * Where does the OLDEST reading currently live in the array?
     *
     * - If the store has not filled up yet (gs_count is still less than
     *   the capacity), we have never wrapped around, so the very first
     *   reading ever written is still sitting at index 0, and everything
     *   is simply in order starting from there.
     *
     * - If the store IS full, the oldest reading is the one about to be
     *   overwritten next - which, as explained in the big comment at the
     *   top of this file, is exactly wherever gs_next_write is pointing.
     */
    oldest_index = (gs_count < TEMPERATURE_STORE_CAPACITY)
        ? 0U
        : gs_next_write;

    /*
     * Walk forward from the oldest reading, wrapping around the array as
     * needed (using "%" again, the same trick as above), copying each one
     * out in time order: oldest first, newest last.
     */
    for (size_t i = 0; i < to_copy; i++)
    {
        out[i] = gs_readings[
            (oldest_index + i) % TEMPERATURE_STORE_CAPACITY];
    }

    xSemaphoreGive(gs_mutex);

    return to_copy;
}
