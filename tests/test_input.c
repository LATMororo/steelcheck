#include "steelcheck/compat.h"
#include "steelcheck/code.h"
#include "steelcheck/database.h"
#include "steelcheck/input.h"
#include "steelcheck/material.h"
#include "steelcheck/nbr8800.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Helpers
//==================================================================================================

/*
 *  Loads text into *input with the Gerdau database (in *db), collecting the diagnostics.
 */

static SteelCheckStatus load_ ( SteelCheckInput* input, SteelCheckDatabase* db, const char* text,
                                TestCollector* collector )
{
  if ( db->count == 0
       && steelcheck_database_load_file( db, "data/gerdau_w_hp.csv", nullptr ) != STEELCHECK_OK )
  {
    return STEELCHECK_ERROR_OPEN;
  }

  SteelCheckReporter reporter = test_reporter ( collector );

  return steelcheck_input_load ( input, steelcheck_view_from_cstr( text ), db, &reporter );
}

/*
 *  True if text is rejected with status, and its first diagnostic is the error at line and
 *  column whose message contains message.
 */

static bool rejected_ ( const char* text, SteelCheckStatus status, size_t line, size_t column,
                        const char* message )
{
  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};
  SteelCheckStatus   result    = load_ ( &input, &db, text, &collector );
  bool               ok        = true;

  if ( result != status )
  {
    fprintf ( stderr, "    status %d, expected %d\n", (int) result, (int) status );
    ok = false;
  }

  if ( !test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, line, column, message ) )
  {
    ok = false;
  }

  // A rejected text leaves the input empty.

  if ( input.members != nullptr || input.count != 0 )
  {
    fprintf ( stderr, "    the input is not empty\n" );
    ok = false;
  }

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );

  return ok;
}

//==================================================================================================
//  Valid input
//==================================================================================================

static void test_input_members ()
{
  static const char text[] =
    "# Two members\n"
    "code     = \"NBR 8800\"\n"
    "material = 'A572 Gr.50'   # literal string\n"
    "\n"
    "[[member]]\n"
    "label   = \"C1 # not a comment\"\n"
    "section = \"W 310 x 38,7\"\n"
    "length  = 3.5\n"
    "Ky      = 0.8\n"
    "Lb      = 1.75\n"
    "Cb      = 1.14\n"
    "NSd     = -300\n"
    "MxSd    = 100.5\n"
    "VySd    = 40\n"
    "\n"
    "  [[member]]  # indented, with a comment\n"
    "section  = \"HP250x62.0\"\n"
    "material = \"ASTM A36\"\n"
    "length   = 4\n"
    "An       = 70\n"
    "Ct       = 0.9\n";

  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};

  CHECK_INT_EQ ( load_( &input, &db, text, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK ( input.code == steelcheck_code_nbr8800() );
  CHECK_SIZE_EQ ( input.count, 2 );

  if ( input.count == 2 )
  {
    const SteelCheckMember* c1 = &input.members[0];
    const SteelCheckMember* p1 = &input.members[1];

    // Strings: '#' inside quotes is not a comment; the designation is found as typed.

    CHECK_STR_EQ ( c1->label, "C1 # not a comment" );
    CHECK ( c1->section == steelcheck_database_find( &db, "W310x38.7" ) );
    CHECK ( c1->material == steelcheck_material_find( "A572 Gr.50" ) );

    // Units: m -> mm, kN -> N, kN m -> N mm; the length is the default of every other length.

    CHECK_NEAR ( c1->Lx, 3500.0, 1e-12 );
    CHECK_NEAR ( c1->Ly, 3500.0, 1e-12 );
    CHECK_NEAR ( c1->Lz, 3500.0, 1e-12 );
    CHECK_NEAR ( c1->Lb, 1750.0, 1e-12 );
    CHECK_NEAR ( c1->Kx, 1.0, 0.0 );
    CHECK_NEAR ( c1->Ky, 0.8, 0.0 );
    CHECK_NEAR ( c1->Cb, 1.14, 0.0 );
    CHECK ( c1->has_actions );
    CHECK_NEAR ( c1->NSd, -300e3, 1e-12 );
    CHECK_NEAR ( c1->MxSd, 100.5e6, 1e-12 );
    CHECK_NEAR ( c1->VySd, 40e3, 1e-12 );
    CHECK_NEAR ( c1->MySd, 0.0, 0.0 );

    // The second member: its own material, a default label, cm^2 -> mm^2, no forces.

    CHECK_STR_EQ ( p1->label, "M2" );
    CHECK ( p1->material == steelcheck_material_find( "A36" ) );
    CHECK_NEAR ( p1->Lb, 4000.0, 1e-12 );
    CHECK_NEAR ( p1->An, 7000.0, 1e-12 );
    CHECK_NEAR ( p1->Ct, 0.9, 0.0 );
    CHECK ( !p1->has_actions );
  }

  steelcheck_input_free ( &input );
  CHECK ( input.members == nullptr && input.count == 0 );
  steelcheck_input_free ( &input );   // safe twice
  steelcheck_input_free ( nullptr );

  steelcheck_database_free ( &db );
}

static void test_input_defaults ()
{
  // No code: the default one. CRLF line ends and a last line without '\n'.

  static const char text[] =
    "[[member]]\r\n"
    "section = \"W150x13\"\r\n"
    "material = \"MR250\"\r\n"
    "length = 2";

  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};

  CHECK_INT_EQ ( load_( &input, &db, text, &collector ), STEELCHECK_OK );
  CHECK ( input.code == steelcheck_code_default() );
  CHECK_SIZE_EQ ( input.count, 1 );

  if ( input.count == 1 )
  {
    CHECK_STR_EQ ( input.members[0].label, "M1" );
    CHECK_STR_EQ ( input.members[0].section->designation, "W150x13.0" );
    CHECK_NEAR ( input.members[0].Lx, 2000.0, 1e-12 );
  }

  // Escapes in double-quoted strings.

  steelcheck_input_free ( &input );

  static const char escaped[] =
    "material = \"A36\"\n"
    "[[member]]\n"
    "label = \"a\\\"b\\\\c\\td\"\n"
    "section = \"W150x13\"\n"
    "length = 1\n";

  CHECK_INT_EQ ( load_( &input, &db, escaped, &collector ), STEELCHECK_OK );

  if ( input.count == 1 )
  {
    CHECK_STR_EQ ( input.members[0].label, "a\"b\\c\td" );
  }

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );
}

