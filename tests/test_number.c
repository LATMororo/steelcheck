#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"
#include "steelcheck/csv.h"
#include "steelcheck/lines.h"
#include "steelcheck/number.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static SteelCheckView v_ ( const char* string )
{
  return steelcheck_view_from_cstr ( string );
}

/*
 *  True if text parses successfully to expected (within a relative 1e-15, i.e. exactly up to the
 *  rounding of the decimal literal). Prints the actual result otherwise.
 */

static bool parses_to_ ( const char* text, double expected )
{
  double           value  = 0.0;
  SteelCheckStatus status = steelcheck_parse_double ( v_( text ), &value );

  if ( status != STEELCHECK_OK || !test_near( value, expected, 1e-15 ) )
  {
    fprintf ( stderr, "    \"%s\": %s, value %.17g, expected %.17g\n", text,
              steelcheck_status_message( status ), value, expected );
    return false;
  }

  return true;
}

/*
 *  True if text is rejected with the expected status, and the output was not touched.
 */

static bool rejected_ ( const char* text, SteelCheckStatus expected )
{
  double           value  = 123.0;
  SteelCheckStatus status = steelcheck_parse_double ( v_( text ), &value );

  if ( status != expected || value != 123.0 )
  {
    fprintf ( stderr, "    \"%s\": %s (value %g), expected %s\n", text,
              steelcheck_status_message( status ), value, steelcheck_status_message( expected ) );
    return false;
  }

  return true;
}

//==================================================================================================
//  Valid numbers
//==================================================================================================

static void test_parse_valid ()
{
  CHECK ( parses_to_( "0",        0.0 ) );
  CHECK ( parses_to_( "148",      148.0 ) );
  CHECK ( parses_to_( "13.0",     13.0 ) );
  CHECK ( parses_to_( "0.67",     0.67 ) );
  CHECK ( parses_to_( "-4.3",     -4.3 ) );
  CHECK ( parses_to_( "+2.5",     2.5 ) );
  CHECK ( parses_to_( ".5",       0.5 ) );
  CHECK ( parses_to_( "5.",       5.0 ) );
  CHECK ( parses_to_( "00012",    12.0 ) );
  CHECK ( parses_to_( "1e3",      1000.0 ) );
  CHECK ( parses_to_( "1.5E-2",   0.015 ) );
  CHECK ( parses_to_( "2.1e+05",  210000.0 ) );
  CHECK ( parses_to_( "14676643", 14676643.0 ) );   // the largest Cw of the catalog, in cm^6

  // A tiny but normal double is fine; only underflow below the normal range is rejected.

  CHECK ( parses_to_( "1e-300",   1e-300 ) );
}

static void test_parse_view_bounds ()
{
  // Only the bytes of the view count, not what follows it in memory: a view of "12" inside "123"
  // is 12. Passing the data straight to strtod() would give 123.

  double         value = 0.0;
  SteelCheckView part  = { .length = 2, .data = "123" };

  CHECK_INT_EQ ( steelcheck_parse_double( part, &value ), STEELCHECK_OK );
  CHECK_NEAR   ( value, 12.0, 0.0 );

  // A field in the middle of a line, as the CSV layer produces it.

  static constexpr char row[] = "W150x13.0,13.0,148";

  SteelCheckView field = { .length = 4, .data = &row[10] };   // "13.0"

  CHECK_INT_EQ ( steelcheck_parse_double( field, &value ), STEELCHECK_OK );
  CHECK_NEAR   ( value, 13.0, 0.0 );
}

//==================================================================================================
//  Rejected text
//==================================================================================================

static void test_parse_invalid ()
{
  // Empty: missing, not invalid, so the caller can accept it for optional columns.

  CHECK ( rejected_( "",        STEELCHECK_ERROR_MISSING_VALUE ) );

  // Things strtod() would accept, fully or partly.

  CHECK ( rejected_( " 12",     STEELCHECK_ERROR_NUMBER ) );   // leading space
  CHECK ( rejected_( "12 ",     STEELCHECK_ERROR_NUMBER ) );   // trailing space
  CHECK ( rejected_( "inf",     STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "-INF",    STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "nan",     STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "0x1A",    STEELCHECK_ERROR_NUMBER ) );   // hexadecimal
  CHECK ( rejected_( "1,5",     STEELCHECK_ERROR_NUMBER ) );   // decimal comma: strtod gives 1
  CHECK ( rejected_( "12mm",    STEELCHECK_ERROR_NUMBER ) );   // unit: strtod gives 12
  CHECK ( rejected_( "1e",      STEELCHECK_ERROR_NUMBER ) );   // strtod gives 1
  CHECK ( rejected_( "1e+",     STEELCHECK_ERROR_NUMBER ) );

  // Other malformed numbers.

  CHECK ( rejected_( ".",       STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "-",       STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "+",       STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "--1",     STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "1.2.3",   STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "e5",      STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "1_000",   STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "1'000",   STEELCHECK_ERROR_NUMBER ) );
  CHECK ( rejected_( "12\xC2\xA0", STEELCHECK_ERROR_NUMBER ) ); // UTF-8 no-break space
  CHECK ( rejected_( "abc",     STEELCHECK_ERROR_NUMBER ) );

  // Too long: 64 digits is one more than the maximum.

  char long_number[65];

  memset ( long_number, '1', 64 );
  long_number[64] = '\0';

  CHECK ( rejected_( long_number, STEELCHECK_ERROR_NUMBER ) );

  // Exactly the maximum length is accepted.

  long_number[steelcheck_number_max_length] = '\0';

  double value = 0.0;

  CHECK_INT_EQ ( steelcheck_parse_double( v_( long_number ), &value ), STEELCHECK_OK );
}

