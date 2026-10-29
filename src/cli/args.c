#include "cli/args.h"

#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  True if arg is the short or the long spelling of an option.
 */

static bool is_option_ ( const char* arg, const char* short_name, const char* long_name )
{
  return strcmp ( arg, short_name ) == 0 || strcmp ( arg, long_name ) == 0;
}

/*
 *  Records the database path, refusing a second one. Returns false with a message on error.
 */

static bool set_db_path_ ( CliOptions* options, const char* path )
{
  if ( path[0] == '\0' )
  {
    snprintf ( options->error, sizeof options->error, "option '--db' needs a FILE" );
    return false;
  }

  if ( options->db_path != nullptr )
  {
    snprintf ( options->error, sizeof options->error, "option '--db' given twice" );
    return false;
  }

  options->db_path = path;

  return true;
}

/*
 *  Checks the number of arguments of the command. Returns false with a message on error.
 */

static bool check_arg_count_ ( CliOptions* options )
{
  switch ( options->command )
  {
    case CLI_COMMAND_NONE:
      if ( !options->help && !options->version )
      {
        snprintf ( options->error, sizeof options->error, "no command given" );
        return false;
      }
      return true;

    case CLI_COMMAND_LIST:
      if ( options->arg_count > 1 )
      {
        snprintf ( options->error, sizeof options->error,
                   "'list' takes at most one FILTER, got %d arguments", options->arg_count );
        return false;
      }
      return true;

    case CLI_COMMAND_SHOW:
      if ( options->arg_count < 1 )
      {
        snprintf ( options->error, sizeof options->error, "'show' needs a DESIGNATION" );
        return false;
      }
      return true;
  }

  return true;
}

//==================================================================================================
//  cli_parse
//==================================================================================================

bool cli_parse ( CliOptions* options, int argc, const char* const argv[] )
{
  *options = (CliOptions) {};

  bool options_ended = false;   // after "--", everything is the command and its arguments
  int  i             = 1;

  for ( ; i < argc; i++ )
  {
    const char* arg = argv[i];

    // Not an option (or no options any more): this is the command.

    if ( options_ended || arg[0] != '-' || arg[1] == '\0' )
    {
      break;
    }

    if ( strcmp( arg, "--" ) == 0 )
    {
      options_ended = true;
    }
    else if ( is_option_( arg, "-h", "--help" ) )
    {
      options->help = true;
    }
    else if ( is_option_( arg, "-V", "--version" ) )
    {
      options->version = true;
    }
    else if ( is_option_( arg, "-q", "--quiet" ) )
    {
      options->quiet = true;
    }
    else if ( is_option_( arg, "-d", "--db" ) )
    {
      // The value is the next argument.

      if ( i + 1 >= argc )
      {
        snprintf ( options->error, sizeof options->error, "option '%s' needs a FILE", arg );
        return false;
      }

      if ( !set_db_path_( options, argv[++i] ) )
      {
        return false;
      }
    }
    else if ( strncmp( arg, "--db=", 5 ) == 0 )
    {
      if ( !set_db_path_( options, arg + 5 ) )
      {
        return false;
      }
    }
    else
    {
      snprintf ( options->error, sizeof options->error, "unknown option '%s'", arg );
      return false;
    }
  }

  // The command and its arguments.

  if ( i < argc )
  {
    const char* command = argv[i];

    if ( strcmp( command, "list" ) == 0 )
    {
      options->command = CLI_COMMAND_LIST;
    }
    else if ( strcmp( command, "show" ) == 0 )
    {
      options->command = CLI_COMMAND_SHOW;
    }
    else
    {
      snprintf ( options->error, sizeof options->error, "unknown command '%s'", command );
      return false;
    }

    options->args      = &argv[i + 1];
    options->arg_count = argc - i - 1;
  }

  return check_arg_count_( options );
}

//==================================================================================================
//  cli_join_args
//==================================================================================================

bool cli_join_args ( const char* const args[], int count, char* dest, size_t size )
{
  if ( dest == nullptr || size == 0 )
  {
    return false;
  }

  size_t used = 0;

  dest[0] = '\0';

  for ( int i = 0; i < count; i++ )
  {
    size_t length = strlen ( args[i] );
    size_t needed = length + ( i > 0 ? 1 : 0 );   // the argument and the space before it

    // Room for the argument, its separator and the final '\0'.

    if ( used + needed >= size )
    {
      dest[0] = '\0';
      return false;
    }

    if ( i > 0 )
    {
      dest[used++] = ' ';
    }

    memcpy ( dest + used, args[i], length );
    used += length;
  }

  dest[used] = '\0';

  return true;
}
