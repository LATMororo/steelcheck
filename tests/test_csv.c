#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"
#include "steelcheck/csv.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/lines.h"
#include "suites.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Test reporter: collects diagnostics so the tests can check them
//==================================================================================================

constexpr size_t collector_capacity_ = 16;

typedef struct Collector
{
  size_t               count;      // diagnostics received (may exceed the capacity)
  size_t               errors;
  size_t               warnings;
  SteelCheckDiagnostic items[collector_capacity_];
} Collector;

/*
 *  The reporter callback. context is the Collector, passed through the void* of the reporter.
 *  The diagnostic only lives during the call, so it is copied.
 */

static void collect_ ( void* context, const SteelCheckDiagnostic* diagnostic )
{
  Collector* collector = context;

  if ( collector->count < collector_capacity_ )
  {
    collector->items[collector->count] = *diagnostic;
  }

  collector->count++;

  if ( diagnostic->severity == STEELCHECK_SEVERITY_ERROR )
  {
    collector->errors++;
  }
  else
  {
    collector->warnings++;
  }
}

/*
 *  A reporter that sends diagnostics to collector.
 */

static SteelCheckReporter reporter_for_ ( Collector* collector )
{
  return (SteelCheckReporter) { .report = collect_, .context = collector };
}

/*
 *  True if diagnostic i exists and has the given severity, line, column, and a message that
 *  contains text. Prints the diagnostic otherwise, to help find the failing case.
 */

static bool diagnostic_is_ ( const Collector* collector, size_t i, SteelCheckSeverity severity,
                             size_t line, size_t column, const char* text )
{
  if ( i >= collector->count || i >= collector_capacity_ )
  {
    fprintf ( stderr, "    no diagnostic %zu (only %zu)\n", i, collector->count );
    return false;
  }

  const SteelCheckDiagnostic* d = &collector->items[i];

  bool ok = d->severity == severity && d->line == line && d->column == column
         && strstr ( d->message, text ) != nullptr;

  if ( !ok )
  {
    fprintf ( stderr, "    diagnostic %zu: %s at %zu:%zu: %s\n", i,
              steelcheck_severity_name ( d->severity ), d->line, d->column, d->message );
  }

  return ok;
}

static SteelCheckView v_ ( const char* string )
{
  return steelcheck_view_from_cstr ( string );
}

//==================================================================================================
//  Diagnostics
//==================================================================================================

static void test_report ()
{
  Collector          collector = {};
  SteelCheckReporter reporter  = reporter_for_ ( &collector );

  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER, 9, 3,
                      "column '%s' at %zu", "Ix_cm4", (size_t) 13 );

  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( diagnostic_is_ ( &collector, 0, STEELCHECK_SEVERITY_ERROR, 9, 3, "column 'Ix_cm4' at 13" ) );
  CHECK_INT_EQ ( collector.items[0].status, STEELCHECK_ERROR_HEADER );

  // A long message is truncated, and still '\0'-terminated.

  char long_text[400];

  memset ( long_text, 'a', sizeof long_text - 1 );
  long_text[sizeof long_text - 1] = '\0';

  steelcheck_report ( &reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_OK, 0, 0, "%s", long_text );

  CHECK_SIZE_EQ ( strlen( collector.items[1].message ), steelcheck_diagnostic_message_size - 1 );

  // No reporter, or a reporter without a callback: nothing happens.

  steelcheck_report ( nullptr, STEELCHECK_SEVERITY_ERROR, STEELCHECK_OK, 0, 0, "ignored" );
  steelcheck_report ( &(SteelCheckReporter) {}, STEELCHECK_SEVERITY_ERROR, STEELCHECK_OK, 0, 0,
                      "ignored" );

  CHECK_SIZE_EQ ( collector.count, 2 );
  CHECK_STR_EQ  ( steelcheck_severity_name( STEELCHECK_SEVERITY_WARNING ), "warning" );
  CHECK_STR_EQ  ( steelcheck_severity_name( STEELCHECK_SEVERITY_ERROR ), "error" );
}

//==================================================================================================
//  Line classification and metadata
//==================================================================================================

