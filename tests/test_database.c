#include "steelcheck/compat.h"
#include "steelcheck/database.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

/*
 *  Loads text into an empty database, with diagnostics going to collector.
 */

static SteelCheckStatus load_ ( SteelCheckDatabase* db, const char* text, TestCollector* collector )
{
  SteelCheckReporter reporter = test_reporter ( collector );

  return steelcheck_database_load ( db, steelcheck_view_from_cstr( text ), &reporter );
}


//==================================================================================================
//  CSV texts used by the tests
//==================================================================================================

/*
 *  Macros, not arrays, so that adjacent string literals can be joined into one text at compile
 *  time: HEADER_ W150_13_ W150_18_ is a single string literal.
 */

#define HEADER_                                                                                  \
  "designation,family,h_shape,on_request,mass_kg_m,d_mm,bf_mm,tw_mm,tf_mm,h_mm,dp_mm,A_cm2,"     \
  "Ix_cm4,Wx_cm3,rx_cm,Zx_cm3,Iy_cm4,Wy_cm3,ry_cm,Zy_cm3,rt_cm,J_cm4,lambda_f,lambda_w,Cw_cm6,"  \
  "u_m2_m,designation_imperial\n"

#define W150_13_                                                                                 \
  "W150x13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,96.4,82,16.4,2.22,25.5,2.60," \
  "1.72,10.20,27.49,4181,0.67,W6x8.5\n"

#define W150_18_                                                                                 \
  "W150x18.0,W,0,0,18.0,153,102,5.8,7.1,139,119,23.4,939,122.8,6.34,139.4,126,24.7,2.32,38.5,"   \
  "2.69,4.34,7.18,20.48,6683,0.69,W6x12\n"

//==================================================================================================
//  Loading texts
//==================================================================================================

static void test_load_small ()
{
  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( &db, "# catalog: test catalog\n"
                             "# edition: 1\n"
                             "\n"
                             HEADER_ W150_13_ W150_18_, &collector ),
                 STEELCHECK_OK );

  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK_SIZE_EQ ( db.count, 2 );
  CHECK_SIZE_EQ ( db.capacity, 16 );

  // Sections are in file order, validated (r computed), with their source lines.

  CHECK_STR_EQ ( db.sections[0].designation, "W150x13.0" );
  CHECK_STR_EQ ( db.sections[1].designation, "W150x18.0" );
  CHECK_SIZE_EQ ( db.sections[1].source_line, 6 );
  CHECK ( steelcheck_section_was_computed( &db.sections[0], STEELCHECK_FIELD_R ) );

  CHECK_STR_EQ ( steelcheck_database_metadata( &db, "catalog" ), "test catalog" );
  CHECK_STR_EQ ( steelcheck_database_metadata( &db, "edition" ), "1" );
  CHECK ( steelcheck_database_metadata( &db, "units" ) == nullptr );
  CHECK ( steelcheck_database_metadata( &db, "Catalog" ) == nullptr );   // case-sensitive

  // Lookup, in the different spellings.

  const SteelCheckSection* s = steelcheck_database_find ( &db, "w 150 x 18,0" );

  CHECK ( s == &db.sections[1] );
  CHECK ( steelcheck_database_find( &db, "W150x18" ) == &db.sections[1] );
  CHECK ( steelcheck_database_find( &db, "W150x13.0" ) == &db.sections[0] );
  CHECK ( steelcheck_database_find( &db, "W150x22.5" ) == nullptr );
  CHECK ( steelcheck_database_find( &db, "" ) == nullptr );
  CHECK ( steelcheck_database_find( &db, nullptr ) == nullptr );
  CHECK ( steelcheck_database_find( nullptr, "W150x13.0" ) == nullptr );

  steelcheck_database_free ( &db );

  CHECK ( db.sections == nullptr && db.count == 0 && db.capacity == 0 );
  CHECK_SIZE_EQ ( db.metadata_count, 0 );
}

static void test_load_duplicate ()
{
  // The same section twice, spelled differently: one error, and the database stays empty.

  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( &db, HEADER_ W150_13_ W150_18_
                             "w150X13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,"
                             "96.4,82,16.4,2.22,25.5,2.60,1.72,10.20,27.49,4181,0.67,W6x8.5\n",
                        &collector ),
                 STEELCHECK_ERROR_DUPLICATE );

  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 4, 0,
                              "duplicate designation 'w150X13.0' (also line 2: 'W150x13.0')" ) );
  CHECK_INT_EQ ( collector.items[0].status, STEELCHECK_ERROR_DUPLICATE );

  CHECK ( db.sections == nullptr );
  CHECK_SIZE_EQ ( db.count, 0 );
}

