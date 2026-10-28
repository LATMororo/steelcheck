#include "steelcheck/section.h"

#include "steelcheck/number.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Unit conversion factors: catalog unit -> internal unit (mm based)
//==================================================================================================

constexpr double from_mm_  = 1.0;      // mm   -> mm
constexpr double from_cm_  = 10.0;     // cm   -> mm
constexpr double from_cm2_ = 1.0e2;    // cm^2 -> mm^2
constexpr double from_cm3_ = 1.0e3;    // cm^3 -> mm^3
constexpr double from_cm4_ = 1.0e4;    // cm^4 -> mm^4
constexpr double from_cm6_ = 1.0e6;    // cm^6 -> mm^6
constexpr double as_is_    = 1.0;      // kg/m, m^2/m and ratios are kept in their own units

//==================================================================================================
//  Column table (public, through steelcheck_section_columns)
//==================================================================================================

/*
 *  Designated initializers ([STEELCHECK_FIELD_IX] = ...) tie each entry to its field, whatever
 *  the order in which the entries are written. The array size is fixed to STEELCHECK_FIELD_COUNT,
 *  so a field without an entry would get a zero entry (name == nullptr); a test checks that every
 *  name is set.
 */

static const SteelCheckColumnSpec columns_[STEELCHECK_FIELD_COUNT] =
{
  [STEELCHECK_FIELD_DESIGNATION]          = { .name = "designation",          .required = true  },
  [STEELCHECK_FIELD_FAMILY]               = { .name = "family",               .required = true  },
  [STEELCHECK_FIELD_H_SHAPE]              = { .name = "h_shape",              .required = false },
  [STEELCHECK_FIELD_ON_REQUEST]           = { .name = "on_request",           .required = false },
  [STEELCHECK_FIELD_MASS]                 = { .name = "mass_kg_m",            .required = true  },
  [STEELCHECK_FIELD_D]                    = { .name = "d_mm",                 .required = true  },
  [STEELCHECK_FIELD_BF]                   = { .name = "bf_mm",                .required = true  },
  [STEELCHECK_FIELD_TW]                   = { .name = "tw_mm",                .required = true  },
  [STEELCHECK_FIELD_TF]                   = { .name = "tf_mm",                .required = true  },
  [STEELCHECK_FIELD_H]                    = { .name = "h_mm",                 .required = true  },
  [STEELCHECK_FIELD_DP]                   = { .name = "dp_mm",                .required = true  },
  [STEELCHECK_FIELD_R]                    = { .name = "r_mm",                 .required = false },
  [STEELCHECK_FIELD_A]                    = { .name = "A_cm2",                .required = true  },
  [STEELCHECK_FIELD_IX]                   = { .name = "Ix_cm4",               .required = true  },
  [STEELCHECK_FIELD_WX]                   = { .name = "Wx_cm3",               .required = true  },
  [STEELCHECK_FIELD_RX]                   = { .name = "rx_cm",                .required = true  },
  [STEELCHECK_FIELD_ZX]                   = { .name = "Zx_cm3",               .required = true  },
  [STEELCHECK_FIELD_IY]                   = { .name = "Iy_cm4",               .required = true  },
  [STEELCHECK_FIELD_WY]                   = { .name = "Wy_cm3",               .required = true  },
  [STEELCHECK_FIELD_RY]                   = { .name = "ry_cm",                .required = true  },
  [STEELCHECK_FIELD_ZY]                   = { .name = "Zy_cm3",               .required = true  },
  [STEELCHECK_FIELD_RT]                   = { .name = "rt_cm",                .required = false },
  [STEELCHECK_FIELD_J]                    = { .name = "J_cm4",                .required = true  },
  [STEELCHECK_FIELD_LAMBDA_F]             = { .name = "lambda_f",             .required = false },
  [STEELCHECK_FIELD_LAMBDA_W]             = { .name = "lambda_w",             .required = false },
  [STEELCHECK_FIELD_CW]                   = { .name = "Cw_cm6",               .required = true  },
  [STEELCHECK_FIELD_U]                    = { .name = "u_m2_m",               .required = false },
  [STEELCHECK_FIELD_DESIGNATION_IMPERIAL] = { .name = "designation_imperial", .required = false },
};

//==================================================================================================
//  How each field is stored (private)
//==================================================================================================