static void test_classify ()
{
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "" ), nullptr ),          STEELCHECK_LINE_BLANK );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( " \t " ), nullptr ),      STEELCHECK_LINE_BLANK );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "#" ), nullptr ),         STEELCHECK_LINE_COMMENT );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "  # note" ), nullptr ),  STEELCHECK_LINE_COMMENT );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "# key value" ), nullptr ), STEELCHECK_LINE_COMMENT );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "# two words: x" ), nullptr ), STEELCHECK_LINE_COMMENT );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "# : x" ), nullptr ),     STEELCHECK_LINE_COMMENT );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "designation,family" ), nullptr ), STEELCHECK_LINE_DATA );
  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "  W150x13.0,W" ), nullptr ), STEELCHECK_LINE_DATA );

  // '#' is special only at the start: inside data it is ordinary data.

  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "W150x13.0,#1" ), nullptr ), STEELCHECK_LINE_DATA );
}

static void test_metadata ()
{
  SteelCheckMetadata meta = {};

  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "# edition: 08/25" ), &meta ), STEELCHECK_LINE_METADATA );
  CHECK ( steelcheck_view_equals_cstr( meta.key, "edition" ) );
  CHECK ( steelcheck_view_equals_cstr( meta.value, "08/25" ) );

  // No space after '#', an empty value, and a ':' inside the value.

  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "#units:" ), &meta ), STEELCHECK_LINE_METADATA );
  CHECK ( steelcheck_view_equals_cstr( meta.key, "units" ) );
  CHECK ( steelcheck_view_is_empty( meta.value ) );

  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "  #  time_1 :  10:30  " ), &meta ),
                 STEELCHECK_LINE_METADATA );
  CHECK ( steelcheck_view_equals_cstr( meta.key, "time_1" ) );
  CHECK ( steelcheck_view_equals_cstr( meta.value, "10:30" ) );

  // For any other kind, metadata is reset to empty views.

  CHECK_INT_EQ ( steelcheck_csv_classify( v_( "# plain comment" ), &meta ), STEELCHECK_LINE_COMMENT );
  CHECK ( meta.key.data == nullptr && meta.value.data == nullptr );
}

//==================================================================================================
//  Field iterator
//==================================================================================================

/*
 *  Splits line and compares the fields, and their column numbers, with expected[0 .. count-1].
 */

static bool fields_match_ ( const char* line, size_t count, const char* const expected[] )
{
  SteelCheckFields iter   = {};
  SteelCheckView   field  = {};
  size_t           column = 0;

  steelcheck_fields_init ( &iter, v_( line ) );

  for ( size_t i = 0; i < count; i++ )
  {
    if ( !steelcheck_fields_next( &iter, &field, &column ) )
    {
      fprintf ( stderr, "    only %zu field(s), expected %zu\n", i, count );
      return false;
    }

    if ( column != i + 1 || !steelcheck_view_equals_cstr( field, expected[i] ) )
    {
      fprintf ( stderr, "    field %zu is \"%.*s\" (column %zu), expected \"%s\"\n", i + 1,
                steelcheck_view_print_length ( field ), field.data, column, expected[i] );
      return false;
    }
  }

  if ( steelcheck_fields_next( &iter, &field, &column ) )
  {
    fprintf ( stderr, "    more than the expected %zu field(s)\n", count );
    return false;
  }

  return column == 0 && field.length == 0;
}

static void test_fields ()
{
  CHECK ( fields_match_( "a,b,c",   3, (const char* const[]) { "a", "b", "c" } ) );
  CHECK ( fields_match_( " a , b ", 2, (const char* const[]) { "a", "b" } ) );
  CHECK ( fields_match_( "a,,c",    3, (const char* const[]) { "a", "", "c" } ) );
  CHECK ( fields_match_( "a,",      2, (const char* const[]) { "a", "" } ) );
  CHECK ( fields_match_( ",",       2, (const char* const[]) { "", "" } ) );
  CHECK ( fields_match_( "a",       1, (const char* const[]) { "a" } ) );
  CHECK ( fields_match_( "",        1, (const char* const[]) { "" } ) );
  CHECK ( fields_match_( nullptr,   1, (const char* const[]) { "" } ) );
  CHECK ( fields_match_( "\tW150x13.0 ,\t13.0", 2, (const char* const[]) { "W150x13.0", "13.0" } ) );

  // Quotes are not interpreted by the iterator: they are reported by the header and row
  // functions. Here a quoted field is simply split at its comma.

  CHECK ( fields_match_( "\"a,b\"", 2, (const char* const[]) { "\"a", "b\"" } ) );
}

static void test_fields_null_arguments ()
{
  SteelCheckFields iter   = {};
  SteelCheckView   field  = {};
  size_t           column = 0;

  steelcheck_fields_init ( nullptr, v_( "a" ) );   // must not crash
  steelcheck_fields_init ( &iter, v_( "a" ) );

  CHECK ( !steelcheck_fields_next( nullptr, &field, &column ) );
  CHECK ( !steelcheck_fields_next( &iter, nullptr, &column ) );
  CHECK ( !steelcheck_fields_next( &iter, &field, nullptr ) );

  CHECK ( steelcheck_fields_next( &iter, &field, &column ) );   // not consumed by the above
  CHECK ( steelcheck_view_equals_cstr( field, "a" ) );
}

