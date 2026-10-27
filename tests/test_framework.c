#include "steelcheck/compat.h"
#include "suites.h"
#include "test.h"

#include <math.h>

/*
 *  Tests of the test framework's own comparison helpers.
 */

static void test_near_tolerances ()
{
  CHECK (  test_near ( 1.0, 1.0001, 1e-3 ) );
  CHECK ( !test_near ( 1.0, 1.0001, 1e-6 ) );
  CHECK (  test_near ( 0.0, 1e-15,  1e-9 ) );
  CHECK ( !test_near ( 0.0, 1e-6,   1e-9 ) );
  CHECK ( !test_near ( 1.0, (double) NAN, 1e-3 ) );   // NAN has type float

  CHECK_NEAR ( 345.0 / 1.10, 313.636, 1e-5 );
}

static void test_str_eq_null ()
{
  CHECK (  test_str_eq ( nullptr, nullptr ) );
  CHECK ( !test_str_eq ( "W150x13.0", nullptr ) );
  CHECK (  test_str_eq ( "W150x13.0", "W150x13.0" ) );
}

//==================================================================================================
//  test_framework_suite
//==================================================================================================

void test_framework_suite ()
{
  RUN_TEST ( test_near_tolerances );
  RUN_TEST ( test_str_eq_null );
}