//==================================================================================================
//  Errors
//==================================================================================================

static void test_input_syntax_errors ()
{
  CHECK ( rejected_( "[member]\n", STEELCHECK_ERROR_SYNTAX, 1, 1, "unsupported table" ) );
  CHECK ( rejected_( "[[beam]]\n", STEELCHECK_ERROR_SYNTAX, 1, 3, "unsupported table [[beam]]" ) );
  CHECK ( rejected_( "[[member]\n", STEELCHECK_ERROR_SYNTAX, 1, 1, "malformed table header" ) );
  CHECK ( rejected_( "= 3\n", STEELCHECK_ERROR_SYNTAX, 1, 1, "expected a key" ) );
  CHECK ( rejected_( "code \"NBR\"\n", STEELCHECK_ERROR_SYNTAX, 1, 6,
                     "expected '=' after the key 'code'" ) );
  CHECK ( rejected_( "code =\n", STEELCHECK_ERROR_SYNTAX, 1, 7, "missing value" ) );
  CHECK ( rejected_( "code = \"NBR\n", STEELCHECK_ERROR_SYNTAX, 1, 8,
                     "string without its closing \"" ) );
  CHECK ( rejected_( "code = \"N\\x\"\n", STEELCHECK_ERROR_SYNTAX, 1, 10,
                     "unsupported escape sequence '\\x'" ) );
  CHECK ( rejected_( "code = \"\"\"NBR\"\"\"\n", STEELCHECK_ERROR_SYNTAX, 1, 8,
                     "multi-line strings" ) );
  CHECK ( rejected_( "code = [1, 2]\n", STEELCHECK_ERROR_SYNTAX, 1, 8, "arrays" ) );
  CHECK ( rejected_( "code = \"NBR 8800\" x\n", STEELCHECK_ERROR_SYNTAX, 1, 19,
                     "unexpected text after the value" ) );
}

