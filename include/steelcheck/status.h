#ifndef STEELCHECK_STATUS_H
#define STEELCHECK_STATUS_H

#include "steelcheck/compat.h"

/*
 *  Status codes shared by the whole core library.
 */

enum SteelCheckStatus : int
{
  STEELCHECK_OK                     = 0,
  STEELCHECK_ERROR_OPEN             = 1,
  STEELCHECK_ERROR_READ             = 2,
  STEELCHECK_ERROR_TOO_LARGE        = 3,
  STEELCHECK_ERROR_NO_MEMORY        = 4,
  STEELCHECK_ERROR_OVERFLOW         = 5,
  STEELCHECK_ERROR_INVALID_ARGUMENT = 6,
  STEELCHECK_ERROR_SYNTAX           = 7,  // malformed CSV line: quote, '\0' byte, wrong field count
  STEELCHECK_ERROR_HEADER           = 8,  // CSV header: missing, duplicate or empty column name
  STEELCHECK_ERROR_MISSING_VALUE    = 9,  // an empty field where a value was expected
  STEELCHECK_ERROR_NUMBER           = 10, // a field that is not a valid number
  STEELCHECK_ERROR_RANGE            = 11, // a number too large or too small for a double
  STEELCHECK_ERROR_VALUE            = 12, // a well-formed but not allowed value, e.g. family "X"
  STEELCHECK_ERROR_INCONSISTENT     = 13, // values that disagree, e.g. Wx far from 2 Ix / d
};

typedef enum SteelCheckStatus SteelCheckStatus;

/*
 *  Function to return a short English message for a status code.
 */

[[nodiscard]] const char* steelcheck_status_message ( SteelCheckStatus status );

#endif // STEELCHECK_STATUS_H
