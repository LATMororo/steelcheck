#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"
#include "steelcheck/lines.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

/*
 *  A string literal and its length, for inputs that are not '\0'-terminated views. sizeof counts
 *  the literal's terminating '\0', hence the - 1.
 */

#define LIT_(s) (s), (sizeof (s) - 1)

/*
 *  The three lines stored in every tests/data fixture, whatever its line endings.
 */

static const char* const fixture_lines_[] = { "first line", "second line", "third line" };

//==================================================================================================
//  view_eq_
//==================================================================================================

static bool view_eq_ ( SteelCheckView view, const char* expected, size_t expected_length )
{
  return view.length == expected_length
      && ( expected_length == 0 || memcmp ( view.data, expected, expected_length ) == 0 );
}

//==================================================================================================
//  lines_match_
//==================================================================================================

/*
 *  Runs the iterator over input and compares every produced line, and its number, with
 *  expected[0 .. count-1]. Prints the first difference to stderr. Used as CHECK ( lines_match_
 *  (...) ), so a failure reports the line of the test case itself.
 */

static bool lines_match_ ( const char* input, size_t length, size_t count,
                           const char* const expected[] )
{
  SteelCheckLines iter        = {};
  SteelCheckView  line        = {};
  size_t          line_number = 0;

  steelcheck_lines_init ( &iter, (SteelCheckView) { .length = length, .data = input } );

  for ( size_t i = 0; i < count; i++ )
  {
    if ( !steelcheck_lines_next( &iter, &line, &line_number ) )
    {
      fprintf ( stderr, "    only %zu line(s), expected %zu\n", i, count );
      return false;
    }

    if ( line_number != i + 1 )
    {
      fprintf ( stderr, "    line number %zu, expected %zu\n", line_number, i + 1 );
      return false;
    }

    if ( !view_eq_( line, expected[i], strlen( expected[i] ) ) )
    {
      fprintf ( stderr, "    line %zu is \"%.*s\", expected \"%s\"\n",
                i + 1, (int) line.length, line.data, expected[i] );
      return false;
    }
  }

  if ( steelcheck_lines_next( &iter, &line, &line_number ) )
  {
    fprintf ( stderr, "    more than the expected %zu line(s)\n", count );
    return false;
  }

  // After the last line, the outputs are reset.

  return line.data == nullptr && line.length == 0 && line_number == 0;
}

//==================================================================================================
//  Line endings, BOM, empty input
//==================================================================================================

static void test_lines_endings ()
{
  CHECK ( lines_match_( LIT_( "a\nb\n" ),     2, (const char* const[]) { "a", "b" } ) );
  CHECK ( lines_match_( LIT_( "a\nb" ),       2, (const char* const[]) { "a", "b" } ) );
  CHECK ( lines_match_( LIT_( "a\r\nb\r\n" ), 2, (const char* const[]) { "a", "b" } ) );
  CHECK ( lines_match_( LIT_( "a\r\nb\r" ),   2, (const char* const[]) { "a", "b" } ) );
  CHECK ( lines_match_( LIT_( "a\nb\r\n" ),   2, (const char* const[]) { "a", "b" } ) );

  // A '\r' inside a line stays: CR-only line endings (old Mac) are not supported.

  CHECK ( lines_match_( LIT_( "a\rb\n" ),     1, (const char* const[]) { "a\rb" } ) );

  // Only one '\r' is removed.

  CHECK ( lines_match_( LIT_( "a\r\r\n" ),    1, (const char* const[]) { "a\r" } ) );
}

static void test_lines_empty ()
{
  CHECK ( lines_match_( LIT_( "" ),           0, nullptr ) );
  CHECK ( lines_match_( LIT_( "\n" ),         1, (const char* const[]) { "" } ) );
  CHECK ( lines_match_( LIT_( "\n\n" ),       2, (const char* const[]) { "", "" } ) );
  CHECK ( lines_match_( LIT_( "\r\n" ),       1, (const char* const[]) { "" } ) );
  CHECK ( lines_match_( LIT_( "a\n\nb\n" ),   3, (const char* const[]) { "a", "", "b" } ) );

  // An empty view (no data at all) has no lines.

  CHECK ( lines_match_( nullptr, 0,            0, nullptr ) );
}

static void test_lines_bom ()
{
  // The BOM is written as a separate literal: "\xBFa" would be read as one hex escape, \xBFa.

  CHECK ( lines_match_( LIT_( "\xEF\xBB\xBF" "a\n" ), 1, (const char* const[]) { "a" } ) );
  CHECK ( lines_match_( LIT_( "\xEF\xBB\xBF" ),       0, nullptr ) );

  // A BOM after the start is ordinary data.

  CHECK ( lines_match_( LIT_( "a\n" "\xEF\xBB\xBF" "b\n" ), 2,
                        (const char* const[]) { "a", "\xEF\xBB\xBF" "b" } ) );

  // An incomplete BOM is not skipped.

  CHECK ( lines_match_( LIT_( "\xEF\xBB" "a" ),       1,
                        (const char* const[]) { "\xEF\xBB" "a" } ) );
}

