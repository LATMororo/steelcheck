#include "steelcheck/compat.h"
#include "steelcheck/code.h"
#include "steelcheck/database.h"
#include "steelcheck/material.h"
#include "steelcheck/member.h"
#include "steelcheck/report.h"
#include "steelcheck/result.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

/*
 *  Size of the buffer that receives a report: the HTML of one member is about 20 kB.
 */

constexpr size_t report_size_ = 65536;

//==================================================================================================
//  Helpers
//==================================================================================================

/*
 *  Reads everything written to file into text and closes file (see read_back_ in test_cli.c).
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
 *  True if text contains part. Prints the start of the text otherwise.
 */

static bool contains_ ( const char* text, const char* part )
{
  if ( strstr( text, part ) != nullptr )
  {
    return true;
  }

  fprintf ( stderr, "    \"%s\" not found in:\n%.2000s\n", part, text );
  return false;
}

/*
 *  A checked W310x38.7 column of A572 Gr.50, 3 m long, with the given label and forces
 *  (has_actions if any is not zero). The database must outlive the member.
 */

static bool checked_ ( SteelCheckDatabase* db, SteelCheckMember* m, SteelCheckResult* r,
                       const char* label, double NSd, double MxSd )
{
  if ( steelcheck_database_load_file( db, "data/gerdau_w_hp.csv", nullptr ) != STEELCHECK_OK )
  {
    return false;
  }

  if ( steelcheck_member_init( m, label, steelcheck_database_find( db, "W310x38.7" ),
                               steelcheck_material_find( "A572 Gr.50" ), 3000.0 )
       != STEELCHECK_OK )
  {
    return false;
  }

  m->has_actions = NSd != 0.0 || MxSd != 0.0;
  m->NSd         = NSd;
  m->MxSd        = MxSd;

  return steelcheck_check_member( steelcheck_code_default(), m, r, nullptr ) == STEELCHECK_OK;
}

//==================================================================================================
//  Numbers and units
//==================================================================================================

/*
 *  The formatted text of value.
 */

static bool number_is_ ( double value, const char* expected )
{
  char text[steelcheck_number_text_size];

  steelcheck_format_number ( value, text, sizeof text );

  if ( strcmp( text, expected ) == 0 )
  {
    return true;
  }

  fprintf ( stderr, "    %g formatted as \"%s\", expected \"%s\"\n", value, text, expected );
  return false;
}

static void test_format_number ()
{
  // Below 1000: 4 significant digits.

  CHECK ( number_is_( 6.18, "6.18" ) );
  CHECK ( number_is_( 85.8, "85.8" ) );
  CHECK ( number_is_( 0.65842, "0.6584" ) );
  CHECK ( number_is_( 345.0, "345" ) );
  CHECK ( number_is_( -0.25, "-0.25" ) );
  CHECK ( number_is_( 0.0, "0" ) );

  // From 1000: whole numbers, or one decimal.

  CHECK ( number_is_( 8581.0, "8581" ) );
  CHECK ( number_is_( 14676643.0, "14676643" ) );
  CHECK ( number_is_( 1597.4, "1597.4" ) );
  CHECK ( number_is_( 988.11992, "988.1" ) );
  CHECK ( number_is_( 1170.49, "1170.5" ) );
}

static void test_display_units ()
{
  SteelCheckDisplayUnit kn  = steelcheck_display_unit ( STEELCHECK_UNIT_N );
  SteelCheckDisplayUnit knm = steelcheck_display_unit ( STEELCHECK_UNIT_NMM );
  SteelCheckDisplayUnit cm2 = steelcheck_display_unit ( STEELCHECK_UNIT_MM2 );
  SteelCheckDisplayUnit mpa = steelcheck_display_unit ( STEELCHECK_UNIT_MPA );

  CHECK_STR_EQ ( kn.text, "kN" );
  CHECK_NEAR ( kn.factor, 1e-3, 0.0 );
  CHECK_STR_EQ ( knm.text, "kN m" );
  CHECK_NEAR ( knm.factor, 1e-6, 0.0 );
  CHECK_STR_EQ ( cm2.text, "cm^2" );
  CHECK_NEAR ( cm2.factor, 1e-2, 0.0 );
  CHECK_STR_EQ ( mpa.text, "MPa" );
  CHECK_NEAR ( mpa.factor, 1.0, 0.0 );

  // Every unit has a text and a LaTeX form, possibly empty, never nullptr.

  SteelCheckDisplayUnit none = steelcheck_display_unit ( STEELCHECK_UNIT_NONE );

  CHECK ( none.text != nullptr && none.tex != nullptr );
  CHECK_NEAR ( none.factor, 1.0, 0.0 );
}

