#include "steelcheck/compat.h"
#include "steelcheck/builtin.h"
#include "steelcheck/database.h"
#include "steelcheck/input.h"
#include "suites.h"
#include "test.h"

#include "cli/args.h"
#include "cli/commands.h"

#include <stdio.h>
#include <string.h>

/*
 *  Number of arguments of an argv array written in a test, without its final nullptr (a real
 *  argv also ends with argv[argc] == nullptr).
 */

#define ARGC_(argv) ( (int) ( sizeof argv / sizeof argv[0] ) - 1 )

//==================================================================================================
//  Capturing output
//==================================================================================================

/*
 *  Size of the buffer that receives a command's output.
 */

constexpr size_t output_size_ = 16384;

/*
 *  Reads everything written to file into text (of size bytes) and closes file. tmpfile() gives a
 *  temporary file that is deleted when closed: commands write to it like to stdout, and the test
 *  reads it back. Returns false if the file is missing or the output does not fit.
 */

static bool read_back_ ( FILE* file, char* text, size_t size )
{
  text[0] = '\0';

  if ( file == nullptr )
  {
    fprintf ( stderr, "    tmpfile() failed\n" );
    return false;
  }

  rewind ( file );

  size_t length = fread ( text, 1, size - 1, file );
  bool   fits   = feof ( file ) || fgetc ( file ) == EOF;

  text[length] = '\0';
  fclose ( file );

  return fits;
}

/*
 *  True if text contains part. Prints the text otherwise.
 */

static bool contains_ ( const char* text, const char* part )
{
  if ( strstr( text, part ) != nullptr )
  {
    return true;
  }

  fprintf ( stderr, "    \"%s\" not found in:\n%s\n", part, text );
  return false;
}

/*
 *  The Gerdau database, loaded once for the command tests.
 */

static bool load_gerdau_ ( SteelCheckDatabase* db )
{
  return steelcheck_database_load_file( db, "data/gerdau_w_hp.csv", nullptr ) == STEELCHECK_OK;
}

//==================================================================================================
//  Parsing
//==================================================================================================

static void test_parse_list ()
{
  const char* argv[] = { "steelcheck", "-q", "--db", "my.csv", "list", "W310", nullptr };
  CliOptions  options;

  CHECK ( cli_parse( &options, ARGC_( argv ), argv ) );
  CHECK ( options.quiet && !options.help && !options.version );
  CHECK_STR_EQ ( options.db_path, "my.csv" );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_LIST );
  CHECK_INT_EQ ( options.arg_count, 1 );
  CHECK_STR_EQ ( options.args[0], "W310" );
}

static void test_parse_show ()
{
  // --db=FILE, and a designation typed with spaces: four arguments after the command.

  const char* argv[] = { "steelcheck", "--db=my.csv", "show", "W", "310", "x", "38,7", nullptr };
  CliOptions  options;
  char        designation[64];

  CHECK ( cli_parse( &options, ARGC_( argv ), argv ) );
  CHECK_STR_EQ ( options.db_path, "my.csv" );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_SHOW );
  CHECK_INT_EQ ( options.arg_count, 4 );

  CHECK ( cli_join_args( options.args, options.arg_count, designation, sizeof designation ) );
  CHECK_STR_EQ ( designation, "W 310 x 38,7" );

  // Options come before the command: after it, "-q" is an argument of the command.

  const char* late[] = { "steelcheck", "show", "W310x38.7", "-q", nullptr };

  CHECK ( cli_parse( &options, ARGC_( late ), late ) );
  CHECK ( !options.quiet );
  CHECK_INT_EQ ( options.arg_count, 2 );
}

static void test_parse_check ()
{
  const char* argv[] = { "steelcheck", "--html", "out.html", "check", "frame.toml", nullptr };
  const char* eq[]   = { "steelcheck", "--html=out.html", "check", "frame.toml", nullptr };
  CliOptions  options;

  CHECK ( cli_parse( &options, ARGC_( argv ), argv ) );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_CHECK );
  CHECK_STR_EQ ( options.html_path, "out.html" );
  CHECK_INT_EQ ( options.arg_count, 1 );
  CHECK_STR_EQ ( options.args[0], "frame.toml" );

  CHECK ( cli_parse( &options, ARGC_( eq ), eq ) );
  CHECK_STR_EQ ( options.html_path, "out.html" );

  // Without --html: no HTML report.

  const char* plain[] = { "steelcheck", "check", "frame.toml", nullptr };

  CHECK ( cli_parse( &options, ARGC_( plain ), plain ) );
  CHECK ( options.html_path == nullptr );
}

