#ifndef STEELCHECK_CLI_ARGS_H
#define STEELCHECK_CLI_ARGS_H

#include "steelcheck/compat.h"

#include <stddef.h>

/*
 *  Command-line parsing of the steelcheck program:
 *
 *      steelcheck [OPTIONS] COMMAND [ARGUMENTS]
 *
 *  Options come before the command; everything after the command belongs to it. "--" ends the
 *  options, so a command can follow it even if it starts with '-'. This is a small parser of our
 *  own, instead of getopt(), which is POSIX and not available on every compiler for Windows.
 */

typedef enum CliCommand : int
{
  CLI_COMMAND_NONE  = 0,   // only --help or --version
  CLI_COMMAND_LIST  = 1,   // list [FILTER]
  CLI_COMMAND_SHOW  = 2,   // show DESIGNATION...
  CLI_COMMAND_CHECK = 3,   // check INPUT
} CliCommand;

/*
 *  Size of the error message, including its '\0'.
 */

constexpr size_t cli_error_size = 160;

typedef struct CliOptions
{
  bool               help;      // -h, --help
  bool               version;   // -V, --version
  bool               quiet;     // -q, --quiet: do not print warnings
  const char*        db_path;     // -d FILE, --db FILE, --db=FILE; nullptr if not given
  const char*        html_path;   // --html FILE, --html=FILE: the check command also writes an
                                  // HTML report there; nullptr if not given

  CliCommand         command;
  const char* const* args;      // the arguments after the command (points into argv)
  int                arg_count;

  char               error[cli_error_size];   // why cli_parse failed
} CliOptions;

/*
 *  Parses argv[1 .. argc-1] into *options. The pointers in options point into argv.
 *
 *  Returns false, with a message in options->error, for an unknown option or command, an option
 *  without its value, an option given twice, --html with a command other than check, no command
 *  (unless --help or --version is given), or a wrong number of arguments for the command.
 *  --help and --version make the command optional.
 */

[[nodiscard]] bool cli_parse ( CliOptions* options, int argc, const char* const argv[] );

/*
 *  Joins args[0 .. count-1] with single spaces into dest (of size bytes), so that
 *  "steelcheck show W 310 x 38,7" (four arguments) means the designation "W 310 x 38,7".
 *  Returns false (dest then holds "") if the result does not fit.
 */

[[nodiscard]] bool cli_join_args ( const char* const args[], int count, char* dest, size_t size );

#endif // STEELCHECK_CLI_ARGS_H