//==================================================================================================
//  Header mapping
//==================================================================================================

/*
 *  A small spec table: three required columns and one optional.
 */

static const SteelCheckColumnSpec specs_[] =
{
  { .name = "designation", .required = true  },
  { .name = "d_mm",        .required = true  },
  { .name = "bf_mm",       .required = true  },
  { .name = "rt_cm",       .required = false },
};

constexpr size_t spec_count_ = sizeof specs_ / sizeof specs_[0];

enum { spec_designation_, spec_d_, spec_bf_, spec_rt_ };   // indices into specs_

static void test_header_ok ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};

  // Any order, and a missing optional column: no diagnostics.

  CHECK_INT_EQ ( steelcheck_csv_map_header ( &header, specs_, spec_count_,
                                             v_ ( " bf_mm , designation,d_mm" ), 9, &reporter ),
                 STEELCHECK_OK );

  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK_SIZE_EQ ( header.column_count, 3 );

  CHECK_SIZE_EQ ( header.spec_of_column[0], spec_bf_ );
  CHECK_SIZE_EQ ( header.spec_of_column[1], spec_designation_ );
  CHECK_SIZE_EQ ( header.spec_of_column[2], spec_d_ );

  CHECK_SIZE_EQ ( header.column_of_spec[spec_designation_], 1 );
  CHECK_SIZE_EQ ( header.column_of_spec[spec_rt_], steelcheck_csv_none );
}

static void test_header_unknown_column ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};

  // An unknown column is a warning only: the header is still valid, and the column is ignored.

  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, specs_, spec_count_,
                                             v_( "designation,color,d_mm,bf_mm" ), 9, &reporter ),
                 STEELCHECK_OK );

  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( diagnostic_is_( &collector, 0, STEELCHECK_SEVERITY_WARNING, 9, 2, "unknown column 'color'" ) );
  CHECK_SIZE_EQ ( header.spec_of_column[1], steelcheck_csv_none );
}

static void test_header_all_problems_reported ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};

  // Column 2 is empty, column 3 duplicates column 1, column 4 has a quote, column 5 has the wrong
  // case (unknown), and d_mm and bf_mm are missing: every problem is reported.

  CHECK_INT_EQ ( steelcheck_csv_map_header ( &header, specs_, spec_count_,
                                             v_( "designation,,designation,\"rt_cm\",D_MM" ), 4,
                                             &reporter ),
                 STEELCHECK_ERROR_HEADER );

  CHECK_SIZE_EQ ( collector.count, 6 );
  CHECK_SIZE_EQ ( collector.errors, 5 );
  CHECK_SIZE_EQ ( collector.warnings, 1 );

  CHECK ( diagnostic_is_( &collector, 0, STEELCHECK_SEVERITY_ERROR,   4, 2, "empty column name" ) );
  CHECK ( diagnostic_is_( &collector, 1, STEELCHECK_SEVERITY_ERROR,   4, 3, "duplicate column 'designation' (also column 1)" ) );
  CHECK ( diagnostic_is_( &collector, 2, STEELCHECK_SEVERITY_ERROR,   4, 4, "quotes" ) );
  CHECK ( diagnostic_is_( &collector, 3, STEELCHECK_SEVERITY_WARNING, 4, 5, "unknown column 'D_MM'" ) );
  CHECK ( diagnostic_is_( &collector, 4, STEELCHECK_SEVERITY_ERROR,   4, 0, "missing required column 'd_mm'" ) );
  CHECK ( diagnostic_is_( &collector, 5, STEELCHECK_SEVERITY_ERROR,   4, 0, "missing required column 'bf_mm'" ) );
}

static void test_header_too_many_columns ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};

  // 65 columns: designation, d_mm, bf_mm, then 62 more.

  char line[1024] = "designation,d_mm,bf_mm";

  for ( int i = 0; i < 62; i++ )
  {
    char extra[16];
    snprintf ( extra, sizeof extra, ",c%d", i );
    strcat ( line, extra );
  }

  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, specs_, spec_count_, v_( line ), 1, &reporter ),
                 STEELCHECK_ERROR_HEADER );

  // The warnings for the 61 unknown columns that fit come first; the last diagnostic is the error.

  CHECK_SIZE_EQ ( collector.errors, 1 );
  CHECK_SIZE_EQ ( collector.warnings, 61 );
}