static void test_parse_help_version ()
{
  const char* help[]    = { "steelcheck", "--help", nullptr };
  const char* version[] = { "steelcheck", "-V", nullptr };
  const char* both[]    = { "steelcheck", "-h", "list", nullptr };
  CliOptions  options;

  // --help and --version need no command.

  CHECK ( cli_parse( &options, ARGC_( help ), help ) );
  CHECK ( options.help );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_NONE );

  CHECK ( cli_parse( &options, ARGC_( version ), version ) );
  CHECK ( options.version );

  CHECK ( cli_parse( &options, ARGC_( both ), both ) );
  CHECK ( options.help );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_LIST );
}

static void test_parse_double_dash ()
{
  // "--" ends the options: what follows is the command, even if it starts with '-'.

  const char* list[]  = { "steelcheck", "--", "list", nullptr };
  const char* dash[]  = { "steelcheck", "--", "-q", nullptr };
  CliOptions  options;

  CHECK ( cli_parse( &options, ARGC_( list ), list ) );
  CHECK_INT_EQ ( options.command, CLI_COMMAND_LIST );

  CHECK ( !cli_parse( &options, ARGC_( dash ), dash ) );
  CHECK_STR_EQ ( options.error, "unknown command '-q'" );
}

/*
 *  True if argv is rejected with the expected error message.
 */

static bool rejected_ ( int argc, const char* const argv[], const char* expected )
{
  CliOptions options;

  if ( cli_parse( &options, argc, argv ) )
  {
    fprintf ( stderr, "    accepted, expected \"%s\"\n", expected );
    return false;
  }

  if ( strcmp( options.error, expected ) != 0 )
  {
    fprintf ( stderr, "    error \"%s\", expected \"%s\"\n", options.error, expected );
    return false;
  }

  return true;
}

static void test_parse_errors ()
{
  const char* none[]     = { "steelcheck", "-q", nullptr };
  const char* option[]   = { "steelcheck", "--frobnicate", "list", nullptr };
  const char* command[]  = { "steelcheck", "draw", nullptr };
  const char* no_file[]  = { "steelcheck", "list", "--db", nullptr };
  const char* missing[]  = { "steelcheck", "--db", nullptr };
  const char* empty[]    = { "steelcheck", "--db=", "list", nullptr };
  const char* twice[]    = { "steelcheck", "-d", "a.csv", "--db=b.csv", "list", nullptr };
  const char* no_name[]  = { "steelcheck", "show", nullptr };
  const char* too_many[] = { "steelcheck", "list", "W150", "W200", nullptr };
  const char* no_input[] = { "steelcheck", "check", nullptr };
  const char* inputs[]   = { "steelcheck", "check", "a.toml", "b.toml", nullptr };
  const char* no_html[]  = { "steelcheck", "check", "a.toml", "--html", nullptr };
  const char* html_end[] = { "steelcheck", "--html", nullptr };
  const char* html_two[] = { "steelcheck", "--html=a", "--html", "b", "check", "x", nullptr };
  const char* html_cmd[] = { "steelcheck", "--html", "a.html", "list", nullptr };

  CHECK ( rejected_( ARGC_( none ),     none,     "no command given" ) );
  CHECK ( rejected_( ARGC_( option ),   option,   "unknown option '--frobnicate'" ) );
  CHECK ( rejected_( ARGC_( command ),  command,  "unknown command 'draw'" ) );
  CHECK ( rejected_( ARGC_( missing ),  missing,  "option '--db' needs a FILE" ) );
  CHECK ( rejected_( ARGC_( empty ),    empty,    "option '--db' needs a FILE" ) );
  CHECK ( rejected_( ARGC_( twice ),    twice,    "option '--db' given twice" ) );
  CHECK ( rejected_( ARGC_( no_name ),  no_name,  "'show' needs a DESIGNATION" ) );
  CHECK ( rejected_( ARGC_( too_many ), too_many,
                     "'list' takes at most one FILTER, got 2 arguments" ) );
  CHECK ( rejected_( ARGC_( no_input ), no_input,
                     "'check' needs exactly one INPUT file, got 0 arguments" ) );
  CHECK ( rejected_( ARGC_( inputs ),   inputs,
                     "'check' needs exactly one INPUT file, got 2 arguments" ) );
  CHECK ( rejected_( ARGC_( no_html ),  no_html,
                     "'check' needs exactly one INPUT file, got 2 arguments" ) );
  CHECK ( rejected_( ARGC_( html_end ), html_end, "option '--html' needs a FILE" ) );
  CHECK ( rejected_( ARGC_( html_two ), html_two, "option '--html' given twice" ) );
  CHECK ( rejected_( ARGC_( html_cmd ), html_cmd, "option '--html' works only with 'check'" ) );

  // After the command, "--db" is just an argument: here a filter of list.

  CliOptions options;

  CHECK ( cli_parse( &options, ARGC_( no_file ), no_file ) );
  CHECK_STR_EQ ( options.args[0], "--db" );
}

