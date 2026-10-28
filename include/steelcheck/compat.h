#ifndef STEELCHECK_COMPAT_H
#define STEELCHECK_COMPAT_H

/*
 *  C23 check and platform detection.
 *
 *  C23 is 202311L, but GCC 14 still reports the draft value 202000L in -std=c23 mode,
 *  so anything newer than C17 (201710L) is accepted.
 */

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ <= 201710L)
#  error "steelcheck requires a C23 compiler; compile in -std=c23 mode"
#endif

/*
 *  Platform macros for Windows, or any other system (assumed POSIX).
 */

#if defined(_WIN32)
#  define STEELCHECK_WINDOWS 1
#  define STEELCHECK_POSIX   0
#else
#  define STEELCHECK_WINDOWS 0
#  define STEELCHECK_POSIX   1
#endif

/*
 *  STEELCHECK_PRINTF(format_index, first_arg_index) marks a printf-style function, so the compiler
 *  checks its calls like calls to printf (argument types against the % conversions). The indices
 *  count the function's parameters from 1. GCC and Clang support the C23 attribute gnu::format;
 *  other compilers get nothing.
 */

#if defined(__has_c_attribute)
#  if __has_c_attribute(gnu::format)
#    define STEELCHECK_PRINTF(format_index, first_arg_index) \
       [[gnu::format(printf, format_index, first_arg_index)]]
#  endif
#endif

#ifndef STEELCHECK_PRINTF
#  define STEELCHECK_PRINTF(format_index, first_arg_index)
#endif

#endif // STEELCHECK_COMPAT_H
