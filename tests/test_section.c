#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"
#include "steelcheck/csv.h"
#include "steelcheck/lines.h"
#include "steelcheck/section.h"
#include "suites.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Test reporter (same idea as in test_csv.c)
//==================================================================================================

constexpr size_t collector_capacity_ = 16;

typedef struct Collector
{
  size_t               count;
  SteelCheckDiagnostic items[collector_capacity_];
} Collector;

static void collect_ ( void* context, const SteelCheckDiagnostic* diagnostic )
{
  Collector* collector = context;

  if ( collector->count < collector_capacity_ )
  {
    collector->items[collector->count] = *diagnostic;
  }

  collector->count++;
}

/*
 *  True if diagnostic i exists, is at column, and its message contains text.
 */

static bool diagnostic_is_ ( const Collector* collector, size_t i, size_t column, const char* text )
{
  if ( i >= collector->count || i >= collector_capacity_ )
  {
    fprintf ( stderr, "    no diagnostic %zu (only %zu)\n", i, collector->count );
    return false;
  }

  const SteelCheckDiagnostic* d = &collector->items[i];

  if ( d->column != column || strstr( d->message, text ) == nullptr )
  {
    fprintf ( stderr, "    diagnostic %zu at column %zu: %s\n", i, d->column, d->message );
    return false;
  }

  return true;
}

//==================================================================================================
//  Rows used by the tests
//==================================================================================================

/*
 *  The catalog's header line and its first section, W150x13.0.
 */

static constexpr char header_line_[] =
  "designation,family,h_shape,on_request,mass_kg_m,d_mm,bf_mm,tw_mm,tf_mm,h_mm,dp_mm,A_cm2,"
  "Ix_cm4,Wx_cm3,rx_cm,Zx_cm3,Iy_cm4,Wy_cm3,ry_cm,Zy_cm3,rt_cm,J_cm4,lambda_f,lambda_w,Cw_cm6,"
  "u_m2_m,designation_imperial";

static constexpr char w150_row_[] =
  "W150x13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,96.4,82,16.4,2.22,25.5,2.60,"
  "1.72,10.20,27.49,4181,0.67,W6x8.5";

/*
 *  Maps the header, splits row, and converts it. Diagnostics go to collector (may be nullptr).
 */

static SteelCheckStatus convert_ ( const char* header_line, const char* row,
                                   SteelCheckSection* section, Collector* collector )
{
  SteelCheckReporter  reporter = { .report = collect_, .context = collector };
  SteelCheckCsvHeader header   = {};
  SteelCheckView      fields[STEELCHECK_FIELD_COUNT];

  const SteelCheckReporter* r = collector != nullptr ? &reporter : nullptr;

  SteelCheckStatus status = steelcheck_csv_map_header ( &header, steelcheck_section_columns(),
                                                        STEELCHECK_FIELD_COUNT,
                                                        steelcheck_view_from_cstr( header_line ),
                                                        1, r );
  if ( status != STEELCHECK_OK )
  {
    return status;
  }

  status = steelcheck_csv_split_row ( &header, steelcheck_view_from_cstr( row ), 2, fields,
                                      STEELCHECK_FIELD_COUNT, r );
  if ( status != STEELCHECK_OK )
  {
    return status;
  }

  return steelcheck_section_from_row ( section, &header, fields, 2, r );
}

//==================================================================================================
//  Column table
//==================================================================================================

static void test_columns_table ()
{
  const SteelCheckColumnSpec* columns  = steelcheck_section_columns ();
  size_t                      required = 0;

  // Every field has a name (a forgotten designated initializer would leave nullptr), and no two
  // names are the same.

  for ( size_t f = 0; f < STEELCHECK_FIELD_COUNT; f++ )
  {
    CHECK ( columns[f].name != nullptr && columns[f].name[0] != '\0' );

    for ( size_t g = 0; g < f && columns[f].name != nullptr; g++ )
    {
      CHECK ( columns[g].name == nullptr || strcmp( columns[f].name, columns[g].name ) != 0 );
    }

    required += columns[f].required ? 1 : 0;
  }

  CHECK_SIZE_EQ ( required, 20 );
  CHECK_STR_EQ ( columns[STEELCHECK_FIELD_IX].name, "Ix_cm4" );
  CHECK_STR_EQ ( columns[STEELCHECK_FIELD_DESIGNATION_IMPERIAL].name, "designation_imperial" );
}

