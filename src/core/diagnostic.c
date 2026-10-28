#include "steelcheck/diagnostic.h"

#include <stdarg.h>
#include <stdio.h>

//==================================================================================================
//  steelcheck_report
//==================================================================================================

void steelcheck_report ( const SteelCheckReporter* reporter, SteelCheckSeverity severity,
                         SteelCheckStatus status, size_t line, size_t column,
                         const char* format, ... )
{
  // Nobody is listening: skip the formatting work too.

  if ( reporter == nullptr || reporter->report == nullptr )
  {
    return;
  }

  SteelCheckDiagnostic diagnostic =
  {
    .severity = severity,
    .status   = status,
    .line     = line,
    .column   = column,
  };

  // The variable arguments (...) are reached through a va_list, which vsnprintf accepts in place
  // of the arguments themselves. vsnprintf never writes more than the array size and always
  // terminates the message, truncating it if necessary.

  va_list args;
  va_start  ( args, format );
  vsnprintf ( diagnostic.message, sizeof diagnostic.message, format, args );
  va_end    ( args );

  reporter->report ( reporter->context, &diagnostic );
}

//==================================================================================================
//  steelcheck_severity_name
//==================================================================================================

const char* steelcheck_severity_name ( SteelCheckSeverity severity )
{
  switch ( severity )
  {
    case STEELCHECK_SEVERITY_WARNING:
      return "warning";

    case STEELCHECK_SEVERITY_ERROR:
      return "error";
  }

  return "unknown severity";
}
