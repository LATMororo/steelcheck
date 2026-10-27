#include "steelcheck/compat.h"
#include "steelcheck/version.h"

/*
 *  Helper macros to turn a macro's value into a string literal. Two levels are needed:
 *  the outer one expands its argument (STEELCHECK_VERSION_MAJOR -> 0) before the inner
 *  one applies # to it (0 -> "0").
 */

#define STEELCHECK_STR_HELPER(x) #x
#define STEELCHECK_STR(x) STEELCHECK_STR_HELPER(x)

#define STEELCHECK_VERSION_STRING \
  STEELCHECK_STR(STEELCHECK_VERSION_MAJOR) "." \
  STEELCHECK_STR(STEELCHECK_VERSION_MINOR) "." \
  STEELCHECK_STR(STEELCHECK_VERSION_PATCH)

/*
 *  Compiler name and version. Clang is tested first because it also defines __GNUC__.
 *  __clang_version__ and __VERSION__ are compiler extensions, not standard C.
 */

#if defined(__clang__)
#  define STEELCHECK_COMPILER "Clang " __clang_version__
#elif defined(__GNUC__)
#  define STEELCHECK_COMPILER "GCC " __VERSION__
#else
#  define STEELCHECK_COMPILER "unknown compiler"
#endif

//==================================================================================================
//  steelcheck_version
//==================================================================================================

const char* steelcheck_version ()
{
  return STEELCHECK_VERSION_STRING;
}

//==================================================================================================
//  steelcheck_build_info
//==================================================================================================

const char* steelcheck_build_info ()
{
  return STEELCHECK_COMPILER ", C standard " STEELCHECK_STR(__STDC_VERSION__);
}
