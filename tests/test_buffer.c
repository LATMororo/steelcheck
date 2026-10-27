#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"
#include "suites.h"
#include "test.h"

#include <string.h>

/*
 *  File paths are relative to the project root, where make test runs the test runner.
 */

static constexpr char lf_path_[]      = "tests/data/lf.txt";
static constexpr char empty_path_[]   = "tests/data/empty.txt";
static constexpr char missing_path_[] = "tests/data/does_not_exist.txt";
static constexpr char gerdau_path_[]  = "data/gerdau_w_hp.csv";

//==================================================================================================
//  Successful reads
//==================================================================================================

static void test_load_lf_file ()
{
  static constexpr char expected[] = "first line\nsecond line\nthird line\n";

  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, lf_path_ ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( buff.size, sizeof expected - 1 );

  if ( buff.data != nullptr && buff.size == sizeof expected - 1 )
  {
    CHECK ( memcmp ( buff.data, expected, buff.size ) == 0 );
    CHECK ( buff.data[buff.size] == '\0' );
  }

  steelcheck_buffer_free ( &buff );
}

static void test_load_empty_file ()
{
  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, empty_path_ ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( buff.size, 0 );
  CHECK ( buff.data != nullptr );

  if ( buff.data != nullptr )
  {
    CHECK ( buff.data[0] == '\0' );
  }

  steelcheck_buffer_free ( &buff );
}

static void test_load_gerdau_catalog ()
{
  // About 15 KB, larger than the 4 KiB initial capacity, so the buffer has to grow.

  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, gerdau_path_ ), STEELCHECK_OK );
  CHECK ( buff.size > 4u * 1024u );

  if ( buff.data != nullptr && buff.size > 0 )
  {
    CHECK ( strncmp ( buff.data, "# catalog", 9 ) == 0 );
    CHECK ( buff.data[buff.size - 1] == '\n' );
    CHECK ( buff.data[buff.size] == '\0' );

    // A text file has no '\0' inside, so strlen stops exactly at the terminator.

    CHECK_SIZE_EQ ( strlen ( buff.data ), buff.size );
  }

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  Failures
//==================================================================================================

static void test_load_missing_file ()
{
  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, missing_path_ ), STEELCHECK_ERROR_OPEN );
  CHECK ( buff.data == nullptr );
  CHECK_SIZE_EQ ( buff.size, 0 );
}

static void test_load_directory ()
{
  // Linux opens a directory and then fails to read it; Windows fails to open it. Either way the
  // load fails and the buffer stays empty.

  SteelCheckBuffer buff = {};

  CHECK ( steelcheck_buffer_load ( &buff, "tests/data" ) != STEELCHECK_OK );
  CHECK ( buff.data == nullptr );
  CHECK_SIZE_EQ ( buff.size, 0 );
}

static void test_load_invalid_arguments ()
{
  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( nullptr, lf_path_ ), STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, nullptr ),    STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, "" ),         STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK ( buff.data == nullptr );
}

static void test_load_into_non_empty_buffer ()
{
  // Loading again into a buffer that still holds data would leak it, so it is refused, and the
  // existing data is left untouched.

  SteelCheckBuffer buff = {};

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, lf_path_ ), STEELCHECK_OK );

  const char* before = buff.data;

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, lf_path_ ), STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK ( buff.data == before );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  Free and view
//==================================================================================================

static void test_free_is_safe ()
{
  SteelCheckBuffer buff = {};

  steelcheck_buffer_free ( &buff );      // empty buffer
  steelcheck_buffer_free ( nullptr );    // null pointer

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, lf_path_ ), STEELCHECK_OK );

  steelcheck_buffer_free ( &buff );
  steelcheck_buffer_free ( &buff );      // freed twice

  CHECK ( buff.data == nullptr );
  CHECK_SIZE_EQ ( buff.size, 0 );
}

static void test_view ()
{
  SteelCheckBuffer buff = {};

  SteelCheckView empty = steelcheck_buffer_view ( &buff );

  CHECK ( empty.data == nullptr );
  CHECK_SIZE_EQ ( empty.length, 0 );

  SteelCheckView none = steelcheck_buffer_view ( nullptr );

  CHECK ( none.data == nullptr );
  CHECK_SIZE_EQ ( none.length, 0 );

  CHECK_INT_EQ ( steelcheck_buffer_load ( &buff, lf_path_ ), STEELCHECK_OK );

  // A view of a const buffer: the parameter is const-qualified.

  const SteelCheckBuffer* const_buff = &buff;
  SteelCheckView          view       = steelcheck_buffer_view ( const_buff );

  CHECK ( view.data == buff.data );
  CHECK_SIZE_EQ ( view.length, buff.size );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  test_buffer_suite
//==================================================================================================

void test_buffer_suite ()
{
  RUN_TEST ( test_load_lf_file );
  RUN_TEST ( test_load_empty_file );
  RUN_TEST ( test_load_gerdau_catalog );
  RUN_TEST ( test_load_missing_file );
  RUN_TEST ( test_load_directory );
  RUN_TEST ( test_load_invalid_arguments );
  RUN_TEST ( test_load_into_non_empty_buffer );
  RUN_TEST ( test_free_is_safe );
  RUN_TEST ( test_view );
}
