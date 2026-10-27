#ifndef STEELCHECK_VERSION_H
#define STEELCHECK_VERSION_H

//#include "steelcheck/compat.h"

/*
 *  Version numbers, written only here. They are macros because the preprocessor,
 *  which builds the version string in version.c, cannot see constexpr objects.
 */

#define STEELCHECK_VERSION_MAJOR 0
#define STEELCHECK_VERSION_MINOR 1
#define STEELCHECK_VERSION_PATCH 0

/*
 *  Version of the core library as "MAJOR.MINOR.PATCH", e.g. "0.1.0".
 */

const char* steelcheck_version ();

/*
 *  Compiler and C standard used for the build, e.g. "Clang 21.1.0, C standard 202311L".
 */

const char* steelcheck_build_info ();

#endif // STEELCHECK_VERSION_H