typedef enum FieldKind_ : int
{
  KIND_TEXT_,     // copied into a char array (designations) or interpreted (family)
  KIND_FLAG_,     // "0" or "1", stored in a bool
  KIND_NUMBER_,   // a double member of SteelCheckSection, converted to internal units
} FieldKind_;

typedef struct FieldInfo_
{
  FieldKind_ kind;
  size_t     offset;   // KIND_NUMBER_ only: offsetof the double member in SteelCheckSection
  double     scale;    // KIND_NUMBER_ only: catalog unit -> internal unit
} FieldInfo_;

/*
 *  offsetof(type, member) is the position of member inside type, in bytes. Together with the
 *  scale, it lets one table describe where every number goes, instead of a switch with a case per
 *  field. It works because every numeric member is a double: the address computed from the
 *  offset is the address of a double (see number_slot_).
 */

#define NUMBER_(member, factor) \
  { .kind = KIND_NUMBER_, .offset = offsetof ( SteelCheckSection, member ), .scale = (factor) }

static const FieldInfo_ info_[STEELCHECK_FIELD_COUNT] =
{
  [STEELCHECK_FIELD_DESIGNATION]          = { .kind = KIND_TEXT_ },
  [STEELCHECK_FIELD_FAMILY]               = { .kind = KIND_TEXT_ },
  [STEELCHECK_FIELD_H_SHAPE]              = { .kind = KIND_FLAG_ },
  [STEELCHECK_FIELD_ON_REQUEST]           = { .kind = KIND_FLAG_ },
  [STEELCHECK_FIELD_MASS]                 = NUMBER_ ( mass,     as_is_ ),
  [STEELCHECK_FIELD_D]                    = NUMBER_ ( d,        from_mm_ ),
  [STEELCHECK_FIELD_BF]                   = NUMBER_ ( bf,       from_mm_ ),
  [STEELCHECK_FIELD_TW]                   = NUMBER_ ( tw,       from_mm_ ),
  [STEELCHECK_FIELD_TF]                   = NUMBER_ ( tf,       from_mm_ ),
  [STEELCHECK_FIELD_H]                    = NUMBER_ ( h,        from_mm_ ),
  [STEELCHECK_FIELD_DP]                   = NUMBER_ ( dp,       from_mm_ ),
  [STEELCHECK_FIELD_R]                    = NUMBER_ ( r,        from_mm_ ),
  [STEELCHECK_FIELD_A]                    = NUMBER_ ( A,        from_cm2_ ),
  [STEELCHECK_FIELD_IX]                   = NUMBER_ ( Ix,       from_cm4_ ),
  [STEELCHECK_FIELD_WX]                   = NUMBER_ ( Wx,       from_cm3_ ),
  [STEELCHECK_FIELD_RX]                   = NUMBER_ ( rx,       from_cm_ ),
  [STEELCHECK_FIELD_ZX]                   = NUMBER_ ( Zx,       from_cm3_ ),
  [STEELCHECK_FIELD_IY]                   = NUMBER_ ( Iy,       from_cm4_ ),
  [STEELCHECK_FIELD_WY]                   = NUMBER_ ( Wy,       from_cm3_ ),
  [STEELCHECK_FIELD_RY]                   = NUMBER_ ( ry,       from_cm_ ),
  [STEELCHECK_FIELD_ZY]                   = NUMBER_ ( Zy,       from_cm3_ ),
  [STEELCHECK_FIELD_RT]                   = NUMBER_ ( rt,       from_cm_ ),
  [STEELCHECK_FIELD_J]                    = NUMBER_ ( J,        from_cm4_ ),
  [STEELCHECK_FIELD_LAMBDA_F]             = NUMBER_ ( lambda_f, as_is_ ),
  [STEELCHECK_FIELD_LAMBDA_W]             = NUMBER_ ( lambda_w, as_is_ ),
  [STEELCHECK_FIELD_CW]                   = NUMBER_ ( Cw,       from_cm6_ ),
  [STEELCHECK_FIELD_U]                    = NUMBER_ ( u,        as_is_ ),
  [STEELCHECK_FIELD_DESIGNATION_IMPERIAL] = { .kind = KIND_TEXT_ },
};

#undef NUMBER_

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  The double member of section at offset (from the info_ table). The section is reached as an
 *  array of bytes (unsigned char*, the one pointer type that may point into any object), moved by
 *  offset bytes, and turned back into a pointer to the double that lives there.
 */