static void test_header_invalid_arguments ()
{
  SteelCheckCsvHeader header = {};

  CHECK_INT_EQ ( steelcheck_csv_map_header( nullptr, specs_, spec_count_, v_( "a" ), 1, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, nullptr, spec_count_, v_( "a" ), 1, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, specs_, 0, v_( "a" ), 1, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  // Without a reporter, problems are not reported, but the result is the same.

  CHECK_INT_EQ ( steelcheck_csv_map_header ( &header, specs_, spec_count_, v_( "designation" ), 1,
                                             nullptr ),
                 STEELCHECK_ERROR_HEADER );
}

//==================================================================================================
//  Data rows
//==================================================================================================

static void test_rows ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};
  SteelCheckView      fields[spec_count_];

  CHECK_INT_EQ ( steelcheck_csv_map_header ( &header, specs_, spec_count_,
                                             v_( "bf_mm,color,designation,d_mm" ), 1, &reporter ),
                 STEELCHECK_OK );

  // Fields arrive by spec: the unknown column (color) is skipped, and rt_cm, which the file does
  // not have, is an empty view.

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "100, red ,W150x13.0,148" ), 2, fields,
                                            spec_count_, &reporter ),
                 STEELCHECK_OK );

  CHECK ( steelcheck_view_equals_cstr( fields[spec_designation_], "W150x13.0" ) );
  CHECK ( steelcheck_view_equals_cstr( fields[spec_d_],           "148" ) );
  CHECK ( steelcheck_view_equals_cstr( fields[spec_bf_],          "100" ) );
  CHECK ( steelcheck_view_is_empty( fields[spec_rt_] ) );

  // An empty field is allowed here (whether a value is required is checked later).

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "100,,W150x13.0," ), 3, fields,
                                            spec_count_, &reporter ),
                 STEELCHECK_OK );
  CHECK ( steelcheck_view_is_empty( fields[spec_d_] ) );

  // The warning for 'color' is the only diagnostic so far.

  CHECK_SIZE_EQ ( collector.count, 1 );
}