static void test_join_args ()
{
  const char* args[] = { "W", "310", "x", "38,7" };
  char        text[13];   // "W 310 x 38,7" is 12 characters

  CHECK ( cli_join_args( args, 4, text, sizeof text ) );
  CHECK_STR_EQ ( text, "W 310 x 38,7" );

  CHECK ( !cli_join_args( args, 4, text, sizeof text - 1 ) );   // one byte short
  CHECK_STR_EQ ( text, "" );

  CHECK ( cli_join_args( args, 0, text, sizeof text ) );
  CHECK_STR_EQ ( text, "" );

  CHECK ( !cli_join_args( args, 1, nullptr, 8 ) );
}

//==================================================================================================
//  Choosing the database
//==================================================================================================

static void test_choose_source ()
{
  const char* name = nullptr;

  // --db wins over the environment variable, which wins over the built-in database.

  CHECK_INT_EQ ( cli_choose_source( "a.csv", "b.csv", true, &name ), CLI_SOURCE_FILE );
  CHECK_STR_EQ ( name, "a.csv" );

  CHECK_INT_EQ ( cli_choose_source( nullptr, "b.csv", true, &name ), CLI_SOURCE_FILE );
  CHECK_STR_EQ ( name, "b.csv" );

  CHECK_INT_EQ ( cli_choose_source( nullptr, nullptr, true, &name ), CLI_SOURCE_BUILTIN );
  CHECK_STR_EQ ( name, "<built-in>" );

  // Empty strings count as not given (e.g. STEELCHECK_DB= in the shell).

  CHECK_INT_EQ ( cli_choose_source( "", "", true, &name ), CLI_SOURCE_BUILTIN );

  // Nothing at all: a build without #embed and no file.

  CHECK_INT_EQ ( cli_choose_source( nullptr, "", false, &name ), CLI_SOURCE_NONE );
  CHECK ( name == nullptr );
}

//==================================================================================================
//  Printing diagnostics
//==================================================================================================

static void test_print_diagnostic ()
{
  char       text[1024];
  FILE*      file    = tmpfile ();
  CliPrinter printer = { .stream = file, .source = "data.csv", .quiet = false };

  if ( file == nullptr )
  {
    CHECK ( file != nullptr );
    return;
  }

  SteelCheckReporter reporter = { .report = cli_print_diagnostic, .context = &printer };

  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_NUMBER, 12, 6,
                      "column 'd_mm': invalid number '12mm'" );
  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_INCONSISTENT, 113,
                      0, "W610x153.0: mass" );
  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_OPEN, 0, 0,
                      "cannot open file" );

  // Quiet: warnings are hidden, errors are not.

  printer.quiet = true;

  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_OK, 1, 0, "hidden" );
  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_OK, 2, 0, "shown" );

  CHECK ( read_back_( file, text, sizeof text ) );
  CHECK_STR_EQ ( text, "data.csv:12:6: error: column 'd_mm': invalid number '12mm'\n"
                       "data.csv:113: warning: W610x153.0: mass\n"
                       "data.csv: error: cannot open file\n"
                       "data.csv:2: error: shown\n" );
}

//==================================================================================================
//  The list command
//==================================================================================================

/*
 *  Runs cli_list with filter on db, returns the number of sections it printed, and its output in
 *  text.
 */

