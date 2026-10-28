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
};

typedef enum SteelCheckStatus SteelCheckStatus;

/*
 *  Function to return a short English message for a status code.
 */

[[nodiscard]] const char* steelcheck_status_message ( SteelCheckStatus status );

#endif // STEELCHECK_STATUS_H
