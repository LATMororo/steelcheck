#include "steelcheck/compat.h"
#include "steelcheck/builtin.h"
#include "steelcheck/database.h"
#include "steelcheck/version.h"

#include "cli/args.h"
#include "cli/commands.h"

#include <stdio.h>
#include <stdlib.h>

/*
 *  Process exit codes. The underlying type is fixed to int (C23), the type main() returns.
 */

enum steelcheck_exit : int
{
  STEELCHECK_EXIT_SUCCESS       = 0,
  STEELCHECK_EXIT_RUNTIME_ERROR = 1,
  STEELCHECK_EXIT_USAGE_ERROR   = 2,
};

/*
 *  Longest designation accepted by the show command, including its '\0'.
 */

constexpr size_t designation_size_ = 128;

//==================================================================================================
//  print_usage_
//==================================================================================================

static void print_usage_ ( FILE* stream )
{
  fputs ( "Usage: steelcheck [OPTIONS] COMMAND [ARGUMENTS]\n"
          "Check W and HP steel cross sections (ABNT NBR 8800:2024).\n"
          "\n"
          "Commands:\n"
          "  list [FILTER]        list the sections whose designation starts with FILTER\n"
          "                       (e.g. W310, HP); all sections without FILTER\n"
          "  show DESIGNATION     show every property of one section, e.g. W310x38.7\n"
          "                       (spaces, lower case and a decimal comma are accepted)\n"
          "\n"
          "Options:\n"
          "  -d, --db FILE        section database (CSV); default: $STEELCHECK_DB, else the\n"
          "                       built-in Gerdau catalog (if this build has one)\n"
          "  -q, --quiet          do not print warnings about the database\n"
          "  -h, --help           show this help and exit\n"
          "  -V, --version        show version and build information and exit\n",
          stream );
}

//==================================================================================================
//  print_try_help_
//==================================================================================================

static void print_try_help_ ()
{
  fputs ( "Try 'steelcheck --help'.\n", stderr );
}

//==================================================================================================
//  print_version_
//==================================================================================================

static void print_version_ ()
{
  printf ( "steelcheck %s\n", steelcheck_version() );
  printf ( "%s\n", steelcheck_build_info() );
  printf ( "built-in section database: %s\n",
           steelcheck_builtin_available() ? "yes" : "no (the compiler has no #embed)" );
}

//==================================================================================================
//  run_command_
//==================================================================================================

/*
 *  Runs the parsed command on the loaded database and returns the exit code.
 */

static int run_command_ ( const CliOptions* options, const SteelCheckDatabase* db,
                          const char* source )
{
  switch ( options->command )
  {
    case CLI_COMMAND_NONE:
      return STEELCHECK_EXIT_SUCCESS;

    case CLI_COMMAND_LIST:
    {
      const char* filter = options->arg_count > 0 ? options->args[0] : nullptr;

      if ( cli_list( stdout, db, filter ) == 0 && filter != nullptr )
      {
        fprintf ( stderr, "steelcheck: no section matches '%s'\n", filter );
        return STEELCHECK_EXIT_RUNTIME_ERROR;
      }

      return STEELCHECK_EXIT_SUCCESS;
    }

    case CLI_COMMAND_SHOW:
    {
      // "steelcheck show W 310 x 38,7" arrives as four arguments: they form one designation.

      char designation[designation_size_];

      if ( !cli_join_args( options->args, options->arg_count, designation, sizeof designation ) )
      {
        fprintf ( stderr, "steelcheck: designation too long\n" );
        return STEELCHECK_EXIT_USAGE_ERROR;
      }

      if ( !cli_show( stdout, db, designation, source ) )
      {
        fprintf ( stderr, "steelcheck: no section '%s' in %s\n", designation, source );
        fputs ( "Try 'steelcheck list' to see the sections.\n", stderr );
        return STEELCHECK_EXIT_RUNTIME_ERROR;
      }

      return STEELCHECK_EXIT_SUCCESS;
    }
  }

  return STEELCHECK_EXIT_SUCCESS;
}

//==================================================================================================
//  main
//==================================================================================================

int main ( int argc, char* argv[] )
{
  // No argument: show how to use the program.

  if ( argc < 2 )
  {
    print_usage_ ( stderr );

    return STEELCHECK_EXIT_USAGE_ERROR;
  }

  // argv is char*[], the parser takes const char* const[]: C does not convert the pointer types
  // implicitly (even though the parser only promises not to change anything), so it is cast.

  CliOptions options;

  if ( !cli_parse( &options, argc, (const char* const*) argv ) )
  {
    fprintf ( stderr, "steelcheck: %s\n", options.error );
    print_try_help_ ();

    return STEELCHECK_EXIT_USAGE_ERROR;
  }

  if ( options.help )
  {
    print_usage_ ( stdout );

    return STEELCHECK_EXIT_SUCCESS;
  }

  if ( options.version )
  {
    print_version_ ();

    return STEELCHECK_EXIT_SUCCESS;
  }

  // The database: --db, else the STEELCHECK_DB environment variable, else the built-in one.

  const char* source = nullptr;
  CliSource   kind   = cli_choose_source ( options.db_path, getenv( "STEELCHECK_DB" ),
                                           steelcheck_builtin_available(), &source );

  if ( kind == CLI_SOURCE_NONE )
  {
    fputs ( "steelcheck: no section database; use '--db FILE' or set STEELCHECK_DB\n"
            "(this build has no built-in database: its compiler has no #embed)\n", stderr );

    return STEELCHECK_EXIT_USAGE_ERROR;
  }

  CliPrinter         printer  = { .stream = stderr, .source = source, .quiet = options.quiet };
  SteelCheckReporter reporter = { .report = cli_print_diagnostic, .context = &printer };
  SteelCheckDatabase db       = {};
  SteelCheckStatus   status   = kind == CLI_SOURCE_FILE
                              ? steelcheck_database_load_file( &db, source, &reporter )
                              : steelcheck_database_load_builtin( &db, &reporter );

  if ( status != STEELCHECK_OK )
  {
    fprintf ( stderr, "steelcheck: cannot use the section database %s\n", source );

    return STEELCHECK_EXIT_RUNTIME_ERROR;
  }

  int code = run_command_ ( &options, &db, source );

  steelcheck_database_free ( &db );

  return code;
}