static size_t list_ ( const SteelCheckDatabase* db, const char* filter, char* text, size_t size )
{
  FILE*  file    = tmpfile ();
  size_t printed = file != nullptr ? cli_list( file, db, filter ) : 0;

  CHECK ( read_back_( file, text, size ) );

  return printed;
}

static void test_list ()
{
  static char        text[output_size_];
  SteelCheckDatabase db = {};

  CHECK ( load_gerdau_( &db ) );

  CHECK_SIZE_EQ ( list_( &db, nullptr, text, sizeof text ), 108 );
  CHECK ( contains_( text, "108 of 108 sections - Gerdau" ) );
  CHECK ( contains_( text, ", edition 08/25\n" ) );

  // The header: symbols, then units, from the section module.

  CHECK ( contains_( text, "designation  family         m         d        bf" ) );
  CHECK ( contains_( text, "kg/m        mm        mm        mm        mm      cm^2      cm^4" ) );

  // Filters are compared like designations: spaces, case and ',' do not matter.

  CHECK_SIZE_EQ ( list_( &db, "W310", text, sizeof text ), 18 );
  CHECK ( contains_( text, "W310x38.7    W           38.7       310       165       5.8       9.7"
                           "      49.7      8581       727\n" ) );
  CHECK ( contains_( text, "18 of 108 sections" ) );

  CHECK_SIZE_EQ ( list_( &db, "hp", text, sizeof text ), 8 );
  CHECK_SIZE_EQ ( list_( &db, "HP 3", text, sizeof text ), 5 );
  CHECK_SIZE_EQ ( list_( &db, "w 310 x 38,7", text, sizeof text ), 1 );

  // Nothing matches: only the header and the summary.

  CHECK_SIZE_EQ ( list_( &db, "XYZ", text, sizeof text ), 0 );
  CHECK ( contains_( text, "0 of 108 sections" ) );

  steelcheck_database_free ( &db );
}

//==================================================================================================
//  The show command
//==================================================================================================

/*
 *  Runs cli_show for designation on db; returns its result, and its output in text.
 */

static bool show_ ( const SteelCheckDatabase* db, const char* designation, char* text,
                    size_t size )
{
  FILE* file  = tmpfile ();
  bool  found = file != nullptr && cli_show( file, db, designation, "data/gerdau_w_hp.csv" );

  CHECK ( read_back_( file, text, size ) );

  return found;
}

static void test_show ()
{
  static char        text[output_size_];
  SteelCheckDatabase db = {};

  CHECK ( load_gerdau_( &db ) );

  CHECK ( show_( &db, "W 310 x 38,7", text, sizeof text ) );

  // Identity and origin.

  CHECK ( contains_( text, "W310x38.7\n  family W, imperial designation W12x26\n" ) );
  CHECK ( contains_( text, "  from data/gerdau_w_hp.csv, line 51\n" ) );

  // Values in the catalog's units, computed ones marked.

  CHECK ( contains_( text, "    d              310  mm     total depth\n" ) );
  CHECK ( contains_( text, "    r               10  mm     fillet radius (computed)\n" ) );
  CHECK ( contains_( text, "    Ix            8581  cm^4   second moment of area, x\n" ) );
  CHECK ( contains_( text, "    Cw          163728  cm^6   warping constant\n" ) );
  CHECK ( contains_( text, "    bf/2tf       8.505         flange slenderness (computed)\n" ) );
  CHECK ( contains_( text, "    h0           300.3  mm     distance between flange centroids\n" ) );
  CHECK ( contains_( text, "  Catalog marks: none\n" ) );

  // Catalog marks of an (H) section produced on request.

  CHECK ( show_( &db, "W200x100.0", text, sizeof text ) );
  CHECK ( contains_( text, "Catalog marks: wide-flange H shape (H), produced on request (*)\n" ) );

  // Not found: false, and nothing is printed.

  CHECK ( !show_( &db, "W999x1.0", text, sizeof text ) );
  CHECK_STR_EQ ( text, "" );

  steelcheck_database_free ( &db );
}

//==================================================================================================
//  check
//==================================================================================================

