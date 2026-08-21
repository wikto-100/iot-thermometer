/*
 * ============================================================================
 * TEMPERATURE SSI - IMPLEMENTATION
 * ============================================================================
 *
 * See temperature_ssi.h for what "SSI" means in general. This file's job is
 * narrower: whenever lwIP hits the "<!--#temphist-->" placeholder inside
 * network/http/temphist.json, build a small piece of JSON text describing
 * the full temperature history, and hand it back to lwIP to insert in place
 * of the placeholder.
 *
 * THE OUTPUT FORMAT
 * The text this file builds looks like this (with real numbers, not "..."):
 *
 *     {"t":[1000,2000,3000],"v":[21.30,21.35,21.20]}
 *
 * "t" is the list of timestamps (milliseconds since the base station
 * booted, oldest first) and "v" is the matching list of temperature values
 * (as text, like "21.30"). The two lists always have the same length, and
 * reading list position N out of both tells you "at this time, the
 * temperature was this value". This is plain JSON (a very common,
 * simple text format for structured data), which the JavaScript running in
 * the browser (see network/http/index.shtml) can parse directly with
 * JSON.parse() to draw its chart.
 *
 * WHY BUILD THIS BY HAND INSTEAD OF ONE snprintf() CALL?
 * We do not know in advance how many readings there are (anywhere from 0 up
 * to TEMPERATURE_STORE_CAPACITY) or how many digits each number will take,
 * so the final text is a variable length built up piece by piece: an open
 * bracket, then a number, then a comma, then another number, and so on.
 * lwIP hands us a fixed-size scratch buffer (insert_buffer) to write into,
 * and we must be careful never to write past the end of it - the small
 * temperature_ssi_append() helper below takes care of that bounds-checking
 * every single time we add another piece of text, so the rest of the code
 * does not have to repeat that check everywhere.
 */

#include "temperature_ssi.h"

#include "temperature_store.h"
#include "temperature_format.h"

#include "lwip/apps/httpd.h"
#include "lwip/def.h"

#include <stdint.h>
#include <stdio.h>

/*
 * lwIP identifies which placeholder tag triggered the callback by a plain
 * number (an index into gs_ssi_tags below), not by its name - this enum
 * gives that number a readable name to use in the switch statement further
 * down, instead of a "magic number" like 0.
 */
enum
{
    TEMPERATURE_SSI_TAG_HISTORY = 0
};

/*
 * The list of placeholder names this file handles. Its array POSITION is
 * what lwIP passes back to us as "tag_index" (see
 * TEMPERATURE_SSI_TAG_HISTORY above) - so this array and that enum must
 * always be kept lined up with each other. We only handle one placeholder,
 * "temphist", matching the "<!--#temphist-->" text inside
 * network/http/temphist.json.
 */
static const char *gs_ssi_tags[] =
{
    "temphist"
};

/**
 * @brief Append text to insert_buffer, tracking how much has been written
 *
 * This is a small safety helper used everywhere below that we want to add
 * more text to the output. It writes as much of "text" as will still fit,
 * and moves *offset forward by however many characters were actually
 * written - this is what lets every other function below simply call this
 * repeatedly without each one needing to re-check "is there still room?"
 * by hand.
 *
 * @return 1 when the buffer is already completely full (so the caller
 *         should stop trying to add anything else), 0 otherwise
 */
static int temperature_ssi_append(
    char *insert_buffer,
    size_t insert_buffer_length,
    size_t *offset,
    const char *text)
{
    int written;

    if (*offset >= insert_buffer_length)
    {
        return 1;
    }

    /*
     * snprintf() is a version of "print formatted text" that writes into a
     * buffer in memory (instead of to the screen) and, importantly, NEVER
     * writes past the size limit you give it - even if the text you asked
     * it to print would have been longer. That is exactly the safety
     * property we need here, writing into lwIP's fixed-size buffer.
     */
    written = snprintf(
        insert_buffer + *offset,
        insert_buffer_length - *offset,
        "%s",
        text);

    if (written > 0)
    {
        *offset += (size_t)written;
    }

    return 0;
}

/**
 * @brief Build the "{"t":[...],"v":[...]}" JSON text described above
 *
 * This reads the whole current history out of temperature_store (see
 * ../temperature/temperature_store.c) and writes it out piece by piece:
 * the opening "{"t":[", then every timestamp separated by commas, then
 * "],"v":[", then every value separated by commas, then the closing "]}".
 */