static void test_lines_embedded_nul ()
{
  // A '\0' byte does not end a line: lengths are explicit.

  static constexpr char input[] = "a\0b\nc";

  SteelCheckLines iter        = {};
  SteelCheckView  line        = {};
  size_t          line_number = 0;

  steelcheck_lines_init ( &iter, (SteelCheckView) { .length = sizeof input - 1, .data = input } );

  CHECK ( steelcheck_lines_next( &iter, &line, &line_number ) );
  CHECK ( view_eq_( line, "a\0b", 3 ) );

  CHECK ( steelcheck_lines_next( &iter, &line, &line_number ) );
  CHECK ( view_eq_( line, "c", 1 ) );
  CHECK_SIZE_EQ ( line_number, 2 );

  CHECK ( !steelcheck_lines_next( &iter, &line, &line_number ) );
}

static void test_lines_null_arguments ()
{
  SteelCheckLines iter        = {};
  SteelCheckView  line        = {};
  size_t          line_number = 0;

  steelcheck_lines_init ( nullptr, (SteelCheckView) {} );      // must not crash
  steelcheck_lines_init ( &iter, (SteelCheckView) { .length = 2, .data = "a\n" } );

  CHECK ( !steelcheck_lines_next( nullptr, &line, &line_number ) );
  CHECK ( !steelcheck_lines_next( &iter, nullptr, &line_number ) );
  CHECK ( !steelcheck_lines_next( &iter, &line, nullptr ) );

  // The rejected calls did not move the iterator: the line is still there.

  CHECK ( steelcheck_lines_next( &iter, &line, &line_number ) );
  CHECK_SIZE_EQ ( line_number, 1 );
}

//==================================================================================================
//  Files
//==================================================================================================

/*
 *  Loads path and checks that it holds exactly the three fixture lines.
 */

static bool file_has_fixture_lines_ ( const char* path )
{
  SteelCheckBuffer buff = {};

  if ( steelcheck_buffer_load( &buff, path ) != STEELCHECK_OK )
  {
    fprintf ( stderr, "    cannot load %s\n", path );
    return false;
  }

  SteelCheckView view = steelcheck_buffer_view ( &buff );
  bool           ok   = lines_match_ ( view.data, view.length, 3, fixture_lines_ );

  steelcheck_buffer_free ( &buff );

  return ok;
}

static void test_lines_fixture_files ()
{
  // Same three lines, whatever the line endings, BOM, or final newline.

  CHECK ( file_has_fixture_lines_( "tests/data/lf.txt" ) );
  CHECK ( file_has_fixture_lines_( "tests/data/crlf.txt" ) );
  CHECK ( file_has_fixture_lines_( "tests/data/bom.txt" ) );
  CHECK ( file_has_fixture_lines_( "tests/data/no_final_newline.txt" ) );
}

static void test_lines_gerdau_catalog ()
{
  // 8 comment lines, the header (line 9), and 108 sections (lines 10 to 117).

  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load( &buff, "data/gerdau_w_hp.csv" ), STEELCHECK_OK );

  SteelCheckLines iter        = {};
  SteelCheckView  line        = {};
  size_t          line_number = 0;
  size_t          count       = 0;

  steelcheck_lines_init ( &iter, steelcheck_buffer_view( &buff ) );

  while ( steelcheck_lines_next( &iter, &line, &line_number ) )
  {
    count++;

    CHECK ( line.length > 0 );
    CHECK ( memchr( line.data, '\r', line.length ) == nullptr );

    if ( line_number == 9 )
    {
      CHECK ( line.length > 12 && memcmp( line.data, "designation,", 12 ) == 0 );
    }

    if ( line_number == 117 )
    {
      CHECK ( line.length > 11 && memcmp( line.data, "W610x217.0,", 11 ) == 0 );
    }
  }

  CHECK_SIZE_EQ ( count, 117 );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  test_lines_suite
//==================================================================================================

void test_lines_suite ()
{
  RUN_TEST ( test_lines_endings );
  RUN_TEST ( test_lines_empty );
  RUN_TEST ( test_lines_bom );
  RUN_TEST ( test_lines_embedded_nul );
  RUN_TEST ( test_lines_null_arguments );
  RUN_TEST ( test_lines_fixture_files );
  RUN_TEST ( test_lines_gerdau_catalog );
}