//==================================================================================================
//  Conversion of one row
//==================================================================================================

static void test_row_w150 ()
{
  Collector         collector = {};
  SteelCheckSection s         = {};

  CHECK_INT_EQ ( convert_( header_line_, w150_row_, &s, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // Identity.

  CHECK_STR_EQ ( s.designation, "W150x13.0" );
  CHECK_STR_EQ ( s.designation_imperial, "W6x8.5" );
  CHECK_INT_EQ ( s.family, STEELCHECK_FAMILY_W );
  CHECK ( !s.h_shape && !s.on_request );
  CHECK_SIZE_EQ ( s.source_line, 2 );

  // Values in internal units (mm based): catalog value x conversion factor.

  CHECK_NEAR ( s.mass, 13.0,         1e-12 );   // kg/m, unchanged
  CHECK_NEAR ( s.d,    148.0,        1e-12 );   // mm
  CHECK_NEAR ( s.tw,   4.3,          1e-12 );   // mm
  CHECK_NEAR ( s.A,    1660.0,       1e-12 );   // 16.6 cm^2
  CHECK_NEAR ( s.Ix,   6.35e6,       1e-12 );   // 635 cm^4
  CHECK_NEAR ( s.Wx,   85.8e3,       1e-12 );   // 85.8 cm^3
  CHECK_NEAR ( s.rx,   61.8,         1e-12 );   // 6.18 cm
  CHECK_NEAR ( s.ry,   22.2,         1e-12 );   // 2.22 cm
  CHECK_NEAR ( s.rt,   26.0,         1e-12 );   // 2.60 cm
  CHECK_NEAR ( s.J,    1.72e4,       1e-12 );   // 1.72 cm^4
  CHECK_NEAR ( s.Cw,   4.181e9,      1e-12 );   // 4181 cm^6
  CHECK_NEAR ( s.u,    0.67,         1e-12 );   // m^2/m, unchanged
  CHECK_NEAR ( s.lambda_f, 10.20,    1e-12 );   // ratio, unchanged

  // r is not a column of the catalog: absent, value 0. rt is present.

  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_R ) );
  CHECK_NEAR ( s.r, 0.0, 0.0 );
  CHECK (  steelcheck_section_has( &s, STEELCHECK_FIELD_RT ) );
  CHECK (  steelcheck_section_has( &s, STEELCHECK_FIELD_DESIGNATION ) );

  // Out-of-range field numbers are simply "not present".

  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_COUNT ) );
  CHECK ( !steelcheck_section_has( &s, (SteelCheckField) -1 ) );
  CHECK ( !steelcheck_section_has( nullptr, STEELCHECK_FIELD_D ) );
}

static void test_row_optional_columns ()
{
  // Only the required columns, in another order, plus r_mm: everything optional is absent.

  static constexpr char header[] =
    "family,designation,mass_kg_m,d_mm,bf_mm,tw_mm,tf_mm,h_mm,dp_mm,r_mm,A_cm2,Ix_cm4,Wx_cm3,"
    "rx_cm,Zx_cm3,Iy_cm4,Wy_cm3,ry_cm,Zy_cm3,J_cm4,Cw_cm6";

  static constexpr char row[] =
    "HP,HP200x53.0,53.0,204,207,11.3,11.3,181,161,10,68.1,4977,488.0,8.55,551.3,1673,161.7,4.96,"
    "248.6,31.93,155075";

  Collector         collector = {};
  SteelCheckSection s         = {};

  CHECK_INT_EQ ( convert_( header, row, &s, &collector ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  CHECK_INT_EQ ( s.family, STEELCHECK_FAMILY_HP );
  CHECK_STR_EQ ( s.designation, "HP200x53.0" );
  CHECK_STR_EQ ( s.designation_imperial, "" );
  CHECK_NEAR ( s.r, 10.0, 1e-12 );

  CHECK (  steelcheck_section_has( &s, STEELCHECK_FIELD_R ) );
  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_RT ) );
  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_H_SHAPE ) );
  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_DESIGNATION_IMPERIAL ) );
}