static void test_load_all_errors_reported ()
{
  // Line 2: a negative J (validation). Line 3: a valid row. Line 4: an unknown family
  // (conversion). Line 5: too few fields (CSV). All three errors are reported; the result is the
  // status of the first one, and the database stays empty although line 3 was fine.

  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( &db, HEADER_
                             "W150x13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,"
                             "96.4,82,16.4,2.22,25.5,2.60,-1.72,10.20,27.49,4181,0.67,W6x8.5\n"
                             W150_18_
                             "W150x22.5,X,1,0,22.5,152,152,5.8,6.6,139,119,29.0,1229,161.7,6.51,"
                             "179.6,387,50.9,3.65,77.9,4.10,4.75,11.52,20.48,20417,0.88,W6x15\n"
                             "W150x24.0,W,0,0,24.0\n",
                        &collector ),
                 STEELCHECK_ERROR_VALUE );

  CHECK_SIZE_EQ ( collector.errors, 3 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 2, 0,
                              "J_cm4 must be positive, got -1.72" ) );
  CHECK ( test_diagnostic_is( &collector, 1, STEELCHECK_SEVERITY_ERROR, 4, 2,
                              "column 'family': expected W or HP, got 'X'" ) );
  CHECK ( test_diagnostic_is( &collector, 2, STEELCHECK_SEVERITY_ERROR, 5, 0,
                              "5 fields, but the header (line 1) has 27 columns" ) );

  CHECK_SIZE_EQ ( db.count, 0 );
  CHECK ( db.sections == nullptr );
}

static void test_load_header_problems ()
{
  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  // No data line at all: no header.

  CHECK_INT_EQ ( load_( &db, "# only a comment\n\n", &collector ), STEELCHECK_ERROR_HEADER );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "no header line" ) );

  CHECK_INT_EQ ( load_( &db, "", &collector ), STEELCHECK_ERROR_HEADER );

  // A header without required columns: the rows after it are not even read.

  collector = (TestCollector) {};

  CHECK_INT_EQ ( load_( &db, "designation,family\nW150x13.0,W\n", &collector ),
                 STEELCHECK_ERROR_HEADER );
  CHECK_SIZE_EQ ( collector.errors, 18 );   // 20 required columns, 2 present
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 1, 0,
                              "missing required column 'mass_kg_m'" ) );
  CHECK_SIZE_EQ ( db.count, 0 );

  // A header and no rows: an empty database is valid, with a warning.

  collector = (TestCollector) {};

  CHECK_INT_EQ ( load_( &db, HEADER_, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( db.count, 0 );
  CHECK_SIZE_EQ ( collector.warnings, 1 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_WARNING, 1, 0,
                              "no sections after the header" ) );

  steelcheck_database_free ( &db );
}

