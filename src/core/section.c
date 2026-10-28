#include "steelcheck/section.h"

#include "steelcheck/number.h"

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