static void test_parse_range ()
{
  CHECK ( rejected_( "1e400",   STEELCHECK_ERROR_RANGE ) );   // overflow
  CHECK ( rejected_( "-1e400",  STEELCHECK_ERROR_RANGE ) );
  CHECK ( rejected_( "1e-400",  STEELCHECK_ERROR_RANGE ) );   // underflow to 0
  CHECK ( rejected_( "1e-310",  STEELCHECK_ERROR_RANGE ) );   // below the normal range
}

static void test_parse_null_argument ()
{
  CHECK_INT_EQ ( steelcheck_parse_double( v_( "1" ), nullptr ), STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "1" ), nullptr ),   STEELCHECK_ERROR_INVALID_ARGUMENT );
}

//==================================================================================================
//  Flags
//==================================================================================================

static void test_parse_flag ()
{
  bool flag = false;

  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "1" ), &flag ), STEELCHECK_OK );
  CHECK ( flag );

  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "0" ), &flag ), STEELCHECK_OK );
  CHECK ( !flag );

  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "" ),     &flag ), STEELCHECK_ERROR_MISSING_VALUE );
  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "2" ),    &flag ), STEELCHECK_ERROR_NUMBER );
  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "01" ),   &flag ), STEELCHECK_ERROR_NUMBER );
  CHECK_INT_EQ ( steelcheck_parse_flag( v_( "true" ), &flag ), STEELCHECK_ERROR_NUMBER );
}

//==================================================================================================
//  The Gerdau catalog: every numeric field of every row
//==================================================================================================

/*
 *  Catalog columns (1-based): 1 designation and 2 family are text, 3 h_shape and 4 on_request are
 *  flags, 5 to 26 are numbers, 27 designation_imperial is text. The field iterator is enough here.
 */

static void test_gerdau_numbers ()
{
  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load( &buff, "data/gerdau_w_hp.csv" ), STEELCHECK_OK );

  SteelCheckLines lines       = {};
  SteelCheckView  line        = {};
  size_t          line_number = 0;
  size_t          numbers     = 0;
  size_t          flags       = 0;
  size_t          failures    = 0;

  // Values of the first (W150x13.0) and last (W610x217.0) sections, to check a few by value.

  double first_d  = 0.0, first_cw = 0.0, first_u = 0.0;
  double last_cw  = 0.0;

  steelcheck_lines_init ( &lines, steelcheck_buffer_view( &buff ) );

  while ( steelcheck_lines_next( &lines, &line, &line_number ) )
  {
    // Data rows are lines 10 to 117 (comments and the header come before).

    if ( line_number < 10 )
    {
      continue;
    }

    SteelCheckFields fields = {};
    SteelCheckView   field  = {};
    size_t           column = 0;

    steelcheck_fields_init ( &fields, line );

    while ( steelcheck_fields_next( &fields, &field, &column ) )
    {
      SteelCheckStatus status = STEELCHECK_OK;

      if ( column == 3 || column == 4 )
      {
        bool flag = false;
        status    = steelcheck_parse_flag ( field, &flag );
        flags++;
      }
      else if ( column >= 5 && column <= 26 )
      {
        double value = 0.0;
        status       = steelcheck_parse_double ( field, &value );
        numbers++;

        if ( line_number == 10 && column == 6 )  first_d  = value;
        if ( line_number == 10 && column == 25 ) first_cw = value;
        if ( line_number == 10 && column == 26 ) first_u  = value;
        if ( line_number == 117 && column == 25 ) last_cw = value;
      }

      if ( status != STEELCHECK_OK )
      {
        fprintf ( stderr, "    line %zu, column %zu: \"%.*s\": %s\n", line_number, column,
                  steelcheck_view_print_length ( field ), field.data,
                  steelcheck_status_message ( status ) );
        failures++;
      }
    }
  }

  // 108 rows: 22 numbers and 2 flags each, all valid.

  CHECK_SIZE_EQ ( numbers, 108 * 22 );
  CHECK_SIZE_EQ ( flags, 108 * 2 );
  CHECK_SIZE_EQ ( failures, 0 );

  CHECK_NEAR ( first_d,  148.0,      0.0 );
  CHECK_NEAR ( first_cw, 4181.0,     0.0 );   // printed "4.181" in the catalog: thousands dot
  CHECK_NEAR ( first_u,  0.67,       1e-15 );
  CHECK_NEAR ( last_cw,  14676643.0, 0.0 );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  test_number_suite
//==================================================================================================

void test_number_suite ()
{
  RUN_TEST ( test_parse_valid );
  RUN_TEST ( test_parse_view_bounds );
  RUN_TEST ( test_parse_invalid );
  RUN_TEST ( test_parse_range );
  RUN_TEST ( test_parse_null_argument );
  RUN_TEST ( test_parse_flag );
  RUN_TEST ( test_gerdau_numbers );
}