static void test_load_growth ()
{
  // 40 sections: the array grows from 16 to 32 to 64 slots. The rows are generated, with
  // designations W1x13.0 to W40x13.0 and the values of W150x13.0.

  static char text[8192];
  size_t      used = (size_t) snprintf ( text, sizeof text, "%s", HEADER_ );

  for ( int i = 1; i <= 40; i++ )
  {
    used += (size_t) snprintf ( text + used, sizeof text - used,
                                "W%dx13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,"
                                "96.4,82,16.4,2.22,25.5,2.60,1.72,10.20,27.49,4181,0.67,\n", i );
  }

  CHECK ( used < sizeof text );

  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( &db, text, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK_SIZE_EQ ( db.count, 40 );
  CHECK_SIZE_EQ ( db.capacity, 64 );

  // Every section survived the reallocations, in order.

  const SteelCheckSection* s = steelcheck_database_find ( &db, "W37x13.0" );

  CHECK ( s == &db.sections[36] );
  CHECK_STR_EQ ( db.sections[39].designation, "W40x13.0" );
  CHECK_SIZE_EQ ( db.sections[39].source_line, 41 );

  steelcheck_database_free ( &db );
}

static void test_load_metadata_limits ()
{
  // 18 entries: "a" twice (the second is ignored), one with a value too long for 127 characters,
  // and 16 more distinct keys, of which only 15 fit after "a" (16 at most).

  static char text[4096];
  size_t      used = (size_t) snprintf ( text, sizeof text, "# a: first\n# a: second\n" );

  used += (size_t) snprintf ( text + used, sizeof text - used, "# long: %0200d\n", 7 );

  for ( int i = 0; i < 16; i++ )
  {
    used += (size_t) snprintf ( text + used, sizeof text - used, "# key%d: %d\n", i, i );
  }

  used += (size_t) snprintf ( text + used, sizeof text - used, "%s%s", HEADER_, W150_13_ );

  CHECK ( used < sizeof text );

  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( &db, text, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( db.metadata_count, steelcheck_metadata_max );
  CHECK_STR_EQ ( steelcheck_database_metadata( &db, "a" ), "first" );
  CHECK ( steelcheck_database_metadata( &db, "long" ) == nullptr );
  CHECK_STR_EQ ( steelcheck_database_metadata( &db, "key14" ), "14" );
  CHECK ( steelcheck_database_metadata( &db, "key15" ) == nullptr );

  CHECK_SIZE_EQ ( collector.warnings, 3 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_WARNING, 2, 0,
                              "metadata 'a' given twice" ) );
  CHECK ( test_diagnostic_is( &collector, 1, STEELCHECK_SEVERITY_WARNING, 3, 0,
                              "metadata 'long' ignored (too long)" ) );
  CHECK ( test_diagnostic_is( &collector, 2, STEELCHECK_SEVERITY_WARNING, 19, 0,
                              "metadata 'key15' ignored (at most 16 entries)" ) );

  steelcheck_database_free ( &db );
}

static void test_load_invalid_arguments ()
{
  TestCollector      collector = {};
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( load_( nullptr, HEADER_ W150_13_, &collector ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  // Loading into a database that is not empty would lose (leak) its sections: refused, and the
  // database keeps what it had.

  CHECK_INT_EQ ( load_( &db, HEADER_ W150_13_, &collector ), STEELCHECK_OK );
  CHECK_INT_EQ ( load_( &db, HEADER_ W150_18_, &collector ), STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_SIZE_EQ ( db.count, 1 );

  CHECK_INT_EQ ( steelcheck_database_load_file( &db, "data/gerdau_w_hp.csv", nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  steelcheck_database_free ( &db );
  steelcheck_database_free ( &db );        // twice
  steelcheck_database_free ( nullptr );

  CHECK_INT_EQ ( steelcheck_database_load_file( &db, nullptr, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
}

//==================================================================================================
//  Loading files
//==================================================================================================

static void test_load_file_gerdau ()
{
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( steelcheck_database_load_file( &db, "data/gerdau_w_hp.csv", &reporter ),
                 STEELCHECK_OK );

  // 108 sections; the only finding is the catalog's own W610x153.0 mass (a warning).

  CHECK_SIZE_EQ ( db.count, 108 );
  CHECK_SIZE_EQ ( collector.errors, 0 );
  CHECK_SIZE_EQ ( collector.warnings, 1 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_WARNING, 113, 0,
                              "W610x153.0: mass_kg_m 154.2" ) );

  CHECK_STR_EQ ( steelcheck_database_metadata( &db, "edition" ), "08/25" );
  CHECK ( strstr( steelcheck_database_metadata( &db, "catalog" ), "Gerdau" ) != nullptr );

  // Lookup as a user would type it.

  const SteelCheckSection* s = steelcheck_database_find ( &db, "W 310 x 38,7" );

  CHECK ( s != nullptr );

  if ( s != nullptr )
  {
    CHECK_STR_EQ ( s->designation, "W310x38.7" );
    CHECK_NEAR ( s->d,  310.0,  1e-12 );
    CHECK_NEAR ( s->Ix, 8581e4, 1e-12 );   // 8581 cm^4
  }

  CHECK ( steelcheck_database_find( &db, "W360x58" ) != nullptr );        // printed "W 360 x 58"
  CHECK ( steelcheck_database_find( &db, "hp 310 x 132" ) != nullptr );   // printed "HP 310 x 132"
  CHECK ( steelcheck_database_find( &db, "W999x1.0" ) == nullptr );
  CHECK_STR_EQ ( db.sections[db.count - 1].designation, "W610x217.0" );

  steelcheck_database_free ( &db );
}

static void test_load_file_missing ()
{
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );
  SteelCheckDatabase db        = {};

  CHECK_INT_EQ ( steelcheck_database_load_file( &db, "tests/data/does_not_exist.csv", &reporter ),
                 STEELCHECK_ERROR_OPEN );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "tests/data/does_not_exist.csv: cannot open file" ) );
  CHECK_SIZE_EQ ( db.count, 0 );
}

//==================================================================================================
//  test_database_suite
//==================================================================================================

void test_database_suite ()
{
  RUN_TEST ( test_load_small );
  RUN_TEST ( test_load_duplicate );
  RUN_TEST ( test_load_all_errors_reported );
  RUN_TEST ( test_load_header_problems );
  RUN_TEST ( test_load_growth );
  RUN_TEST ( test_load_metadata_limits );
  RUN_TEST ( test_load_invalid_arguments );
  RUN_TEST ( test_load_file_gerdau );
  RUN_TEST ( test_load_file_missing );
}
