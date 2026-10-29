#ifndef STEELCHECK_CLI_COMMANDS_H
#define STEELCHECK_CLI_COMMANDS_H

#include "steelcheck/compat.h"
#include "steelcheck/database.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/input.h"
#include "steelcheck/status.h"

#include <stddef.h>
#include <stdio.h>

/*
 *  The commands of the steelcheck program. They write to the stream they are given (stdout in the
 *  program, a temporary file in the tests) and never exit: main() decides the exit code.
 */

/*
 *  Where the section database comes from.
 */

typedef enum CliSource : int
{
  CLI_SOURCE_NONE    = 0,   // nothing available
  CLI_SOURCE_FILE    = 1,   // a CSV file
  CLI_SOURCE_BUILTIN = 2,   // the built-in database (builtin.h)
} CliSource;

/*
 *  Chooses the database, in this order: the --db option, the STEELCHECK_DB environment variable,
 *  the built-in database (if builtin_available). An empty string counts as not given. *name
 *  receives the file path, steelcheck_builtin_source ("<built-in>"), or nullptr for none.
 */

[[nodiscard]] CliSource cli_choose_source ( const char* option, const char* environment,
                                            bool builtin_available, const char** name );

/*
 *  Context of cli_print_diagnostic: where to print, the name of the source (the database path),
 *  and whether warnings are hidden.
 */

typedef struct CliPrinter
{
  FILE*       stream;
  const char* source;
  bool        quiet;
} CliPrinter;

/*
 *  Reporter callback (context: a CliPrinter). Prints one diagnostic as
 *  "source:line:column: severity: message", leaving out a line or column of 0, e.g.
 *  "data/gerdau_w_hp.csv:113: warning: W610x153.0: mass_kg_m 154.2 differs ...".
 */

void cli_print_diagnostic ( void* context, const SteelCheckDiagnostic* diagnostic );

/*
 *  Prints a table of the sections whose designation starts with filter (compared like
 *  designations: ignoring spaces and case, ',' as '.'), or of all sections if filter is nullptr,
 *  followed by a summary line. Returns the number of sections printed.
 */

size_t cli_list ( FILE* out, const SteelCheckDatabase* db, const char* filter );

/*
 *  Prints every property of the section with the given designation, in the catalog's units, and
 *  where it comes from (source and line). Returns false, printing nothing, if there is no such
 *  section.
 */

[[nodiscard]] bool cli_show ( FILE* out, const SteelCheckDatabase* db, const char* designation,
                              const char* source );

/*
 *  What the check command found.
 */

typedef struct CliCheckSummary
{
  size_t members;   // members checked
  size_t failed;    // members with at least one check that FAILs
} CliCheckSummary;

/*
 *  Checks every member of input against its code. Writes the text report of each member to out,
 *  then a table with the governing check of each member, and the totals; if html is not nullptr,
 *  also writes a whole HTML page (titled title) to it. Returns STEELCHECK_OK and fills *summary,
 *  or the status of the first member that could not be checked (out of memory; the input was
 *  validated on loading).
 */

[[nodiscard]] SteelCheckStatus cli_check ( FILE* out, FILE* html, const SteelCheckInput* input,
                                           const char* title, const SteelCheckReporter* reporter,
                                           CliCheckSummary* summary );

#endif // STEELCHECK_CLI_COMMANDS_H