static void test_input_value_errors ()
{
  CHECK ( rejected_( "code = \"AISC\"\n", STEELCHECK_ERROR_VALUE, 1, 8,
                     "unknown design code 'AISC'" ) );
  CHECK ( rejected_( "material = \"S355\"\n", STEELCHECK_ERROR_VALUE, 1, 12,
                     "unknown material 'S355'" ) );
  CHECK ( rejected_( "steel = \"A36\"\n", STEELCHECK_ERROR_VALUE, 1, 1, "unknown key 'steel'" ) );
  CHECK ( rejected_( "code = \"NBR 8800\"\ncode = \"NBR 8800\"\n", STEELCHECK_ERROR_DUPLICATE, 2,
                     1, "key 'code' given twice" ) );

  // Inside a member.

  CHECK ( rejected_( "[[member]]\nsection = W310x38.7\n", STEELCHECK_ERROR_VALUE, 2, 11,
                     "'section' needs a string in quotes" ) );
  CHECK ( rejected_( "[[member]]\nlength = \"3\"\n", STEELCHECK_ERROR_VALUE, 2, 10,
                     "'length' needs a number, not a string" ) );
  CHECK ( rejected_( "[[member]]\nlength = 3,5\n", STEELCHECK_ERROR_NUMBER, 2, 10,
                     "for 'length': '3,5'" ) );
  CHECK ( rejected_( "[[member]]\nNSd = 1e999\n", STEELCHECK_ERROR_RANGE, 2, 7,
                     "for 'NSd': '1e999'" ) );
  CHECK ( rejected_( "[[member]]\nsection = \"W999x1\"\n", STEELCHECK_ERROR_VALUE, 2, 11,
                     "no section 'W999x1' in the database" ) );
  CHECK ( rejected_( "[[member]]\nlabel = \"12345678901234567890123456789012\"\n",
                     STEELCHECK_ERROR_VALUE, 2, 9, "label too long" ) );
  CHECK ( rejected_( "[[member]]\nLx = 1\nLx = 2\n", STEELCHECK_ERROR_DUPLICATE, 3, 1,
                     "key 'Lx' given twice in this member" ) );
  CHECK ( rejected_( "[[member]]\nnsd = 1\n", STEELCHECK_ERROR_VALUE, 2, 1,
                     "unknown key 'nsd' in [[member]]" ) );
  CHECK ( rejected_( "[[member]]\ncode = \"NBR 8800\"\n", STEELCHECK_ERROR_VALUE, 2, 1,
                     "top-level keys must come before the first [[member]]" ) );
}

static void test_input_missing ()
{
  CHECK ( rejected_( "", STEELCHECK_ERROR_MISSING_VALUE, 0, 0, "no [[member]] tables" ) );
  CHECK ( rejected_( "# only a comment\ncode = \"NBR 8800\"\n", STEELCHECK_ERROR_MISSING_VALUE,
                     0, 0, "no [[member]] tables" ) );

  // Reported at the member's [[member]] line, numbered by position.

  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nlength = 3\n",
                     STEELCHECK_ERROR_MISSING_VALUE, 2, 0, "member 1 has no 'section'" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\n",
                     STEELCHECK_ERROR_MISSING_VALUE, 2, 0, "member 1 has no 'length'" ) );
  CHECK ( rejected_( "[[member]]\nsection = \"W150x13\"\nlength = 3\n",
                     STEELCHECK_ERROR_MISSING_VALUE, 1, 0, "member 1 has no 'material'" ) );

  // The data of a complete member is validated, and errors are reported at its line.

  CHECK ( rejected_( "material = \"A36\"\n"
                     "[[member]]\nsection = \"W150x13\"\nlength = 3\n"
                     "[[member]]\nsection = \"W150x13\"\nlength = 3\nCb = 0\n",
                     STEELCHECK_ERROR_VALUE, 5, 0, "member 'M2': Cb must be positive, got 0" ) );
}

/*
 *  The moment diagram for Cb: kN m -> N mm, has_moments, and the rules for giving it.
 */

static void test_input_moments ()
{
  static const char text[] =
    "material = \"A36\"\n"
    "[[member]]\nsection = \"W150x13\"\nlength = 4\nMA = 7.5\nMB = -10\nMC = 7.5\n"
    "[[member]]\nsection = \"W150x13\"\nlength = 4\nMA = 1\nMB = 2\nMC = 3\nMmax = 4\n"
    "[[member]]\nsection = \"W150x13\"\nlength = 4\n";

  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};

  CHECK_INT_EQ ( load_( &input, &db, text, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( input.count, 3 );

  if ( input.count == 3 )
  {
    CHECK ( input.members[0].has_moments );
    CHECK_NEAR ( input.members[0].MA, 7.5e6, 1e-12 );
    CHECK_NEAR ( input.members[0].MB, -10e6, 1e-12 );
    CHECK_NEAR ( input.members[0].Mmax, 0.0, 0.0 );
    CHECK ( !input.members[0].has_actions );   // the diagram is not a design force

    CHECK_NEAR ( input.members[1].Mmax, 4e6, 1e-12 );

    // Neither Cb nor moments: Cb = 1.0.

    CHECK ( !input.members[2].has_moments );
    CHECK_NEAR ( input.members[2].Cb, 1.0, 0.0 );
  }

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );

  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "MA = 1\nMB = 2\n", STEELCHECK_ERROR_MISSING_VALUE, 2, 0,
                     "member 1: give MA, MB and MC together" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "Mmax = 2\n", STEELCHECK_ERROR_MISSING_VALUE, 2, 0,
                     "member 1: Mmax needs MA, MB and MC" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "Cb = 1.2\nMA = 1\nMB = 2\nMC = 3\n", STEELCHECK_ERROR_VALUE, 2, 0,
                     "member 1: give either Cb or MA, MB and MC, not both" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "MA = 1\nMB = 2\nMC = 3\nMmax = 2.5\n", STEELCHECK_ERROR_VALUE, 2, 0,
                     "Mmax 2.5e+06 N mm is smaller than one of MA, MB, MC" ) );
}

