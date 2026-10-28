#include "steelcheck/compat.h"
#include "steelcheck/view.h"
#include "suites.h"
#include "test.h"

#include <limits.h>

/*
 *  A view of a string literal (the literal itself stays alive for the whole program).
 */

static SteelCheckView v_ ( const char* string )
{
  return steelcheck_view_from_cstr ( string );
}

//==================================================================================================
//  Tests
//==================================================================================================

static void test_view_from_cstr ()
{
  SteelCheckView view = v_ ( "W150x13.0" );

  CHECK_SIZE_EQ ( view.length, 9 );
  CHECK ( view.data != nullptr );

  SteelCheckView none = v_ ( nullptr );

  CHECK ( none.data == nullptr );
  CHECK ( steelcheck_view_is_empty( none ) );
  CHECK ( steelcheck_view_is_empty( v_( "" ) ) );
  CHECK ( !steelcheck_view_is_empty( view ) );
}

static void test_view_trim ()
{
  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( "  Ix_cm4  " ) ), "Ix_cm4" ) );
  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( "\t a b \t" ) ), "a b" ) );
  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( "abc" ) ), "abc" ) );
  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( "   " ) ), "" ) );
  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( "" ) ), "" ) );

  // Only spaces and tabs are blanks: '\r' is kept (the line iterator already removed CRLF).

  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( " a\r" ) ), "a\r" ) );

  // UTF-8 bytes (>= 0x80) are ordinary data, also at the ends.

  CHECK ( steelcheck_view_equals_cstr( steelcheck_view_trim( v_( " ção " ) ), "ção" ) );

  // An empty view without data is returned unchanged.

  SteelCheckView none = steelcheck_view_trim ( (SteelCheckView) {} );

  CHECK ( none.data == nullptr && none.length == 0 );
}

static void test_view_equals ()
{
  CHECK (  steelcheck_view_equals( v_( "abc" ), v_( "abc" ) ) );
  CHECK ( !steelcheck_view_equals( v_( "abc" ), v_( "abd" ) ) );
  CHECK ( !steelcheck_view_equals( v_( "abc" ), v_( "ab" ) ) );
  CHECK (  steelcheck_view_equals( v_( "" ), (SteelCheckView) {} ) );

  CHECK (  steelcheck_view_equals_cstr( v_( "Ix_cm4" ), "Ix_cm4" ) );
  CHECK ( !steelcheck_view_equals_cstr( v_( "Ix_cm4" ), "ix_cm4" ) );   // case-sensitive
  CHECK (  steelcheck_view_equals_cstr( (SteelCheckView) {}, nullptr ) );

  // Only the first length bytes count: a view of part of a string.

  SteelCheckView part = { .length = 2, .data = "W150" };

  CHECK ( steelcheck_view_equals_cstr( part, "W1" ) );
}

static void test_view_starts_with ()
{
  CHECK (  steelcheck_view_starts_with( v_( "W150x13.0" ), "W150" ) );
  CHECK (  steelcheck_view_starts_with( v_( "W150x13.0" ), "" ) );
  CHECK (  steelcheck_view_starts_with( v_( "W150x13.0" ), "W150x13.0" ) );
  CHECK ( !steelcheck_view_starts_with( v_( "W150" ), "W150x13.0" ) );
  CHECK ( !steelcheck_view_starts_with( v_( "HP200" ), "W" ) );
  CHECK (  steelcheck_view_starts_with( (SteelCheckView) {}, nullptr ) );
}

static void test_view_print_length ()
{
  CHECK_INT_EQ ( steelcheck_view_print_length( v_( "abc" ) ), 3 );
  CHECK_INT_EQ ( steelcheck_view_print_length( (SteelCheckView) {} ), 0 );

  // Clamped, never converted to a negative int. Only the length is used, so data can be nullptr.

  SteelCheckView huge = { .length = (size_t) INT_MAX + 10, .data = nullptr };

  CHECK_INT_EQ ( steelcheck_view_print_length( huge ), INT_MAX );
}

//==================================================================================================
//  test_view_suite
//==================================================================================================

void test_view_suite ()
{
  RUN_TEST ( test_view_from_cstr );
  RUN_TEST ( test_view_trim );
  RUN_TEST ( test_view_equals );
  RUN_TEST ( test_view_starts_with );
  RUN_TEST ( test_view_print_length );
}