static void test_row_empty_optional_field ()
{
  // rt_cm (column 21) present in the header but empty in the row: allowed, field absent.

  static constexpr char row[] =
    "W150x13.0,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,6.18,96.4,82,16.4,2.22,25.5,,"
    "1.72,10.20,27.49,4181,0.67,W6x8.5";

  SteelCheckSection s = {};

  CHECK_INT_EQ ( convert_( header_line_, row, &s, nullptr ), STEELCHECK_OK );
  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_RT ) );
}

//==================================================================================================
//  Problems in a row
//==================================================================================================

static void test_row_problems ()
{
  // Column 2 family "X", column 3 h_shape "2", column 6 d "12mm", column 13 Ix empty (required),
  // column 25 Cw "1e400": all five are reported, each with its column.

  static constexpr char row[] =
    "W150x13.0,X,2,0,13.0,12mm,100,4.3,4.9,138,118,16.6,,85.8,6.18,96.4,82,16.4,2.22,25.5,2.60,"
    "1.72,10.20,27.49,1e400,0.67,W6x8.5";

  Collector         collector = {};
  SteelCheckSection s         = {};

  CHECK_INT_EQ ( convert_( header_line_, row, &s, &collector ), STEELCHECK_ERROR_VALUE );

  CHECK_SIZE_EQ ( collector.count, 5 );
  CHECK ( diagnostic_is_( &collector, 0,  2, "column 'family': expected W or HP, got 'X'" ) );
  CHECK ( diagnostic_is_( &collector, 1,  3, "column 'h_shape': expected 0 or 1, got '2'" ) );
  CHECK ( diagnostic_is_( &collector, 2,  6, "column 'd_mm': invalid number '12mm'" ) );
  CHECK ( diagnostic_is_( &collector, 3, 13, "column 'Ix_cm4': missing value" ) );
  CHECK ( diagnostic_is_( &collector, 4, 25, "column 'Cw_cm6': number out of range '1e400'" ) );

  // A rejected row leaves an all-zero section, not a half-filled one.

  CHECK_STR_EQ ( s.designation, "" );
  CHECK ( s.present == 0 );
}

static void test_row_long_designation ()
{
  static constexpr char row[] =
    "W150x13.0-this-designation-is-too-long,W,0,0,13.0,148,100,4.3,4.9,138,118,16.6,635,85.8,"
    "6.18,96.4,82,16.4,2.22,25.5,2.60,1.72,10.20,27.49,4181,0.67,W6x8.5";

  Collector         collector = {};
  SteelCheckSection s         = {};

  CHECK_INT_EQ ( convert_( header_line_, row, &s, &collector ), STEELCHECK_ERROR_VALUE );
  CHECK ( diagnostic_is_( &collector, 0, 1, "is longer than 31 characters" ) );
}