static double* number_slot_ ( SteelCheckSection* section, size_t offset )
{
  return (double*) ( (unsigned char*) section + offset );
}

/*
 *  Copies text into dest (of size bytes) and terminates it. Returns false, copying nothing, if
 *  text and its '\0' do not fit.
 */

static bool copy_text_ ( char* dest, size_t size, SteelCheckView text )
{
  if ( text.length >= size )
  {
    return false;
  }

  if ( text.length > 0 )
  {
    memcpy ( dest, text.data, text.length );
  }

  dest[text.length] = '\0';

  return true;
}

/*
 *  The state shared by the checks of one row: where to report, and the first problem found.
 */

typedef struct RowContext_
{
  const SteelCheckReporter*  reporter;
  const SteelCheckCsvHeader* header;
  size_t                     line;
  SteelCheckStatus           first;    // STEELCHECK_OK until a problem is found
} RowContext_;

/*
 *  1-based file column of field, or 0 if the file does not have it.
 */

static size_t column_of_ ( const RowContext_* ctx, SteelCheckField field )
{
  size_t index = ctx->header->column_of_spec[field];

  return index == steelcheck_csv_none ? 0 : index + 1;
}

/*
 *  Records a problem: reports it, prefixed with the column name, and remembers the first status.
 */

STEELCHECK_PRINTF ( 4, 5 )
static void problem_ ( RowContext_* ctx, SteelCheckField field, SteelCheckStatus status,
                       const char* format, ... )
{
  if ( ctx->first == STEELCHECK_OK )
  {
    ctx->first = status;
  }

  // The detail is formatted first, then prefixed with the column name in a second report call.

  char detail[steelcheck_diagnostic_message_size];

  va_list args;
  va_start ( args, format );
  vsnprintf ( detail, sizeof detail, format, args );
  va_end ( args );

  steelcheck_report ( ctx->reporter, STEELCHECK_SEVERITY_ERROR, status, ctx->line,
                      column_of_( ctx, field ), "column '%s': %s", columns_[field].name, detail );
}

/*
 *  Interprets a text field. Only three fields are text, so a switch over them is clearer here
 *  than a table.
 */

static void read_text_ ( RowContext_* ctx, SteelCheckSection* section, SteelCheckField field,
                         SteelCheckView text )
{
  if ( field == STEELCHECK_FIELD_FAMILY )
  {
    if ( steelcheck_view_equals_cstr( text, "W" ) )
    {
      section->family = STEELCHECK_FAMILY_W;
    }
    else if ( steelcheck_view_equals_cstr( text, "HP" ) )
    {
      section->family = STEELCHECK_FAMILY_HP;
    }
    else
    {
      problem_ ( ctx, field, STEELCHECK_ERROR_VALUE, "expected W or HP, got '%.*s'",
                 steelcheck_view_print_length( text ), text.data );
    }

    return;
  }

  char* dest = field == STEELCHECK_FIELD_DESIGNATION ? section->designation
                                                     : section->designation_imperial;

  if ( !copy_text_( dest, steelcheck_designation_size, text ) )
  {
    problem_ ( ctx, field, STEELCHECK_ERROR_VALUE, "'%.*s' is longer than %zu characters",
               steelcheck_view_print_length( text ), text.data, steelcheck_designation_size - 1 );
  }
}

//==================================================================================================
//  steelcheck_section_columns
//==================================================================================================

const SteelCheckColumnSpec* steelcheck_section_columns ()
{
  return columns_;
}

//==================================================================================================
//  steelcheck_section_has
//==================================================================================================

bool steelcheck_section_has ( const SteelCheckSection* section, SteelCheckField field )
{
  if ( section == nullptr || field < 0 || field >= STEELCHECK_FIELD_COUNT )
  {
    return false;
  }

  return ( section->present >> field ) & 1u;
}

//==================================================================================================
//  steelcheck_section_was_computed
//==================================================================================================

bool steelcheck_section_was_computed ( const SteelCheckSection* section, SteelCheckField field )
{
  if ( section == nullptr || field < 0 || field >= STEELCHECK_FIELD_COUNT )
  {
    return false;
  }

  return ( section->computed >> field ) & 1u;
}

//==================================================================================================
//  steelcheck_family_name
//==================================================================================================

const char* steelcheck_family_name ( SteelCheckFamily family )
{
  switch ( family )
  {
    case STEELCHECK_FAMILY_W:
      return "W";

    case STEELCHECK_FAMILY_HP:
      return "HP";
  }

  return "unknown family";
}

