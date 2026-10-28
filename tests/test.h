#ifndef STEELCHECK_TEST_H
#define STEELCHECK_TEST_H

#include "steelcheck/compat.h"

#include <stddef.h>

/*
 *  Minimal test framework. A failed CHECK prints its location and the test continues;
 *  RUN_TEST reports [PASS] or [FAIL] per test; test_summary returns the exit status.
 */

/*
 *  Counters, declared here and defined in test.c (exactly one definition).
 */

extern size_t test_checks_run;
extern size_t test_checks_failed;

/*
 *  Functions used by the macros below. Each test_fail_* prints one failure and counts it.
 */

void test_fail      ( const char* file, int line, const char* expr );
void test_fail_int  ( const char* file, int line, const char* expr, long long a, long long b );
void test_fail_size ( const char* file, int line, const char* expr, size_t a, size_t b );
void test_fail_str  ( const char* file, int line, const char* expr,
                      const char* a, const char* b );
void test_fail_near ( const char* file, int line, const char* expr,
                      double a, double b, double rel_tol );

bool test_str_eq ( const char* a, const char* b );
bool test_near   ( double a, double b, double rel_tol );

void test_run     ( const char* name, void (*fn) () );
int  test_summary ();

/*
 *  Absolute tolerance used by test_near, so values close to zero compare as equal.
 */

constexpr double test_abs_tol = 1e-12;

/*
 *  Check macros. Each argument is evaluated exactly once, and do { } while (0) makes each
 *  macro behave like a single statement (safe after an if without braces).
 */

#define CHECK(cond)                                                                       \
  do {                                                                                    \
    test_checks_run++;                                                                    \
    if ( !(cond) )                                                                        \
      test_fail ( __FILE__, __LINE__, "CHECK(" #cond ")" );                               \
  } while ( 0 )

#define CHECK_INT_EQ(a, b)                                                                \
  do {                                                                                    \
    long long check_a_ = (a);                                                             \
    long long check_b_ = (b);                                                             \
    test_checks_run++;                                                                    \
    if ( check_a_ != check_b_ )                                                           \
      test_fail_int ( __FILE__, __LINE__, "CHECK_INT_EQ(" #a ", " #b ")",                 \
                      check_a_, check_b_ );                                               \
  } while ( 0 )

#define CHECK_SIZE_EQ(a, b)                                                               \
  do {                                                                                    \
    size_t check_a_ = (a);                                                                \
    size_t check_b_ = (b);                                                                \
    test_checks_run++;                                                                    \
    if ( check_a_ != check_b_ )                                                           \
      test_fail_size ( __FILE__, __LINE__, "CHECK_SIZE_EQ(" #a ", " #b ")",               \
                       check_a_, check_b_ );                                              \
  } while ( 0 )

#define CHECK_STR_EQ(a, b)                                                                \
  do {                                                                                    \
    const char* check_a_ = (a);                                                           \
    const char* check_b_ = (b);                                                           \
    test_checks_run++;                                                                    \
    if ( !test_str_eq( check_a_, check_b_ ) )                                             \
      test_fail_str ( __FILE__, __LINE__, "CHECK_STR_EQ(" #a ", " #b ")",                 \
                      check_a_, check_b_ );                                               \
  } while ( 0 )

#define CHECK_NEAR(a, b, rel_tol)                                                         \
  do {                                                                                    \
    double check_a_   = (a);                                                              \
    double check_b_   = (b);                                                              \
    double check_tol_ = (rel_tol);                                                        \
    test_checks_run++;                                                                    \
    if ( !test_near( check_a_, check_b_, check_tol_ ) )                                   \
      test_fail_near ( __FILE__, __LINE__, "CHECK_NEAR(" #a ", " #b ", " #rel_tol ")",    \
                       check_a_, check_b_, check_tol_ );                                  \
  } while ( 0 )

#define RUN_TEST(fn) test_run ( #fn, fn )

#endif // STEELCHECK_TEST_H
