#include "steelcheck/compat.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 *  The single definitions of the counters declared extern in test.h.
 */

size_t test_checks_run    = 0;
size_t test_checks_failed = 0;

static size_t tests_run    = 0;
static size_t tests_failed = 0;

//==================================================================================================
//  test_fail, test_fail_int, test_fail_size, test_fail_str, test_fail_near
//==================================================================================================

void test_fail ( const char* file, int line, const char* expr )
{
  test_checks_failed++;
  fprintf ( stderr, "  %s:%d: %s failed\n", file, line, expr );
}

void test_fail_int ( const char* file, int line, const char* expr, long long a, long long b )
{
  test_fail ( file, line, expr );
  fprintf ( stderr, "    left:  %lld\n    right: %lld\n", a, b );
}

void test_fail_size ( const char* file, int line, const char* expr, size_t a, size_t b )
{
  test_fail ( file, line, expr );
  fprintf ( stderr, "    left:  %zu\n    right: %zu\n", a, b );
}

void test_fail_str ( const char* file, int line, const char* expr,
                     const char* a, const char* b )
{
  test_fail ( file, line, expr );
  fprintf ( stderr, "    left:  %s%s%s\n", a ? "\"" : "", a ? a : "(null)", a ? "\"" : "" );
  fprintf ( stderr, "    right: %s%s%s\n", b ? "\"" : "", b ? b : "(null)", b ? "\"" : "" );
}

void test_fail_near ( const char* file, int line, const char* expr,
                      double a, double b, double rel_tol )
{
  test_fail ( file, line, expr );
  fprintf ( stderr, "    left:  %.17g\n    right: %.17g\n    rel_tol: %g\n", a, b, rel_tol );
}

//==================================================================================================
//  test_str_eq
//==================================================================================================

bool test_str_eq ( const char* a, const char* b )
{
  // Two null pointers are equal; a null pointer never equals a string.

  if ( a == nullptr || b == nullptr )
    return a == b;

  return strcmp ( a, b ) == 0;
}

//==================================================================================================
//  test_near
//==================================================================================================

bool test_near ( double a, double b, double rel_tol )
{
  // Exact equality first, so equal infinities pass. NaN never passes: every comparison is false.

  if ( a == b )
    return true;

  double diff  = fabs ( a - b );
  double scale = fmax ( fabs( a ), fabs( b ) );

  return diff <= fmax ( rel_tol * scale, test_abs_tol );
}

//==================================================================================================
//  test_run
//==================================================================================================

void test_run ( const char* name, void (*fn) () )
{
  size_t failed_before = test_checks_failed;

  fn ();
  tests_run++;

  if ( test_checks_failed == failed_before )
  {
    printf ( "[PASS] %s\n", name );
  }
  else
  {
    tests_failed++;
    printf ( "[FAIL] %s\n", name );
  }

  // Keep the [PASS]/[FAIL] lines (stdout) and the failure details (stderr) in order.

  fflush ( stdout );
}

//==================================================================================================
//  test_summary
//==================================================================================================

int test_summary ()
{
  printf ( "\n%zu tests, %zu failed; %zu checks, %zu failed\n",
           tests_run, tests_failed, test_checks_run, test_checks_failed );

  return tests_failed == 0 ? 0 : 1;
}