//==================================================================================================
//  steelcheck_section_from_row
//==================================================================================================

SteelCheckStatus steelcheck_section_from_row ( SteelCheckSection* section,
                                               const SteelCheckCsvHeader* header,
                                               const SteelCheckView* fields,
                                               size_t line_number,
                                               const SteelCheckReporter* reporter )
{
  // The header must have been mapped with this module's table: then fields[f] is field f.

  if ( section == nullptr || header == nullptr || fields == nullptr
       || header->specs != columns_ || header->spec_count != STEELCHECK_FIELD_COUNT )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  *section = (SteelCheckSection) { .source_line = line_number };

  RowContext_ ctx = { .reporter = reporter, .header = header, .line = line_number };

  for ( int f = 0; f < STEELCHECK_FIELD_COUNT; f++ )
  {
    SteelCheckField  field = (SteelCheckField) f;
    SteelCheckView   text  = fields[f];

    // An empty field: an error if the column is required, otherwise the field is simply absent
    // (its value stays 0 and its present bit stays clear).

    if ( text.length == 0 )
    {
      if ( columns_[f].required )
      {
        problem_ ( &ctx, field, STEELCHECK_ERROR_MISSING_VALUE, "missing value" );
      }

      continue;
    }

    SteelCheckStatus status = STEELCHECK_OK;

    switch ( info_[f].kind )
    {
      case KIND_TEXT_:
        read_text_ ( &ctx, section, field, text );
        break;

      case KIND_FLAG_:
      {
        bool* flag = field == STEELCHECK_FIELD_H_SHAPE ? &section->h_shape : &section->on_request;

        status = steelcheck_parse_flag ( text, flag );

        if ( status != STEELCHECK_OK )
        {
          problem_ ( &ctx, field, status, "expected 0 or 1, got '%.*s'",
                     steelcheck_view_print_length( text ), text.data );
        }
        break;
      }

      case KIND_NUMBER_:
      {
        double value = 0.0;

        status = steelcheck_parse_double ( text, &value );

        if ( status == STEELCHECK_OK )
        {
          *number_slot_ ( section, info_[f].offset ) = value * info_[f].scale;
        }
        else
        {
          problem_ ( &ctx, field, status, "%s '%.*s'", steelcheck_status_message( status ),
                     steelcheck_view_print_length( text ), text.data );
        }
        break;
      }
    }

    section->present |= (uint32_t) 1 << f;
  }

  if ( ctx.first != STEELCHECK_OK )
  {
    *section = (SteelCheckSection) {};
  }

  return ctx.first;
}

//==================================================================================================
//  Validation: private helpers
//==================================================================================================

constexpr double steel_density_ = 7850.0;                   // kg/m^3, for the mass check
constexpr double pi_            = 3.14159265358979323846;   // M_PI is not standard C

static constexpr char area_formula_[] = "2 bf tf + (d - 2 tf) tw + (4 - pi) r^2";

/*
 *  The state shared by the checks of one section.
 */

typedef struct CheckContext_
{
  const SteelCheckReporter* reporter;
  const SteelCheckSection*  section;
  bool                      failed;    // an error was found
} CheckContext_;

/*
 *  Reports a finding about the section, prefixed with its designation. Errors carry
 *  STEELCHECK_ERROR_VALUE and make the validation fail; warnings carry
 *  STEELCHECK_ERROR_INCONSISTENT.
 */

STEELCHECK_PRINTF ( 3, 4 )
static void finding_ ( CheckContext_* ctx, SteelCheckSeverity severity, const char* format, ... )
{
  char detail[steelcheck_diagnostic_message_size];

  va_list args;
  va_start ( args, format );
  vsnprintf ( detail, sizeof detail, format, args );
  va_end ( args );

  bool             error  = severity == STEELCHECK_SEVERITY_ERROR;
  SteelCheckStatus status = error ? STEELCHECK_ERROR_VALUE : STEELCHECK_ERROR_INCONSISTENT;

  if ( error )
  {
    ctx->failed = true;
  }

  steelcheck_report ( ctx->reporter, severity, status, ctx->section->source_line, 0, "%s: %s",
                      ctx->section->designation, detail );
}

/*
 *  The value of a numeric field, read through the info_ table (the const twin of number_slot_).
 */

static double value_ ( const SteelCheckSection* section, SteelCheckField field )
{
  return *(const double*) ( (const unsigned char*) section + info_[field].offset );
}

