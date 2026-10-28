#ifndef STEELCHECK_DIAGNOSTIC_H
#define STEELCHECK_DIAGNOSTIC_H

#include "steelcheck/compat.h"
#include "steelcheck/status.h"

#include <stddef.h>

/*
 *  Diagnostics: errors and warnings with a location and a message.
 *
 *  The core library never prints. It hands every problem it finds to a reporter, a callback chosen
 *  by the caller: the CLI prints diagnostics, the GUI will show them, and the tests collect them in
 *  an array to check them. This is how C does callbacks: a function pointer plus a void* context
 *  that is passed back to the function, so it can keep state without global variables.
 */

typedef enum SteelCheckSeverity : int
{
  STEELCHECK_SEVERITY_WARNING = 0,   // the input is used anyway (e.g. an unknown column is ignored)
  STEELCHECK_SEVERITY_ERROR   = 1,   // the input is rejected
} SteelCheckSeverity;

/*
 *  Size of the message, including its '\0'. Longer messages are truncated (safely, by vsnprintf).
 */

constexpr size_t steelcheck_diagnostic_message_size = 160;

typedef struct SteelCheckDiagnostic
{
  SteelCheckSeverity severity;
  SteelCheckStatus   status;      // the kind of problem, e.g. STEELCHECK_ERROR_HEADER
  size_t             line;        // 1-based line number, 0 if not applicable
  size_t             column;      // 1-based CSV column number, 0 if not applicable
  char               message[steelcheck_diagnostic_message_size];
} SteelCheckDiagnostic;

typedef struct SteelCheckReporter
{
  void  (*report) ( void* context, const SteelCheckDiagnostic* diagnostic );
  void*   context;               // passed back unchanged to report()
} SteelCheckReporter;

/*
 *  Builds a diagnostic with a printf-style message and hands it to the reporter. A nullptr
 *  reporter, or one whose report is nullptr, ignores the diagnostic. The diagnostic lives only
 *  during the call: a reporter that keeps it must copy it.
 */

STEELCHECK_PRINTF ( 6, 7 )
void steelcheck_report ( const SteelCheckReporter* reporter, SteelCheckSeverity severity,
                         SteelCheckStatus status, size_t line, size_t column,
                         const char* format, ... );

/*
 *  "warning" or "error".
 */

[[nodiscard]] const char* steelcheck_severity_name ( SteelCheckSeverity severity );

#endif // STEELCHECK_DIAGNOSTIC_H