static void test_rows_errors ()
{
  Collector           collector = {};
  SteelCheckReporter  reporter  = reporter_for_ ( &collector );
  SteelCheckCsvHeader header    = {};
  SteelCheckView      fields[spec_count_];

  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, specs_, spec_count_,
                                             v_( "designation,d_mm,bf_mm" ), 9, &reporter ),
                 STEELCHECK_OK );

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "W150x13.0,148" ), 10, fields,
                                            spec_count_, &reporter ),
                 STEELCHECK_ERROR_SYNTAX );
  CHECK ( diagnostic_is_( &collector, 0, STEELCHECK_SEVERITY_ERROR, 10, 0,
                           "2 fields, but the header (line 9) has 3 columns" ) );

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "W150x13.0,148,100,7" ), 11, fields,
                                            spec_count_, &reporter ),
                 STEELCHECK_ERROR_SYNTAX );
  CHECK ( diagnostic_is_( &collector, 1, STEELCHECK_SEVERITY_ERROR, 11, 0, "4 fields" ) );

  // A quote is reported at its column, and a rejected row leaves no half-filled fields.

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "W150x13.0,\"148\",100" ), 12, fields,
                                            spec_count_, &reporter ),
                 STEELCHECK_ERROR_SYNTAX );
  CHECK ( diagnostic_is_( &collector, 2, STEELCHECK_SEVERITY_ERROR, 12, 2, "quotes" ) );
  CHECK ( steelcheck_view_is_empty( fields[spec_designation_] ) );

  // A '\0' byte inside a field (a view has an explicit length, so it can hold one).

  static constexpr char with_nul[] = "W150x13.0,1\0" "48,100";

  CHECK_INT_EQ ( steelcheck_csv_split_row ( &header, (SteelCheckView) { sizeof with_nul - 1, with_nul },
                                            13, fields, spec_count_, &reporter ),
                 STEELCHECK_ERROR_SYNTAX );
  CHECK ( diagnostic_is_( &collector, 3, STEELCHECK_SEVERITY_ERROR, 13, 2, "'\\0' byte" ) );

  // Too small a fields array.

  CHECK_INT_EQ ( steelcheck_csv_split_row( &header, v_( "a,b,c" ), 14, fields, 2, &reporter ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
}

//==================================================================================================
//  The Gerdau catalog, end to end
//==================================================================================================

/*
 *  Every column of the catalog, plus r_mm, which the catalog does not have (optional). 
 */

static const SteelCheckColumnSpec gerdau_specs_[] =
{
  { "designation", true  }, { "family",  true  }, { "h_shape", false }, { "on_request", false },
  { "mass_kg_m",   true  }, { "d_mm",    true  }, { "bf_mm",   true  }, { "tw_mm",      true  },
  { "tf_mm",       true  }, { "h_mm",    true  }, { "dp_mm",   true  }, { "r_mm",       false },
  { "A_cm2",       true  }, { "Ix_cm4",  true  }, { "Wx_cm3",  true  }, { "rx_cm",      true  },
  { "Zx_cm3",      true  }, { "Iy_cm4",  true  }, { "Wy_cm3",  true  }, { "ry_cm",      true  },
  { "Zy_cm3",      true  }, { "rt_cm",   false }, { "J_cm4",   true  }, { "lambda_f",   false },
  { "lambda_w",    false }, { "Cw_cm6",  true  }, { "u_m2_m",  false },
  { "designation_imperial", false },
};

constexpr size_t gerdau_spec_count_ = sizeof gerdau_specs_ / sizeof gerdau_specs_[0];

static void test_gerdau_catalog ()
{
  Collector          collector = {};
  SteelCheckReporter reporter  = reporter_for_ ( &collector );
  SteelCheckBuffer   buff      = {};

  CHECK_INT_EQ ( steelcheck_buffer_load( &buff, "data/gerdau_w_hp.csv" ), STEELCHECK_OK );

  SteelCheckLines     lines       = {};
  SteelCheckView      line        = {};
  size_t              line_number = 0;
  SteelCheckMetadata  meta        = {};
  SteelCheckCsvHeader header      = {};
  SteelCheckView      fields[gerdau_spec_count_];
  SteelCheckView      last_designation = {};

  bool   have_header = false;
  size_t metadata    = 0;
  size_t comments    = 0;
  size_t rows        = 0;

  steelcheck_lines_init ( &lines, steelcheck_buffer_view( &buff ) );

  while ( steelcheck_lines_next( &lines, &line, &line_number ) )
  {
    switch ( steelcheck_csv_classify( line, &meta ) )
    {
      case STEELCHECK_LINE_BLANK:
        break;

      case STEELCHECK_LINE_COMMENT:
        comments++;
        break;

      case STEELCHECK_LINE_METADATA:
        metadata++;
        if ( steelcheck_view_equals_cstr( meta.key, "edition" ) )
        {
          CHECK ( steelcheck_view_equals_cstr( meta.value, "08/25" ) );
        }
        break;

      case STEELCHECK_LINE_DATA:
        if ( !have_header )
        {
          CHECK_INT_EQ ( steelcheck_csv_map_header( &header, gerdau_specs_, gerdau_spec_count_,
                                                    line, line_number, &reporter ),
                         STEELCHECK_OK );
          CHECK_SIZE_EQ ( line_number, 9 );
          have_header = true;
        }
        else
        {
          CHECK_INT_EQ ( steelcheck_csv_split_row( &header, line, line_number, fields,
                                                   gerdau_spec_count_, &reporter ),
                         STEELCHECK_OK );
          last_designation = fields[0];
          rows++;
        }
        break;
    }
  }

  // 7 metadata lines (catalog, table, edition, standard, units, h_shape, note) and 1 comment
  // (the fillet radius line has no "key:"), the header, and 108 rows, with no diagnostics.

  CHECK_SIZE_EQ ( metadata, 7 );
  CHECK_SIZE_EQ ( comments, 1 );
  CHECK_SIZE_EQ ( rows, 108 );
  CHECK_SIZE_EQ ( header.column_count, 27 );
  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK ( steelcheck_view_equals_cstr( last_designation, "W610x217.0" ) );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  test_csv_suite
//==================================================================================================

void test_csv_suite ()
{
  RUN_TEST ( test_report );
  RUN_TEST ( test_classify );
  RUN_TEST ( test_metadata );
  RUN_TEST ( test_fields );
  RUN_TEST ( test_fields_null_arguments );
  RUN_TEST ( test_header_ok );
  RUN_TEST ( test_header_unknown_column );
  RUN_TEST ( test_header_all_problems_reported );
  RUN_TEST ( test_header_too_many_columns );
  RUN_TEST ( test_header_invalid_arguments );
  RUN_TEST ( test_rows );
  RUN_TEST ( test_rows_errors );
  RUN_TEST ( test_gerdau_catalog );
}