/*
 *  The value of a numeric field in the catalog's unit (e.g. cm^4 for Ix), for messages.
 */

static double catalog_ ( const SteelCheckSection* section, SteelCheckField field )
{
  return value_( section, field ) / info_[field].scale;
}

/*
 *  Every numeric field that was given must be positive. !( value > 0.0 ) is also true for NaN.
 */

static void check_positive_ ( CheckContext_* ctx )
{
  for ( int f = 0; f < STEELCHECK_FIELD_COUNT; f++ )
  {
    SteelCheckField field = (SteelCheckField) f;

    if ( info_[f].kind != KIND_NUMBER_ || !steelcheck_section_has( ctx->section, field ) )
    {
      continue;
    }

    if ( !( value_( ctx->section, field ) > 0.0 ) )
    {
      finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "%s must be positive, got %g", columns_[f].name,
                 catalog_( ctx->section, field ) );
    }
  }
}

/*
 *  Relations every rolled I-section satisfies. Breaking one means a wrong value, or two columns
 *  swapped in the file.
 */

static void check_geometry_ ( CheckContext_* ctx )
{
  const SteelCheckSection* s = ctx->section;

  if ( 2.0 * s->tf >= s->d )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "tf_mm %g is not less than d / 2 = %g",
               s->tf, s->d / 2.0 );
  }

  if ( s->tw >= s->bf )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "tw_mm %g is not less than bf_mm %g", s->tw, s->bf );
  }

  if ( s->h >= s->d )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "h_mm %g is not less than d_mm %g", s->h, s->d );
  }

  if ( s->dp > s->h )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "dp_mm %g is greater than h_mm %g", s->dp, s->h );
  }

  if ( s->Zx < s->Wx )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "Zx_cm3 %g is less than Wx_cm3 %g",
               catalog_( s, STEELCHECK_FIELD_ZX ), catalog_( s, STEELCHECK_FIELD_WX ) );
  }

  if ( s->Zy < s->Wy )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR, "Zy_cm3 %g is less than Wy_cm3 %g",
               catalog_( s, STEELCHECK_FIELD_ZY ), catalog_( s, STEELCHECK_FIELD_WY ) );
  }

  if ( s->Iy > s->Ix )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR,
               "Iy_cm4 %g is greater than Ix_cm4 %g (axes swapped?)",
               catalog_( s, STEELCHECK_FIELD_IY ), catalog_( s, STEELCHECK_FIELD_IX ) );
  }
}

/*
 *  The designation must start with the family name followed by a digit: "W150x13.0" for W,
 *  "HP200x53.0" for HP. (A W designation never starts with "HP", and vice versa.)
 */

static void check_designation_ ( CheckContext_* ctx )
{
  const SteelCheckSection* s      = ctx->section;
  const char*              family = steelcheck_family_name ( s->family );
  SteelCheckView           name   = steelcheck_view_from_cstr ( s->designation );
  size_t                   length = strlen ( family );

  bool ok = steelcheck_view_starts_with( name, family ) && name.length > length
         && name.data[length] >= '0' && name.data[length] <= '9';

  if ( !ok )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_ERROR,
               "designation does not match family %s (expected %s followed by a digit)",
               family, family );
  }
}

/*
 *  Warns if a catalog value differs from the value the other values imply by more than tolerance
 *  (relative). Fields not given in the file are skipped. expected is positive: the positivity
 *  check has passed.
 */

static void compare_ ( CheckContext_* ctx, SteelCheckField field, double expected,
                       double tolerance, const char* formula )
{
  if ( !steelcheck_section_has( ctx->section, field ) )
  {
    return;
  }

  double given     = value_( ctx->section, field );
  double deviation = ( given - expected ) / expected;

  if ( fabs( deviation ) > tolerance )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_WARNING, "%s %.4g differs from %s = %.4g (%+.1f %%)",
               columns_[field].name, given / info_[field].scale, formula,
               expected / info_[field].scale, 100.0 * deviation );
  }
}

/*
 *  The nominal mass in the designation (the number after the last 'x', e.g. 153.0 in W610x153.0)
 *  against mass_kg_m. Designations without such a number are not checked.
 */

