#ifndef STEELCHECK_NUMBER_H
#define STEELCHECK_NUMBER_H

#include "steelcheck/compat.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>

/*
 *  Conversion of field views to numbers.
 *
 *  Accepted format (decimal only, '.' as the decimal separator, nothing else in the field):
 *
 *      [sign] digits [ "." [digits] ] [exponent]      e.g.  13.0   -4.3   148   5.   1e3
 *      [sign] "." digits [exponent]                   e.g.  .5     -.25
 *
 *      sign      "+" or "-"
 *      exponent  "e" or "E", an optional sign, and at least one digit   e.g.  1.5E-2
 *
 *  Rejected, although strtod() alone would accept or half-accept them: leading or trailing
 *  spaces, "inf", "nan", hexadecimal ("0x1A"), a decimal comma ("1,5" would silently become 1),
 *  units ("12mm" would become 12), digit separators ("1_000", "1'000").
 *
 *  Not detectable: a thousands separator written as '.', e.g. "4.181" meaning 4181. It is a
 *  valid decimal number.
 *
 *  The program never calls setlocale(), so strtod() always uses the "C" locale, where the decimal
 *  separator is '.', also on computers set to Portuguese.
 */

/*
 *  Longest accepted number, in characters. Longer fields are rejected as invalid numbers: real
 *  data never needs more than about 20 characters.
 */

constexpr size_t steelcheck_number_max_length = 63;

/*
 *  Converts text to a double.
 *
 *  Returns STEELCHECK_OK and stores the value, or, without touching *value:
 *    STEELCHECK_ERROR_MISSING_VALUE     text is empty (the caller decides whether that is allowed);
 *    STEELCHECK_ERROR_NUMBER            text is not in the accepted format, or is too long;
 *    STEELCHECK_ERROR_RANGE             the value overflows, or is so small that it underflows;
 *    STEELCHECK_ERROR_INVALID_ARGUMENT  value is nullptr.
 */

[[nodiscard]] SteelCheckStatus steelcheck_parse_double ( SteelCheckView text, double* value );

/*
 *  Converts a 0/1 flag ("0" is false, "1" is true), e.g. the h_shape column of the catalog.
 *  Same status codes as steelcheck_parse_double (anything other than "0" or "1" is
 *  STEELCHECK_ERROR_NUMBER).
 */

[[nodiscard]] SteelCheckStatus steelcheck_parse_flag ( SteelCheckView text, bool* value );

#endif // STEELCHECK_NUMBER_H