//==================================================================================================
//  Text report
//==================================================================================================

static void test_text_report ()
{
  static char      text[report_size_];
  SteelCheckDatabase db = {};
  SteelCheckMember   m  = {};
  SteelCheckResult   r  = {};

  CHECK ( checked_( &db, &m, &r, "C1", -300e3, 100e6 ) );

  FILE* file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_text ( file, &m, &r );
  }

  CHECK ( read_back_( file, text, sizeof text ) );

  // The member, in display units.

  CHECK ( contains_( text, "C1: W310x38.7, ASTM A572 Gr.50, ABNT NBR 8800\n" ) );
  CHECK ( contains_( text, "NSd = -300 kN, MxSd = 100 kN m" ) );

  // The code's note, although tension (the first check) is not applicable.

  CHECK ( contains_( text, "  note: Formulas and clause numbers of ABNT NBR 8800:2024" ) );

  // A step of the calculation and the conclusion of a check.

  CHECK ( contains_( text, "    b_ef,w,y       =          264 mm     "
                           "b/t > (b/t)_lim / sqrt(chi)" ) );
  CHECK ( contains_( text, "    N_c,Rd,y       =        985.7 kN     flexural buckling about y" ) );
  CHECK ( contains_( text, "  => resistance 985.7 kN (flexural buckling about y), "
                           "design force 300 kN, utilization 0.30: OK\n" ) );

  // The summary, without trailing spaces in the rows of checks that were not done.

  CHECK ( contains_( text, "  tension                   not applicable\n" ) );
  CHECK ( contains_( text, "  Governing: axial force and bending, utilization 0.86: OK\n" ) );
  CHECK ( strstr( text, " \n" ) == nullptr );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &db );
}

static void test_text_report_moments ()
{
  static char      text[report_size_];
  SteelCheckDatabase db = {};
  SteelCheckMember   m  = {};
  SteelCheckResult   r  = {};

  // Load the database and the member, then check it again with the moment diagram.

  CHECK ( checked_( &db, &m, &r, "B1", 0.0, 100e6 ) );
  steelcheck_result_free ( &r );

  m.has_moments = true;
  m.MA          = 75e6;
  m.MB          = 100e6;
  m.MC          = 75e6;

  CHECK ( steelcheck_check_member( steelcheck_code_default(), &m, &r, nullptr ) == STEELCHECK_OK );

  FILE* file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_text ( file, &m, &r );
  }

  CHECK ( read_back_( file, text, sizeof text ) );
  CHECK ( contains_( text, "           Lb = 3000 mm, Cb from the moments in Lb:\n"
                           "           MA = 75 kN m, MB = 100 kN m, MC = 75 kN m, "
                           "Mmax = (largest)\n" ) );
  CHECK ( contains_( text, "    C_b            =        1.136        moment gradient factor "
                           "[5.4.2.3 a)]\n" ) );

  steelcheck_result_free ( &r );

  // One flange restrained: M1, M2, MCL in the member data, and Cb by 5.4.2.4 a).

  m.has_moments     = false;
  m.has_free_flange = true;
  m.M1              = -100e6;
  m.M2              = 50e6;
  m.MCL             = 0.0;

  CHECK ( steelcheck_check_member( steelcheck_code_default(), &m, &r, nullptr ) == STEELCHECK_OK );

  file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_text ( file, &m, &r );
  }

  CHECK ( read_back_( file, text, sizeof text ) );
  CHECK ( contains_( text, "           Lb = 3000 mm (free flange), Cb from the moments "
                           "(5.4.2.4 a)):\n"
                           "           M1 = -100 kN m, M2 = 50 kN m, MCL = 0 kN m\n" ) );
  CHECK ( contains_( text, "    -> M_2 tensions the free flange: zero in the third term\n" ) );
  CHECK ( contains_( text, "    C_b            =        3.333        moment gradient factor "
                           "[5.4.2.4 a)]\n" ) );

  // FLT against |M1| = 100 kN m, FLM and FLA against MxSd = 100 kN m: the same force, so no
  // lines per limit state.

  CHECK ( contains_( text, "    M_Sd,FLT       =          100 kN m   design moment of FLT" ) );
  CHECK ( strstr( text, "     lateral-torsional buckling: resistance" ) == nullptr );

  steelcheck_result_free ( &r );

  // |M1| = 60 kN m below MxSd = 100 kN m: one line per limit state.

  m.M1 = -60e6;

  CHECK ( steelcheck_check_member( steelcheck_code_default(), &m, &r, nullptr ) == STEELCHECK_OK );

  file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_text ( file, &m, &r );
  }

  CHECK ( read_back_( file, text, sizeof text ) );
  CHECK ( contains_( text, "     lateral-torsional buckling: resistance " ) );
  CHECK ( contains_( text, ", design force 60 kN m, utilization " ) );
  CHECK ( contains_( text, "     flange local buckling: resistance 193 kN m, design force "
                           "100 kN m, utilization 0.52\n" ) );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &db );
}

