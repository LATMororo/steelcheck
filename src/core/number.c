#include "steelcheck/number.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  '0' to '9'. Written out instead of isdigit() for the same reasons as in view.c.
 */

static bool is_digit_ ( char c )
{
  return c >= '0' && c <= '9';
}

/*
 *  Advances *i past a run of digits and returns how many there were.
 */

static size_t skip_digits_ ( SteelCheckView text, size_t* i )
{
  size_t count = 0;

  while ( *i < text.length && is_digit_( text.data[*i] ) )
  {
    ++*i;
    ++count;
  }

  return count;
}

/*
 *  True if the whole of text matches the accepted format (see number.h). This is a small
 *  hand-written scanner: it walks the text once, from left to right, one part of the format after
 *  the other, and the text is valid only if every byte has been consumed at the end.
 */

static bool is_decimal_number_ ( SteelCheckView text )
{
  size_t i = 0;

  // Optional sign.

  if ( i < text.length && ( text.data[i] == '+' || text.data[i] == '-' ) )
  {
    ++i;
  }

  // Integer part, optional '.', fraction part. At least one digit in total: "." and "-" are not
  // numbers.

  size_t digits = skip_digits_ ( text, &i );

  if ( i < text.length && text.data[i] == '.' )
  {
    ++i;
    digits += skip_digits_ ( text, &i );
  }

  if ( digits == 0 )
  {
    return false;
  }

  // Optional exponent, which must have at least one digit: "1e" and "1e+" are not numbers.

  if ( i < text.length && ( text.data[i] == 'e' || text.data[i] == 'E' ) )
  {
    ++i;

    if ( i < text.length && ( text.data[i] == '+' || text.data[i] == '-' ) )
    {
      ++i;
    }

    if ( skip_digits_( text, &i ) == 0 )
    {
      return false;
    }
  }

  // Anything left over ("12mm", "1,5", " ", ...) makes the text invalid.

  return i == text.length;
}

//==================================================================================================
//  steelcheck_parse_double
//==================================================================================================

SteelCheckStatus steelcheck_parse_double ( SteelCheckView text, double* value )
{
  if ( value == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  if ( text.length == 0 )
  {
    return STEELCHECK_ERROR_MISSING_VALUE;
  }

  if ( text.length > steelcheck_number_max_length || !is_decimal_number_( text ) )
  {
    return STEELCHECK_ERROR_NUMBER;
  }

  // strtod() needs a '\0'-terminated string, and a view is not terminated: the bytes after it
  // belong to the next field, or to nothing at all. Passing the view's data directly could make
  // strtod() read past the view (a view of "12" inside "123" would give 123). So the text is
  // copied into a small local array and terminated there. The length check above guarantees that
  // it fits, including the '\0'.

  char copy[steelcheck_number_max_length + 1];

  memcpy ( copy, text.data, text.length );
  copy[text.length] = '\0';

  // strtod() reports overflow and underflow only through errno, and never sets errno to 0 itself,
  // so errno is cleared first. Otherwise an ERANGE left over from an earlier call would show up
  // here.

  char* end = nullptr;

  errno = 0;

  double result = strtod ( copy, &end );

  if ( errno == ERANGE )
  {
    return STEELCHECK_ERROR_RANGE;
  }

  // Both cannot happen after the format check, but they are cheap to verify: strtod() must have
  // used every character, and the format excludes "inf" and "nan".

  if ( end != copy + text.length || !isfinite ( result ) )
  {
    return STEELCHECK_ERROR_NUMBER;
  }

  *value = result;

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_parse_flag
//==================================================================================================

SteelCheckStatus steelcheck_parse_flag ( SteelCheckView text, bool* value )
{
  if ( value == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  if ( text.length == 0 )
  {
    return STEELCHECK_ERROR_MISSING_VALUE;
  }

  if ( steelcheck_view_equals_cstr( text, "0" ) )
  {
    *value = false;
    return STEELCHECK_OK;
  }

  if ( steelcheck_view_equals_cstr( text, "1" ) )
  {
    *value = true;
    return STEELCHECK_OK;
  }

  return STEELCHECK_ERROR_NUMBER;
}