static void test_check ()
{
  static char        text[output_size_ * 4];
  static char        html[output_size_ * 8];
  SteelCheckDatabase db      = {};
  SteelCheckInput    input   = {};
  CliCheckSummary    summary = {};

  CHECK ( load_gerdau_( &db ) );
  CHECK_INT_EQ ( steelcheck_input_load_file( &input, "examples/members.toml", &db, nullptr ),
                 STEELCHECK_OK );

  FILE* out  = tmpfile ();
  FILE* page = tmpfile ();

  if ( out != nullptr && page != nullptr )
  {
    CHECK_INT_EQ ( cli_check( out, page, &input, "members", nullptr, &summary ), STEELCHECK_OK );
  }

  CHECK ( read_back_( out, text, sizeof text ) );
  CHECK ( read_back_( page, html, sizeof html ) );

  // The example's members all pass.

  CHECK_SIZE_EQ ( summary.members, 5 );
  CHECK_SIZE_EQ ( summary.failed, 0 );

  // The text: a report per member, then the table of members and the totals.

  CHECK ( contains_( text, "C1: W310x52.0, ASTM A572 Gr.50, ABNT NBR 8800\n" ) );
  CHECK ( contains_( text, "D1: W150x22.5, ASTM A36, ABNT NBR 8800\n" ) );
  CHECK ( contains_( text, "  V2               W310x38.7    bending about x                   "
                           "0.95  OK\n" ) );
  CHECK ( contains_( text, "Members (ABNT NBR 8800)\n" ) );
  CHECK ( contains_( text, "  P1               HP250x62.0   -                                    "
                           "-  resistances only\n" ) );
  CHECK ( contains_( text, "\n5 members checked, 0 failed\n" ) );

  // The HTML page: one section per member.

  CHECK ( contains_( html, "<title>members</title>" ) );
  CHECK ( contains_( html, "W410x53.0" ) );
  CHECK ( contains_( html, "</html>\n" ) );

  steelcheck_input_free ( &input );

  // A member that fails is counted; no HTML without a page.

  static const char failing[] =
    "material = \"A36\"\n"
    "[[member]]\nlabel = \"T1\"\nsection = \"W150x13\"\nlength = 2\nNSd = 1000\n";

  CHECK_INT_EQ ( steelcheck_input_load( &input, steelcheck_view_from_cstr( failing ), &db,
                                        nullptr ),
                 STEELCHECK_OK );

  out = tmpfile ();

  if ( out != nullptr )
  {
    CHECK_INT_EQ ( cli_check( out, nullptr, &input, "t", nullptr, &summary ), STEELCHECK_OK );
  }

  CHECK ( read_back_( out, text, sizeof text ) );
  CHECK_SIZE_EQ ( summary.members, 1 );
  CHECK_SIZE_EQ ( summary.failed, 1 );
  CHECK ( contains_( text, "  T1               W150x13.0    tension" ) );
  CHECK ( contains_( text, "FAIL\n\n1 member checked, 1 failed\n" ) );

  steelcheck_input_free ( &input );

  // A warning about a member (Cb below 1.0) is reported once, when the input is read, and not
  // again when the member is checked.

  static const char warned[] =
    "material = \"A36\"\n"
    "[[member]]\nsection = \"W150x13\"\nlength = 2\nCb = 0.8\nMxSd = 10\n";

  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK_INT_EQ ( steelcheck_input_load( &input, steelcheck_view_from_cstr( warned ), &db,
                                        &reporter ),
                 STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.warnings, 1 );

  out = tmpfile ();

  if ( out != nullptr )
  {
    CHECK_INT_EQ ( cli_check( out, nullptr, &input, "t", &reporter, &summary ), STEELCHECK_OK );
  }

  CHECK ( read_back_( out, text, sizeof text ) );
  CHECK_SIZE_EQ ( collector.count, 1 );

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );
}

//==================================================================================================
//  test_cli_suite
//==================================================================================================

void test_cli_suite ()
{
  RUN_TEST ( test_parse_list );
  RUN_TEST ( test_parse_show );
  RUN_TEST ( test_parse_check );
  RUN_TEST ( test_parse_help_version );
  RUN_TEST ( test_parse_double_dash );
  RUN_TEST ( test_parse_errors );
  RUN_TEST ( test_join_args );
  RUN_TEST ( test_choose_source );
  RUN_TEST ( test_print_diagnostic );
  RUN_TEST ( test_list );
  RUN_TEST ( test_show );
  RUN_TEST ( test_check );
}
