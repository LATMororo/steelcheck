#include "steelcheck/compat.h"
#include "steelcheck/version.h"

#include <stdio.h>
#include <string.h>

/*
 *  Process exit codes. The underlying type is fixed to int (C23), the type main() returns.
 */

enum steelcheck_exit : int
{
  STEELCHECK_EXIT_SUCCESS       = 0,
  STEELCHECK_EXIT_RUNTIME_ERROR = 1,
  STEELCHECK_EXIT_USAGE_ERROR   = 2,
};

//==================================================================================================
//  is_option_
//==================================================================================================

/*
 *  True if arg matches the short or the long spelling of an option. Each option is spelled
 *  only once, in the calls in main(), so the code and the help text are easy to keep in sync.
 */

static bool is_option_ ( const char* arg, const char* short_name, const char* long_name )
{
  return strcmp ( arg, short_name ) == 0 || strcmp ( arg, long_name ) == 0;
}

//==================================================================================================
//  print_usage_
//==================================================================================================

static void print_usage_ ( FILE* stream )
{
  fputs ( "Usage: steelcheck [OPTION]\n"
          "Check W and HP steel cross sections (ABNT NBR 8800:2024).\n"
          "\n"
          "Options:\n"
          "  -h, --help       show this help and exit\n"
          "  -V, --version    show version and build information and exit\n",
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

  // Only one option is accepted for now, so every argument after the first is an error,
  // whether the first one is valid or not.

  if ( argc > 2 )
  {
    for ( int i = 2; i < argc; i++ )
      fprintf ( stderr, "steelcheck: unexpected extra argument '%s'\n", argv[i] );

    print_try_help_ ();

    return STEELCHECK_EXIT_USAGE_ERROR;
  }

  // Exactly one argument.

  const char* arg = argv[1];

  if ( is_option_( arg, "-h", "--help" ) )
  {
    print_usage_ ( stdout );

    return STEELCHECK_EXIT_SUCCESS;
  }

  if ( is_option_( arg, "-V", "--version" ) )
  {
    print_version_ ();

    return STEELCHECK_EXIT_SUCCESS;
  }

  fprintf ( stderr, "steelcheck: unknown option '%s'\n", arg );
  print_try_help_ ();

  return STEELCHECK_EXIT_USAGE_ERROR;
}
