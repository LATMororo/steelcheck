#include "cli/commands.h"

#include "steelcheck/builtin.h"

#include <math.h>
#include <string.h>

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  Size of the text of one formatted number.
 */

constexpr size_t number_size_ = 32;

/*
 *  Formats a value the way catalogs print it: up to 4 significant digits below 1000 (6.18, 85.8,
 *  0.67), whole numbers above (8581, 14676643), with one decimal if it has one (1597.4).
 */

static void format_number_ ( double value, char* text, size_t size )
{
  if ( fabs( value ) < 1000.0 )
  {
    snprintf ( text, size, "%.4g", value );
  }
  else if ( fabs( value - round( value ) ) < 1e-6 * fabs( value ) )
  {
    snprintf ( text, size, "%.0f", value );
  }
  else
  {
    snprintf ( text, size, "%.1f", value );
  }
}

/*
 *  The designation filter as a prefix to compare with designation keys: without spaces and tabs,
 *  in upper case, ',' as '.'. Unlike steelcheck_designation_key, no ".0" is added: "W310" must
 *  match "W310X38.7". Returns false if it does not fit.
 */

static bool filter_prefix_ ( const char* filter, char* prefix, size_t size )
{
  size_t length = 0;

  for ( const char* p = filter; *p != '\0'; p++ )
  {
    char c = *p;

    if ( c == ' ' || c == '\t' )
    {
      continue;
    }

    if ( length + 1 >= size )
    {
      return false;
    }

    c                = c == ',' ? '.' : c;
    prefix[length++] = c >= 'a' && c <= 'z' ? (char) ( c - 'a' + 'A' ) : c;
  }

  prefix[length] = '\0';

  return true;
}

/*
 *  Prints one line of the show command: symbol, value, unit, description, and a note.
 */

static void show_field_ ( FILE* out, const SteelCheckSection* section, SteelCheckField field )
{
  const SteelCheckFieldInfo* info = steelcheck_field_info ( field );
  char                       number[number_size_] = "-";
  const char*                note = "";

  if ( steelcheck_section_was_computed( section, field ) )
  {
    format_number_ ( steelcheck_section_value( section, field ), number, sizeof number );
    note = " (computed)";
  }
  else if ( steelcheck_section_has( section, field ) )
  {
    format_number_ ( steelcheck_section_value( section, field ), number, sizeof number );
  }

  fprintf ( out, "    %-7s %10s  %-6s %s%s\n", info->symbol, number, info->unit,
            info->description, note );
}

//==================================================================================================
//  cli_choose_source
//==================================================================================================

CliSource cli_choose_source ( const char* option, const char* environment, bool builtin_available,
                              const char** name )
{
  if ( option != nullptr && option[0] != '\0' )
  {
    *name = option;
    return CLI_SOURCE_FILE;
  }

  if ( environment != nullptr && environment[0] != '\0' )
  {
    *name = environment;
    return CLI_SOURCE_FILE;
  }

  if ( builtin_available )
  {
    *name = steelcheck_builtin_source;
    return CLI_SOURCE_BUILTIN;
  }

  *name = nullptr;
  return CLI_SOURCE_NONE;
}

//==================================================================================================
//  cli_print_diagnostic
//==================================================================================================

void cli_print_diagnostic ( void* context, const SteelCheckDiagnostic* diagnostic )
{
  const CliPrinter* printer = context;

  if ( printer->quiet && diagnostic->severity == STEELCHECK_SEVERITY_WARNING )
  {
    return;
  }

  // The location: "source:line:column", without the parts that are 0.

  fprintf ( printer->stream, "%s", printer->source );

  if ( diagnostic->line > 0 )
  {
    fprintf ( printer->stream, ":%zu", diagnostic->line );

    if ( diagnostic->column > 0 )
    {
      fprintf ( printer->stream, ":%zu", diagnostic->column );
    }
  }

  fprintf ( printer->stream, ": %s: %s\n", steelcheck_severity_name( diagnostic->severity ),
            diagnostic->message );
}

//==================================================================================================
//  cli_list
//==================================================================================================