/*
 *  The moments for Cb with one flange restrained: M1, M2, MCL with their signs.
 */

static void test_input_free_flange ()
{
  static const char text[] =
    "material = \"A36\"\n"
    "[[member]]\nsection = \"W150x13\"\nlength = 4\nM1 = -20\nM2 = 5\nMCL = 10\n";

  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};

  CHECK_INT_EQ ( load_( &input, &db, text, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( input.count, 1 );

  if ( input.count == 1 )
  {
    CHECK ( input.members[0].has_free_flange );
    CHECK ( !input.members[0].has_moments );
    CHECK_NEAR ( input.members[0].M1, -20e6, 1e-12 );
    CHECK_NEAR ( input.members[0].M2, 5e6, 1e-12 );
    CHECK_NEAR ( input.members[0].MCL, 10e6, 1e-12 );
  }

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );

  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "M1 = -20\nM2 = 5\n", STEELCHECK_ERROR_MISSING_VALUE, 2, 0,
                     "member 1: give M1, M2 and MCL together" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "Cb = 2\nM1 = -20\nM2 = 5\nMCL = 0\n", STEELCHECK_ERROR_VALUE, 2, 0,
                     "member 1: give only one of Cb, MA-MB-MC and M1-M2-MCL" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "MA = 1\nMB = 1\nMC = 1\nM1 = -20\nM2 = 5\nMCL = 0\n",
                     STEELCHECK_ERROR_VALUE, 2, 0,
                     "member 1: give only one of Cb, MA-MB-MC and M1-M2-MCL" ) );
  CHECK ( rejected_( "material = \"A36\"\n[[member]]\nsection = \"W150x13\"\nlength = 4\n"
                     "M1 = 20\nM2 = 5\nMCL = 0\n", STEELCHECK_ERROR_VALUE, 2, 0,
                     "M1 must be negative" ) );
}

/*
 *  Every problem is reported, not only the first; an unknown top-level material is not reported
 *  again as a missing one.
 */

static void test_input_all_errors ()
{
  static const char text[] =
    "material = \"A999\"\n"
    "[[member]]\n"
    "section = \"W150x13\"\n"
    "length = 3\n"
    "[[member]]\n"
    "section = \"W150x13\"\n";

  SteelCheckDatabase db        = {};
  SteelCheckInput    input     = {};
  TestCollector      collector = {};

  CHECK_INT_EQ ( load_( &input, &db, text, &collector ), STEELCHECK_ERROR_VALUE );
  CHECK_SIZE_EQ ( collector.errors, 2 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 1, 12,
                              "unknown material 'A999'" ) );
  CHECK ( test_diagnostic_is( &collector, 1, STEELCHECK_SEVERITY_ERROR, 5, 0,
                              "member 2 has no 'length'" ) );
  CHECK ( input.members == nullptr );

  steelcheck_database_free ( &db );
}

static void test_input_arguments ()
{
  SteelCheckDatabase db    = {};
  SteelCheckInput    input = {};

  CHECK_INT_EQ ( steelcheck_input_load( nullptr, steelcheck_view_from_cstr( "" ), &db, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_input_load( &input, steelcheck_view_from_cstr( "" ), nullptr,
                                        nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  // A file that does not exist.

  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK_INT_EQ ( steelcheck_input_load_file( &input, "tests/data/no_such_file.toml", &db,
                                             &reporter ),
                 STEELCHECK_ERROR_OPEN );
  CHECK_SIZE_EQ ( collector.errors, 1 );

  // The example of the project loads.

  CHECK_INT_EQ ( steelcheck_database_load_file( &db, "data/gerdau_w_hp.csv", nullptr ),
                 STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_input_load_file( &input, "examples/members.toml", &db, nullptr ),
                 STEELCHECK_OK );
  CHECK_SIZE_EQ ( input.count, 5 );

  steelcheck_input_free ( &input );
  steelcheck_database_free ( &db );
}

//==================================================================================================
//  test_input_suite
//==================================================================================================

void test_input_suite ()
{
  RUN_TEST ( test_input_members );
  RUN_TEST ( test_input_defaults );
  RUN_TEST ( test_input_syntax_errors );
  RUN_TEST ( test_input_value_errors );
  RUN_TEST ( test_input_missing );
  RUN_TEST ( test_input_moments );
  RUN_TEST ( test_input_free_flange );
  RUN_TEST ( test_input_all_errors );
  RUN_TEST ( test_input_arguments );
}