static void test_row_invalid_arguments ()
{
  // A header mapped with another spec table cannot be used: fields[f] would not be field f.

  static const SteelCheckColumnSpec other[] = { { .name = "designation", .required = true } };

  SteelCheckCsvHeader header = {};
  SteelCheckView      fields[STEELCHECK_FIELD_COUNT] = {};
  SteelCheckSection   s      = {};

  CHECK_INT_EQ ( steelcheck_csv_map_header( &header, other, 1,
                                            steelcheck_view_from_cstr( "designation" ), 1,
                                            nullptr ),
                 STEELCHECK_OK );

  CHECK_INT_EQ ( steelcheck_section_from_row( &s, &header, fields, 2, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_section_from_row( nullptr, &header, fields, 2, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_section_from_row( &s, nullptr, fields, 2, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  CHECK_STR_EQ ( steelcheck_family_name( STEELCHECK_FAMILY_HP ), "HP" );
}

//==================================================================================================
//  The Gerdau catalog, end to end
//==================================================================================================

static void test_gerdau_sections ()
{
  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckBuffer   buff      = {};

  CHECK_INT_EQ ( steelcheck_buffer_load( &buff, "data/gerdau_w_hp.csv" ), STEELCHECK_OK );

  SteelCheckLines     lines       = {};
  SteelCheckView      line        = {};
  size_t              line_number = 0;
  SteelCheckCsvHeader header      = {};
  SteelCheckView      fields[STEELCHECK_FIELD_COUNT];
  SteelCheckSection   section     = {};
  SteelCheckSection   first       = {};
  SteelCheckSection   last        = {};

  bool   have_header = false;
  size_t sections    = 0;
  size_t hp          = 0;
  size_t h_shape     = 0;
  size_t on_request  = 0;

  steelcheck_lines_init ( &lines, steelcheck_buffer_view( &buff ) );

  while ( steelcheck_lines_next( &lines, &line, &line_number ) )
  {
    if ( steelcheck_csv_classify( line, nullptr ) != STEELCHECK_LINE_DATA )
    {
      continue;
    }

    if ( !have_header )
    {
      CHECK_INT_EQ ( steelcheck_csv_map_header( &header, steelcheck_section_columns(),
                                                STEELCHECK_FIELD_COUNT, line, line_number,
                                                &reporter ),
                     STEELCHECK_OK );
      have_header = true;
      continue;
    }

    if ( steelcheck_csv_split_row( &header, line, line_number, fields, STEELCHECK_FIELD_COUNT,
                                   &reporter ) != STEELCHECK_OK
         || steelcheck_section_from_row( &section, &header, fields, line_number,
                                         &reporter ) != STEELCHECK_OK )
    {
      continue;
    }

    if ( sections == 0 )
    {
      first = section;
    }

    last = section;
    sections++;
    hp         += section.family == STEELCHECK_FAMILY_HP ? 1 : 0;
    h_shape    += section.h_shape ? 1 : 0;
    on_request += section.on_request ? 1 : 0;
  }

  CHECK_SIZE_EQ ( collector.count, 0 );
  CHECK_SIZE_EQ ( sections, 108 );
  CHECK_SIZE_EQ ( hp, 8 );
  CHECK_SIZE_EQ ( h_shape, 39 );
  CHECK_SIZE_EQ ( on_request, 16 );

  CHECK_STR_EQ ( first.designation, "W150x13.0" );
  CHECK_SIZE_EQ ( first.source_line, 10 );

  CHECK_STR_EQ ( last.designation, "W610x217.0" );
  CHECK_SIZE_EQ ( last.source_line, 117 );
  CHECK_NEAR ( last.Ix, 191395.0e4,   1e-12 );   // 191395 cm^4
  CHECK_NEAR ( last.Cw, 14676643.0e6, 1e-12 );   // 14676643 cm^6
  CHECK_NEAR ( last.ry, 76.6,         1e-12 );   // 7.66 cm

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  Validation: derived values
//==================================================================================================

/*
 *  The W150x13.0 row, converted. The validation tests change single values of it.
 */

static SteelCheckSection w150_ ()
{
  SteelCheckSection s = {};

  if ( convert_( header_line_, w150_row_, &s, nullptr ) != STEELCHECK_OK )
  {
    fprintf ( stderr, "    cannot convert the W150x13.0 row\n" );
  }

  return s;
}

static void test_validate_derived ()
{
  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // r = (h - d') / 2 = (138 - 118) / 2, computed because the catalog does not give it.

  CHECK_NEAR ( s.r, 10.0, 1e-12 );
  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_R ) );
  CHECK (  steelcheck_section_was_computed( &s, STEELCHECK_FIELD_R ) );

  // h0 = d - tf, r0 = sqrt(rx^2 + ry^2).

  CHECK_NEAR ( s.h0, 148.0 - 4.9, 1e-12 );
  CHECK_NEAR ( s.r0, sqrt( 61.8 * 61.8 + 22.2 * 22.2 ), 1e-12 );

  // The slenderness ratios are now exact: 100 / 9.8 and 118 / 4.3, not the printed 10.20, 27.49.

  CHECK_NEAR ( s.lambda_f, 100.0 / 9.8, 1e-12 );
  CHECK_NEAR ( s.lambda_w, 118.0 / 4.3, 1e-12 );
  CHECK ( steelcheck_section_has( &s, STEELCHECK_FIELD_LAMBDA_F ) );        // given in the file
  CHECK ( steelcheck_section_was_computed( &s, STEELCHECK_FIELD_LAMBDA_F ) ); // and replaced

  // Fields that are read, not computed.

  CHECK ( !steelcheck_section_was_computed( &s, STEELCHECK_FIELD_D ) );
  CHECK ( !steelcheck_section_was_computed( &s, STEELCHECK_FIELD_COUNT ) );
  CHECK ( !steelcheck_section_was_computed( nullptr, STEELCHECK_FIELD_R ) );
}

static void test_validate_given_r ()
{
  // A file that gives r_mm (HP200x53.0, r = 10 mm): r is kept, not recomputed.

  static constexpr char header[] =
    "family,designation,mass_kg_m,d_mm,bf_mm,tw_mm,tf_mm,h_mm,dp_mm,r_mm,A_cm2,Ix_cm4,Wx_cm3,"
    "rx_cm,Zx_cm3,Iy_cm4,Wy_cm3,ry_cm,Zy_cm3,J_cm4,Cw_cm6";

  static constexpr char row[] =
    "HP,HP200x53.0,53.0,204,207,11.3,11.3,181,161,10,68.1,4977,488.0,8.55,551.3,1673,161.7,4.96,"
    "248.6,31.93,155075";

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = {};

  CHECK_INT_EQ ( convert_( header, row, &s, nullptr ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  CHECK_NEAR ( s.r, 10.0, 1e-12 );
  CHECK ( !steelcheck_section_was_computed( &s, STEELCHECK_FIELD_R ) );

  // lambda_f and lambda_w are not in this file: simply computed.

  CHECK ( !steelcheck_section_has( &s, STEELCHECK_FIELD_LAMBDA_F ) );
  CHECK_NEAR ( s.lambda_f, 207.0 / ( 2.0 * 11.3 ), 1e-12 );
}

//==================================================================================================
//  Validation: errors
//==================================================================================================

static void test_validate_errors ()
{
  // Five errors at once: a negative mass, tf too thick for d, d' larger than h, Ix and Iy
  // swapped, and a W designation for an HP section. All are reported.

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();

  double ix = s.Ix;

  s.mass   = -13.0;
  s.tf     = 80.0;
  s.dp     = 140.0;
  s.Ix     = s.Iy;
  s.Iy     = ix;
  s.family = STEELCHECK_FAMILY_HP;

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_ERROR_VALUE );

  CHECK_SIZE_EQ ( collector.count, 5 );
  CHECK ( diagnostic_is_( &collector, 0, 0, "W150x13.0: mass_kg_m must be positive, got -13" ) );
  CHECK ( diagnostic_is_( &collector, 1, 0, "tf_mm 80 is not less than d / 2 = 74" ) );
  CHECK ( diagnostic_is_( &collector, 2, 0, "dp_mm 140 is greater than h_mm 138" ) );
  CHECK ( diagnostic_is_( &collector, 3, 0,
                          "Iy_cm4 635 is greater than Ix_cm4 82 (axes swapped?)" ) );
  CHECK ( diagnostic_is_( &collector, 4, 0, "designation does not match family HP" ) );

  for ( size_t i = 0; i < 5; i++ )
  {
    CHECK_INT_EQ ( collector.items[i].severity, STEELCHECK_SEVERITY_ERROR );
    CHECK_INT_EQ ( collector.items[i].status, STEELCHECK_ERROR_VALUE );
    CHECK_SIZE_EQ ( collector.items[i].line, 2 );
  }

  // Nothing was derived from the rejected values.

  CHECK_NEAR ( s.r, 0.0, 0.0 );
  CHECK ( s.computed == 0 );
}

static void test_validate_more_errors ()
{
  // The remaining geometric relations, and a zero value (not positive either).

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();

  s.tw = 120.0;    // wider than bf = 100
  s.h  = 150.0;    // deeper than d = 148 (then also d' <= h still holds)
  s.Zx = 80.0e3;   // below Wx = 85.8 cm^3
  s.Zy = 10.0e3;   // below Wy = 16.4 cm^3
  s.J  = 0.0;

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_ERROR_VALUE );

  CHECK_SIZE_EQ ( collector.count, 5 );
  CHECK ( diagnostic_is_( &collector, 0, 0, "J_cm4 must be positive, got 0" ) );
  CHECK ( diagnostic_is_( &collector, 1, 0, "tw_mm 120 is not less than bf_mm 100" ) );
  CHECK ( diagnostic_is_( &collector, 2, 0, "h_mm 150 is not less than d_mm 148" ) );
  CHECK ( diagnostic_is_( &collector, 3, 0, "Zx_cm3 80 is less than Wx_cm3 85.8" ) );
  CHECK ( diagnostic_is_( &collector, 4, 0, "Zy_cm3 10 is less than Wy_cm3 16.4" ) );
}

//==================================================================================================
//  Validation: warnings
//==================================================================================================

static void test_validate_warnings ()
{
  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();

  // Wx 10 % too large, and a wrongly printed lambda_f: warnings, the section is still valid.

  s.Wx       = 1.10 * 85.8e3;
  s.lambda_f = 12.0;

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );

  CHECK_SIZE_EQ ( collector.count, 2 );
  CHECK ( diagnostic_is_( &collector, 0, 0,
                          "W150x13.0: Wx_cm3 94.38 differs from 2 Ix / d = 85.81 (+10.0 %)" ) );
  CHECK ( diagnostic_is_( &collector, 1, 0,
                          "lambda_f 12 differs from bf / (2 tf) = 10.2 (+17.6 %)" ) );

  CHECK_INT_EQ ( collector.items[0].severity, STEELCHECK_SEVERITY_WARNING );
  CHECK_INT_EQ ( collector.items[0].status, STEELCHECK_ERROR_INCONSISTENT );

  // The printed lambda_f was replaced by the computed one.

  CHECK_NEAR ( s.lambda_f, 100.0 / 9.8, 1e-12 );
}

static void test_validate_tolerances ()
{
  // Just inside and just outside the exact tolerance (1 %) for rx = 61.8 mm (6.18 cm).

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();
  double             rx        = sqrt( s.Ix / s.A );

  s.rx = rx * ( 1.0 + 0.009 );
  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  s = w150_ ();
  s.rx = rx * ( 1.0 + 0.011 );
  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( diagnostic_is_( &collector, 0, 0, "rx_cm" ) );

  // The mass has a wider tolerance (5 %): 4 % below A x 7850 kg/m^3 is accepted.

  collector = (Collector) {};
  s         = w150_ ();
  s.mass    = 0.96 * s.A * 1e-6 * 7850.0;
  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );

  // ... but then the designation's 13.0 no longer matches (more than 0.5 %).

  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( diagnostic_is_( &collector, 0, 0, "differs from the designation's 13" ) );
}

static void test_validate_designation_without_mass ()
{
  // A designation without a number after 'x' is accepted; only its mass check is skipped.

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckSection  s         = w150_ ();

  strcpy ( s.designation, "W150-light" );

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // Too short to have a digit after the family name.

  strcpy ( s.designation, "W" );

  CHECK_INT_EQ ( steelcheck_section_validate( &s, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK_INT_EQ ( steelcheck_section_validate( nullptr, &reporter ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
}

//==================================================================================================
//  Validation: the Gerdau catalog
//==================================================================================================

static void test_gerdau_validation ()
{
  // Every section is valid; the only finding is the known W610x153.0 inconsistency of the
  // catalog itself (mass printed as 154.2 kg/m).

  Collector          collector = {};
  SteelCheckReporter reporter  = { .report = collect_, .context = &collector };
  SteelCheckBuffer   buff      = {};

  CHECK_INT_EQ ( steelcheck_buffer_load( &buff, "data/gerdau_w_hp.csv" ), STEELCHECK_OK );

  SteelCheckLines     lines       = {};
  SteelCheckView      line        = {};
  size_t              line_number = 0;
  SteelCheckCsvHeader header      = {};
  SteelCheckView      fields[STEELCHECK_FIELD_COUNT];
  SteelCheckSection   section     = {};
  bool                have_header = false;
  size_t              valid       = 0;
  size_t              computed_r  = 0;

  steelcheck_lines_init ( &lines, steelcheck_buffer_view( &buff ) );

  while ( steelcheck_lines_next( &lines, &line, &line_number ) )
  {
    if ( steelcheck_csv_classify( line, nullptr ) != STEELCHECK_LINE_DATA )
    {
      continue;
    }

    if ( !have_header )
    {
      have_header = steelcheck_csv_map_header( &header, steelcheck_section_columns(),
                                               STEELCHECK_FIELD_COUNT, line, line_number,
                                               &reporter ) == STEELCHECK_OK;
      continue;
    }

    if ( steelcheck_csv_split_row( &header, line, line_number, fields, STEELCHECK_FIELD_COUNT,
                                   &reporter ) == STEELCHECK_OK
         && steelcheck_section_from_row( &section, &header, fields, line_number,
                                         &reporter ) == STEELCHECK_OK
         && steelcheck_section_validate( &section, &reporter ) == STEELCHECK_OK )
    {
      valid++;
      computed_r += steelcheck_section_was_computed( &section, STEELCHECK_FIELD_R ) ? 1 : 0;
    }
  }

  CHECK_SIZE_EQ ( valid, 108 );
  CHECK_SIZE_EQ ( computed_r, 108 );
  CHECK_SIZE_EQ ( collector.count, 1 );
  CHECK ( diagnostic_is_( &collector, 0, 0,
                          "W610x153.0: mass_kg_m 154.2 differs from the designation's 153" ) );
  CHECK_SIZE_EQ ( collector.items[0].line, 113 );
  CHECK_INT_EQ ( collector.items[0].severity, STEELCHECK_SEVERITY_WARNING );

  steelcheck_buffer_free ( &buff );
}

//==================================================================================================
//  test_section_suite
//==================================================================================================

void test_section_suite ()
{
  RUN_TEST ( test_columns_table );
  RUN_TEST ( test_row_w150 );
  RUN_TEST ( test_row_optional_columns );
  RUN_TEST ( test_row_empty_optional_field );
  RUN_TEST ( test_row_problems );
  RUN_TEST ( test_row_long_designation );
  RUN_TEST ( test_row_invalid_arguments );
  RUN_TEST ( test_gerdau_sections );
  RUN_TEST ( test_validate_derived );
  RUN_TEST ( test_validate_given_r );
  RUN_TEST ( test_validate_errors );
  RUN_TEST ( test_validate_more_errors );
  RUN_TEST ( test_validate_warnings );
  RUN_TEST ( test_validate_tolerances );
  RUN_TEST ( test_validate_designation_without_mass );
  RUN_TEST ( test_gerdau_validation );
}