size_t cli_list ( FILE* out, const SteelCheckDatabase* db, const char* filter )
{
  // The columns of the table, with their symbols and units from the section module.

  static const SteelCheckField columns[] =
  {
    STEELCHECK_FIELD_MASS, STEELCHECK_FIELD_D,  STEELCHECK_FIELD_BF, STEELCHECK_FIELD_TW,
    STEELCHECK_FIELD_TF,   STEELCHECK_FIELD_A,  STEELCHECK_FIELD_IX, STEELCHECK_FIELD_IY,
  };

  constexpr size_t column_count = sizeof columns / sizeof columns[0];

  char prefix[steelcheck_designation_key_size] = "";

  if ( filter != nullptr && !filter_prefix_( filter, prefix, sizeof prefix ) )
  {
    return 0;
  }

  // Header: symbols, then units.

  fprintf ( out, "%-12s %-6s", "designation", "family" );

  for ( size_t c = 0; c < column_count; c++ )
  {
    fprintf ( out, " %9s", steelcheck_field_info( columns[c] )->symbol );
  }

  fprintf ( out, "\n%-12s %-6s", "", "" );

  for ( size_t c = 0; c < column_count; c++ )
  {
    fprintf ( out, " %9s", steelcheck_field_info( columns[c] )->unit );
  }

  fprintf ( out, "\n" );

  // One line per matching section.

  size_t printed = 0;

  for ( size_t i = 0; i < db->count; i++ )
  {
    const SteelCheckSection* s = &db->sections[i];
    char                     key[steelcheck_designation_key_size];

    if ( !steelcheck_designation_key( s->designation, key, sizeof key )
         || strncmp( key, prefix, strlen( prefix ) ) != 0 )
    {
      continue;
    }

    fprintf ( out, "%-12s %-6s", s->designation, steelcheck_family_name( s->family ) );

    for ( size_t c = 0; c < column_count; c++ )
    {
      char number[number_size_];

      format_number_ ( steelcheck_section_value( s, columns[c] ), number, sizeof number );
      fprintf ( out, " %9s", number );
    }

    fprintf ( out, "\n" );
    printed++;
  }

  // Summary, with the catalog's name and edition when the file gives them.

  const char* catalog = steelcheck_database_metadata ( db, "catalog" );
  const char* edition = steelcheck_database_metadata ( db, "edition" );

  fprintf ( out, "\n%zu of %zu sections", printed, db->count );

  if ( catalog != nullptr )
  {
    fprintf ( out, " - %s", catalog );
  }

  if ( edition != nullptr )
  {
    fprintf ( out, ", edition %s", edition );
  }

  fprintf ( out, "\n" );

  return printed;
}

//==================================================================================================
//  cli_show
//==================================================================================================

bool cli_show ( FILE* out, const SteelCheckDatabase* db, const char* designation,
                const char* source )
{
  const SteelCheckSection* s = steelcheck_database_find ( db, designation );

  if ( s == nullptr )
  {
    return false;
  }

  // Groups of fields, in the order they are printed.

  static const SteelCheckField dimensions[] =
  {
    STEELCHECK_FIELD_MASS, STEELCHECK_FIELD_D, STEELCHECK_FIELD_BF, STEELCHECK_FIELD_TW,
    STEELCHECK_FIELD_TF,   STEELCHECK_FIELD_H, STEELCHECK_FIELD_DP, STEELCHECK_FIELD_R,
  };

  static const SteelCheckField properties[] =
  {
    STEELCHECK_FIELD_A,  STEELCHECK_FIELD_IX, STEELCHECK_FIELD_WX, STEELCHECK_FIELD_RX,
    STEELCHECK_FIELD_ZX, STEELCHECK_FIELD_IY, STEELCHECK_FIELD_WY, STEELCHECK_FIELD_RY,
    STEELCHECK_FIELD_ZY, STEELCHECK_FIELD_RT, STEELCHECK_FIELD_J,  STEELCHECK_FIELD_CW,
    STEELCHECK_FIELD_U,
  };

  static const SteelCheckField slenderness[] =
  {
    STEELCHECK_FIELD_LAMBDA_F, STEELCHECK_FIELD_LAMBDA_W,
  };

  // Identity and origin.

  fprintf ( out, "%s\n", s->designation );
  fprintf ( out, "  family %s", steelcheck_family_name( s->family ) );

  if ( s->designation_imperial[0] != '\0' )
  {
    fprintf ( out, ", imperial designation %s", s->designation_imperial );
  }

  fprintf ( out, "\n  from %s, line %zu\n", source, s->source_line );

  // The fields, group by group.

  fprintf ( out, "\n  Dimensions\n" );

  for ( size_t i = 0; i < sizeof dimensions / sizeof dimensions[0]; i++ )
  {
    show_field_ ( out, s, dimensions[i] );
  }

  fprintf ( out, "\n  Section properties\n" );

  for ( size_t i = 0; i < sizeof properties / sizeof properties[0]; i++ )
  {
    show_field_ ( out, s, properties[i] );
  }

  fprintf ( out, "\n  Slenderness\n" );

  for ( size_t i = 0; i < sizeof slenderness / sizeof slenderness[0]; i++ )
  {
    show_field_ ( out, s, slenderness[i] );
  }

  // Derived values, which have no CSV column.

  char h0[number_size_];
  char r0[number_size_];

  format_number_ ( s->h0, h0, sizeof h0 );
  format_number_ ( s->r0 / 10.0, r0, sizeof r0 );   // mm -> cm, like rx and ry

  fprintf ( out, "\n  Derived\n" );
  fprintf ( out, "    %-7s %10s  %-6s %s\n", "h0", h0, "mm", "distance between flange centroids" );
  fprintf ( out, "    %-7s %10s  %-6s %s\n", "r0", r0, "cm", "polar radius of gyration" );

  // Catalog marks.

  fprintf ( out, "\n  Catalog marks: " );

  if ( !s->h_shape && !s->on_request )
  {
    fprintf ( out, "none" );
  }

  if ( s->h_shape )
  {
    fprintf ( out, "wide-flange H shape (H)%s", s->on_request ? ", " : "" );
  }

  if ( s->on_request )
  {
    fprintf ( out, "produced on request (*)" );
  }

  fprintf ( out, "\n" );

  return true;
}
