/*
 * ============================================================================
 * TEMPERATURE FORMAT - PUBLIC INTERFACE
 * ============================================================================
 *
 * Both boards, and several different files within each, need to turn a
 * temperature value into human-readable text, for slightly different
 * purposes: printing it in a debug log message, or building it into a
 * piece of JSON text for the web page (see
 * base_station/web/temperature_ssi.c). This one tiny shared function is
 * the single place that number-to-text conversion happens, so all of
 * those places agree on exactly the same formatting.
 *
 * WHY "CENTI-CELSIUS"?
 * Throughout this whole project, a temperature is stored as a whole number
 * (an int16_t) representing HUNDREDTHS of a degree Celsius, rather than as
 * a fractional ("float") number of degrees directly. For example, 2345
 * means 23.45 degrees C. "Centi-" means "hundredth", the same prefix as in
 * "centimeter" (a hundredth of a meter) - hence "centi-Celsius" for "a
 * hundredth of a degree Celsius". Small microcontrollers like the ones
 * used in this project are much faster, and much more predictable, doing
 * math with whole numbers than with fractional numbers, so the raw
 * network protocol and internal storage everywhere in this project uses
 * whole numbers - this function is where that raw number FINALLY gets
 * turned into the "23.45"-style text a person actually wants to read.
 */

#ifndef TEMPERATURE_FORMAT_H
#define TEMPERATURE_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format a signed centi-Celsius value as "-DD.DD" or "DD.DD"
 *
 * Examples:
 *     2345  ->  "23.45"
 *     -725  ->  "-7.25"
 *
 * @param[in]  temperature_centi_c value to format
 * @param[out] buffer destination buffer the text is written into. This
 *                    function follows the same safety rule as the standard
 *                    C library's snprintf() (which it uses internally): it
 *                    will never write more than buffer_length bytes into
 *                    this buffer, no matter how long the resulting text
 *                    would otherwise have been.
 * @param[in]  buffer_length capacity of buffer, in bytes
 *
 * @return number of characters that would be written excluding the
 *         terminating null, matching snprintf()'s return convention. A
 *         buffer of 16 bytes is always big enough to hold any possible
 *         result from this function, including the trailing null
 *         terminator (the largest possible text, "-327.67", is only 7
 *         characters long, since the underlying value is a 16-bit signed
 *         number).
 */
int temperature_format_centi_c(
    int16_t temperature_centi_c,
    char *buffer,
    size_t buffer_length
);

#ifdef __cplusplus
}
#endif

#endif
