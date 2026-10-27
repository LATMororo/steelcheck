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

#endif // STEELCHECK_COMPAT_H