static void test_text_report_resistances ()
{
  static char      text[report_size_];
  SteelCheckDatabase db = {};
  SteelCheckMember   m  = {};
  SteelCheckResult   r  = {};

  CHECK ( checked_( &db, &m, &r, "P1", 0.0, 0.0 ) );

  FILE* file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_text ( file, &m, &r );
  }

  CHECK ( read_back_( file, text, sizeof text ) );
  CHECK ( contains_( text, "  compression               resistance               985.7 kN\n" ) );
  CHECK ( contains_( text, "  No design forces: resistances only.\n" ) );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &db );
}

//==================================================================================================
//  HTML report
//==================================================================================================

static void test_html_report ()
{
  static char      text[report_size_];
  SteelCheckDatabase db = {};
  SteelCheckMember   m  = {};
  SteelCheckResult   r  = {};

  // A label with characters that HTML would take as markup.

  CHECK ( checked_( &db, &m, &r, "<b>&\"x\"", -300e3, 100e6 ) );

  FILE* file = tmpfile ();

  if ( file != nullptr )
  {
    steelcheck_report_html_begin ( file, "Report <1>" );
    steelcheck_report_html_member ( file, &m, &r );
    steelcheck_report_html_end ( file );
  }

  CHECK ( read_back_( file, text, sizeof text ) );

  // A whole page, with KaTeX from the CDN.

  CHECK ( strncmp( text, "<!DOCTYPE html>\n", 16 ) == 0 );
  CHECK ( contains_( text, "<title>Report &lt;1&gt;</title>" ) );
  CHECK ( contains_( text, "cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/katex.min.js" ) );
  CHECK ( contains_( text, "</body>\n</html>\n" ) );

  // The label is escaped: never as markup.

  CHECK ( contains_( text, "&lt;b&gt;&amp;&quot;x&quot;" ) );
  CHECK ( strstr( text, "<b>&" ) == nullptr );

  // The equations are LaTeX, for KaTeX to typeset.

  CHECK ( contains_( text, "\\lambda_{0,y}" ) );
  CHECK ( contains_( text, "\\mathrm{kN}" ) );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &db );
}

//==================================================================================================
//  test_report_suite
//==================================================================================================

void test_report_suite ()
{
  RUN_TEST ( test_format_number );
  RUN_TEST ( test_display_units );
  RUN_TEST ( test_text_report );
  RUN_TEST ( test_text_report_resistances );
  RUN_TEST ( test_text_report_moments );
  RUN_TEST ( test_html_report );
}
