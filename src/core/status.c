#include "steelcheck/status.h"

//==================================================================================================
//  steelcheck_status_message
//==================================================================================================

const char* steelcheck_status_message ( SteelCheckStatus status )
{
  // Handle the status cases.

  switch ( status )
  {
    case STEELCHECK_OK:
      return "ok";

    case STEELCHECK_ERROR_OPEN:
      return "cannot open file";

    case STEELCHECK_ERROR_READ:
      return "read error";

    case STEELCHECK_ERROR_TOO_LARGE:
      return "file too large";

    case STEELCHECK_ERROR_NO_MEMORY:
      return "out of memory";

    case STEELCHECK_ERROR_OVERFLOW:
      return "size overflow";

    case STEELCHECK_ERROR_INVALID_ARGUMENT:
      return "invalid argument";

    case STEELCHECK_ERROR_SYNTAX:
      return "syntax error";

    case STEELCHECK_ERROR_HEADER:
      return "invalid header";

    case STEELCHECK_ERROR_MISSING_VALUE:
      return "missing value";

    case STEELCHECK_ERROR_NUMBER:
      return "invalid number";

    case STEELCHECK_ERROR_RANGE:
      return "number of out of range";
  }

  return "unknown status";
}