static void compare_designation_mass_ ( CheckContext_* ctx )
{
  const SteelCheckSection* s       = ctx->section;
  const char*              x       = strrchr ( s->designation, 'x' );
  double                   nominal = 0.0;

  if ( x == nullptr
       || steelcheck_parse_double( steelcheck_view_from_cstr( x + 1 ), &nominal ) != STEELCHECK_OK
       || !( nominal > 0.0 ) )
  {
    return;
  }

  double deviation = ( s->mass - nominal ) / nominal;

  if ( fabs( deviation ) > steelcheck_tolerance_name )
  {
    finding_ ( ctx, STEELCHECK_SEVERITY_WARNING,
               "mass_kg_m %.4g differs from the designation's %.4g (%+.1f %%)",
               s->mass, nominal, 100.0 * deviation );
  }
}

//==================================================================================================
//  steelcheck_section_validate
//==================================================================================================

SteelCheckStatus steelcheck_section_validate ( SteelCheckSection* section,
                                               const SteelCheckReporter* reporter )
{
  if ( section == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  CheckContext_ ctx = { .reporter = reporter, .section = section };

  // 1. Errors. All checks run, so every error is reported; then stop, before any value is derived
  //    (a zero thickness would mean a division by zero below).

  check_positive_ ( &ctx );
  check_geometry_ ( &ctx );
  check_designation_ ( &ctx );

  if ( ctx.failed )
  {
    return STEELCHECK_ERROR_VALUE;
  }

  // 2. Derived values. r only if the file did not give it.

  if ( !steelcheck_section_has( section, STEELCHECK_FIELD_R ) )
  {
    section->r         = ( section->h - section->dp ) / 2.0;
    section->computed |= (uint32_t) 1 << STEELCHECK_FIELD_R;
  }

  section->h0 = section->d - section->tf;
  section->r0 = sqrt ( section->rx * section->rx + section->ry * section->ry );

  // 3. Consistency warnings. The area includes the four fillets: each adds a square r x r minus
  //    a quarter circle, so 4 (r^2 - pi r^2 / 4) = (4 - pi) r^2 in total.

  const SteelCheckSection* s    = section;
  double                   area = 2.0 * s->bf * s->tf + ( s->d - 2.0 * s->tf ) * s->tw
                                + ( 4.0 - pi_ ) * s->r * s->r;

  constexpr double exact  = steelcheck_tolerance_exact;
  constexpr double approx = steelcheck_tolerance_approx;
  constexpr double mass   = steelcheck_tolerance_mass;

  compare_ ( &ctx, STEELCHECK_FIELD_H,    s->d - 2.0 * s->tf,           exact,  "d - 2 tf" );
  compare_ ( &ctx, STEELCHECK_FIELD_A,    area,                         exact,  area_formula_ );
  compare_ ( &ctx, STEELCHECK_FIELD_MASS, s->A * 1e-6 * steel_density_, mass,   "A x 7850 kg/m^3" );
  compare_ ( &ctx, STEELCHECK_FIELD_WX,   2.0 * s->Ix / s->d,           exact,  "2 Ix / d" );
  compare_ ( &ctx, STEELCHECK_FIELD_WY,   2.0 * s->Iy / s->bf,          exact,  "2 Iy / bf" );
  compare_ ( &ctx, STEELCHECK_FIELD_RX,   sqrt( s->Ix / s->A ),         exact,  "sqrt(Ix / A)" );
  compare_ ( &ctx, STEELCHECK_FIELD_RY,   sqrt( s->Iy / s->A ),         exact,  "sqrt(Iy / A)" );
  compare_ ( &ctx, STEELCHECK_FIELD_CW,   s->Iy * s->h0 * s->h0 / 4.0,  approx, "Iy h0^2 / 4" );

  compare_ ( &ctx, STEELCHECK_FIELD_LAMBDA_F, s->bf / ( 2.0 * s->tf ), exact, "bf / (2 tf)" );
  compare_ ( &ctx, STEELCHECK_FIELD_LAMBDA_W, s->dp / s->tw,           exact, "d' / tw" );

  compare_designation_mass_ ( &ctx );

  // 4. The slenderness ratios used from now on are the exact ones, not the rounded catalog values.

  section->lambda_f  = section->bf / ( 2.0 * section->tf );
  section->lambda_w  = section->dp / section->tw;
  section->computed |= (uint32_t) 1 << STEELCHECK_FIELD_LAMBDA_F;
  section->computed |= (uint32_t) 1 << STEELCHECK_FIELD_LAMBDA_W;

  return STEELCHECK_OK;
}
