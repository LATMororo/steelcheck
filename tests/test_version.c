#include "steelcheck/compat.h"
#include "steelcheck/version.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_version_string ()
{
  // The string must match the three numbers, formatted independently here.

  char expected[32];

  int n = snprintf ( expected, sizeof expected, "%d.%d.%d",
                     STEELCHECK_VERSION_MAJOR, STEELCHECK_VERSION_MINOR, STEELCHECK_VERSION_PATCH );

  CHECK ( n > 0 && (size_t) n < sizeof expected );
  CHECK_STR_EQ ( steelcheck_version(), expected );
}

static void test_build_info ()
{
  const char* info = steelcheck_build_info ();

  CHECK ( info != nullptr );
  CHECK ( strstr( info, "C standard 20" ) != nullptr );   // 202311L, or 202000L on GCC 14
}

//==================================================================================================
//  test_version_suite
//==================================================================================================

void test_version_suite ()
{
  RUN_TEST ( test_version_string );
  RUN_TEST ( test_build_info );
}