static int temperature_ssi_append_history(
    char *insert_buffer,
    size_t insert_buffer_length,
    size_t *offset)
{
    /*
     * A local scratch copy of the readings, big enough for the maximum
     * possible number of them. This lives on this function's stack (it is
     * cleaned up automatically when the function returns) rather than
     * being kept anywhere long-term.
     */
    temperature_reading_t readings[TEMPERATURE_STORE_CAPACITY];
    size_t count = temperature_store_get_history(
        readings,
        TEMPERATURE_STORE_CAPACITY);

    if (temperature_ssi_append(
            insert_buffer, insert_buffer_length, offset, "{\"t\":["))
    {
        return 1;
    }

    /* Write out every timestamp, oldest first, separated by commas (but
     * with no comma before the very first one - that is what the "i > 0"
     * check below is for). */
    for (size_t i = 0; i < count; i++)
    {
        int written;

        if (i > 0 &&
            temperature_ssi_append(
                insert_buffer, insert_buffer_length, offset, ","))
        {
            return 1;
        }

        if (*offset >= insert_buffer_length)
        {
            return 1;
        }

        written = snprintf(
            insert_buffer + *offset,
            insert_buffer_length - *offset,
            "%lu",
            (unsigned long)readings[i].timestamp_ms);

        if (written > 0)
        {
            *offset += (size_t)written;
        }
    }

    if (temperature_ssi_append(
            insert_buffer, insert_buffer_length, offset, "],\"v\":["))
    {
        return 1;
    }

    /* Same idea as the timestamps loop above, but for the temperature
     * values. temperature_format_centi_c() (shared with the sensor board's
     * own debug logging - see common/temperature_format.c) turns a raw
     * number like 2130 into readable text like "21.30". */
    for (size_t i = 0; i < count; i++)
    {
        int written;

        if (i > 0 &&
            temperature_ssi_append(
                insert_buffer, insert_buffer_length, offset, ","))
        {
            return 1;
        }

        if (*offset >= insert_buffer_length)
        {
            return 1;
        }

        written = temperature_format_centi_c(
            readings[i].temperature_centi_c,
            insert_buffer + *offset,
            insert_buffer_length - *offset);

        if (written > 0)
        {
            *offset += (size_t)written;
        }
    }

    (void)temperature_ssi_append(
        insert_buffer, insert_buffer_length, offset, "]}");

    return 0;
}

/**
 * @brief The function lwIP calls to fill in an SSI placeholder
 *
 * This has a fixed signature required by lwIP's httpd library - we do not
 * get to choose the parameters, lwIP calls this function itself with these
 * exact arguments whenever it needs a placeholder filled in.
 *
 * @param tag_index          Which placeholder this is, as a position into
 *                            gs_ssi_tags above (see
 *                            TEMPERATURE_SSI_TAG_HISTORY).
 * @param insert_buffer       Where to write the replacement text. This
 *                            memory belongs to lwIP; we are only allowed to
 *                            write into it, not use it after this function
 *                            returns.
 * @param insert_buffer_length How many bytes insert_buffer can hold. This
 *                            is controlled by LWIP_HTTPD_MAX_TAG_INSERT_LEN
 *                            in lwipopts.h - it was made large enough to
 *                            fit the full temperature history as JSON text.
 *
 * @return How many bytes were actually written into insert_buffer.
 */
static u16_t temperature_ssi_handler(
    int tag_index,
    char *insert_buffer,
    int insert_buffer_length
#if LWIP_HTTPD_SSI_MULTIPART
    /*
     * LWIP_HTTPD_SSI_MULTIPART would let a single placeholder be filled in
     * across several smaller chunks, for cases where the replacement text
     * is too big to fit in one buffer. This project does not need that (our
     * buffer is sized generously enough for the whole history at once), so
     * this option is turned off in lwipopts.h and the extra parameters
     * below never actually get compiled in.
     */
    ,
    uint16_t current_tag_part,
    uint16_t *next_tag_part
#endif
)
{
    size_t offset;

#if LWIP_HTTPD_SSI_MULTIPART
    (void)current_tag_part;
    (void)next_tag_part;
#endif

    if (insert_buffer == NULL || insert_buffer_length <= 0)
    {
        return 0;
    }

    offset = 0;

    /*
     * Right now there is only one possible tag_index value
     * (TEMPERATURE_SSI_TAG_HISTORY), but this is written as a switch
     * statement so a second placeholder could be added later without
     * restructuring this function.
     */
    switch (tag_index)
    {
        case TEMPERATURE_SSI_TAG_HISTORY:
        {
            (void)temperature_ssi_append_history(
                insert_buffer,
                (size_t)insert_buffer_length,
                &offset);

            break;
        }

        default:
        {
            break;
        }
    }

    if (offset >= (size_t)insert_buffer_length)
    {
        return (u16_t)(insert_buffer_length - 1);
    }

    return (u16_t)offset;
}

void temperature_ssi_register(void)
{
    /*
     * Tell lwIP: "here is the function to call when you see one of these
     * placeholder names". From this point on, any file served that
     * contains "<!--#temphist-->" will have it replaced automatically.
     */
    http_set_ssi_handler(
        temperature_ssi_handler,
        gs_ssi_tags,
        LWIP_ARRAYSIZE(gs_ssi_tags));
}
