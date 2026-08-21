/*
 * ============================================================================
 * TEMPERATURE FORMAT - IMPLEMENTATION
 * ============================================================================
 *
 * See temperature_format.h for the "why". This file is the actual "how":
 * turning a raw whole number like 2345 (meaning 23.45 degrees) or -725
 * (meaning -7.25 degrees) into readable text.
 *
 * THE MATH, STEP BY STEP
 * Take -725 as an example (meaning -7.25 degrees C):
 *   1. Split off the sign: is the number negative? If so, remember that,
 *      and continue working with its positive "magnitude" (725) instead -
 *      this makes the rest of the math simpler, since we do not have to
 *      worry about how integer division behaves with negative numbers.
 *   2. Divide the magnitude by 100 to get the WHOLE-DEGREES part:
 *      725 / 100 = 7 (integer division throws away the remainder here).
 *   3. Take the REMAINDER after that division to get the FRACTIONAL part,
 *      as hundredths: 725 % 100 = 25 (the "%" operator, called "modulo",
 *      gives you exactly what division "leaves over").
 *   4. Put it back together as text, with the sign back in front and the
 *      fractional part always shown as exactly two digits (with a leading
 *      zero if needed, so "5" becomes "05" rather than looking like a
 *      different number): "-7.25".
 */

#include "temperature_format.h"

#include <stdio.h>

int temperature_format_centi_c(
    int16_t temperature_centi_c,
    char *buffer,
    size_t buffer_length
)
{
    /*
     * Widen to a 32-bit signed number before negating it. This avoids a
     * subtle edge case: negating the smallest possible 16-bit signed
     * number (-32768) cannot be represented as a positive 16-bit number
     * (the largest one is only 32767), which would misbehave if we tried
     * to negate it while still a 16-bit value. Doing the negation in
     * 32-bit space instead sidesteps that entirely, since a 32-bit signed
     * number has plenty of extra room.
     */
    int32_t signed_value = temperature_centi_c;
    uint32_t magnitude;

    if (signed_value < 0)
    {
        /* Turn the negative value positive, so the digit math below (the
         * / 100 and % 100 in the steps described above) is the same
         * regardless of sign - we just remember to print a "-" in front
         * separately. */
        magnitude = (uint32_t)(-signed_value);

        /*
         * snprintf() is a version of "print formatted text" that writes
         * into a buffer in memory (instead of onto the screen) and never
         * writes past buffer_length, even if the requested text would
         * have been longer - the same safety property mentioned in
         * temperature_format.h. "%02lu" means "print this number as an
         * unsigned long, at least 2 digits wide, padded with a leading
         * zero if it is shorter" - that is what turns a fractional part
         * of "5" into "05" rather than "5".
         */
        return snprintf(
            buffer,
            buffer_length,
            "-%lu.%02lu",
            (unsigned long)(magnitude / 100U),
            (unsigned long)(magnitude % 100U));
    }

    magnitude = (uint32_t)signed_value;

    /* Same idea as above, just without the leading "-" since the value is
     * zero or positive. */
    return snprintf(
        buffer,
        buffer_length,
        "%lu.%02lu",
        (unsigned long)(magnitude / 100U),
        (unsigned long)(magnitude % 100U));
}
