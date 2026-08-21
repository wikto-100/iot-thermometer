#include "temperature_ssi.h"

#include "temperature_store.h"

#include "lwip/apps/httpd.h"
#include "lwip/def.h"

#include <stdint.h>
#include <stdio.h>

enum
{
    TEMPERATURE_SSI_TAG_HISTORY = 0
};

/* The array position is the index supplied to the SSI handler. */
static const char *gs_ssi_tags[] =
{
    "temphist"
};

static int temperature_ssi_format_value(
    int16_t temperature_centi_c,
    char *buffer,
    size_t buffer_length)
{
    int32_t signed_temperature = temperature_centi_c;
    uint32_t magnitude;

    if (signed_temperature < 0)
    {
        magnitude = (uint32_t)(-signed_temperature);

        return snprintf(
            buffer,
            buffer_length,
            "-%lu.%02lu",
            (unsigned long)(magnitude / 100U),
            (unsigned long)(magnitude % 100U));
    }

    magnitude = (uint32_t)signed_temperature;

    return snprintf(
        buffer,
        buffer_length,
        "%lu.%02lu",
        (unsigned long)(magnitude / 100U),
        (unsigned long)(magnitude % 100U));
}

/**
 * @brief Append text to insert_buffer, tracking how much has been written
 *
 * @return 1 when the buffer is full and no further appends should be tried,
 *         0 otherwise
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

static int temperature_ssi_append_history(
    char *insert_buffer,
    size_t insert_buffer_length,
    size_t *offset)
{
    temperature_reading_t readings[TEMPERATURE_STORE_CAPACITY];
    size_t count = temperature_store_get_history(
        readings,
        TEMPERATURE_STORE_CAPACITY);

    if (temperature_ssi_append(
            insert_buffer, insert_buffer_length, offset, "{\"t\":["))
    {
        return 1;
    }

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

        written = temperature_ssi_format_value(
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

static u16_t temperature_ssi_handler(
    int tag_index,
    char *insert_buffer,
    int insert_buffer_length
#if LWIP_HTTPD_SSI_MULTIPART
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
    http_set_ssi_handler(
        temperature_ssi_handler,
        gs_ssi_tags,
        LWIP_ARRAYSIZE(gs_ssi_tags));
}
