#include "steelcheck/csv.h"

#include <string.h>

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  Letters, digits and '_' (ASCII only), the characters allowed in a metadata key. Written out
 *  instead of using isalnum() for the same reasons as in view.c: no undefined behavior for bytes
 *  >= 0x80, and no dependence on the locale.
 */

static bool is_key_char_ ( char c )
{
  return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' )
      || c == '_';
}

static bool is_key_ ( SteelCheckView key )
{
  if ( key.length == 0 )
  {
    return false;
  }

  for ( size_t i = 0; i < key.length; i++ )
  {
    if ( !is_key_char_( key.data[i] ) )
    {
      return false;
    }
  }

  return true;
}

/*
 *  Checks a field for bytes this restricted CSV does not allow. Returns nullptr if the field is
 *  fine, otherwise a message naming the problem.
 */

static const char* field_problem_ ( SteelCheckView field )
{
  if ( field.length == 0 )
  {
    return nullptr;
  }

  if ( memchr( field.data, '"', field.length ) != nullptr )
  {
    return "quotes are not supported in fields";
  }

  if ( memchr( field.data, '\0', field.length ) != nullptr )
  {
    return "field contains a '\\0' byte";
  }

  return nullptr;
}

//==================================================================================================
//  steelcheck_csv_classify
//==================================================================================================

SteelCheckLineKind steelcheck_csv_classify ( SteelCheckView line, SteelCheckMetadata* metadata )
{
  if ( metadata != nullptr )
  {
    *metadata = (SteelCheckMetadata) {};
  }

  SteelCheckView trimmed = steelcheck_view_trim ( line );

  if ( trimmed.length == 0 )
  {
    return STEELCHECK_LINE_BLANK;
  }

  if ( trimmed.data[0] != '#' )
  {
    return STEELCHECK_LINE_DATA;
  }

  // A comment. It is metadata if the text after '#' is "key: value" with a valid key: the key is
  // what comes before the first ':', trimmed.

  SteelCheckView text  = { .length = trimmed.length - 1, .data = trimmed.data + 1 };
  const char*    colon = text.length > 0 ? memchr( text.data, ':', text.length ) : nullptr;

  if ( colon == nullptr )
  {
    return STEELCHECK_LINE_COMMENT;
  }

  SteelCheckView key   = { .length = (size_t) ( colon - text.data ), .data = text.data };
  SteelCheckView value = { .length = text.length - key.length - 1, .data = colon + 1 };

  key = steelcheck_view_trim ( key );

  if ( !is_key_( key ) )
  {
    return STEELCHECK_LINE_COMMENT;
  }

  if ( metadata != nullptr )
  {
    metadata->key   = key;
    metadata->value = steelcheck_view_trim ( value );
  }

  return STEELCHECK_LINE_METADATA;
}

//==================================================================================================
//  steelcheck_fields_init
//==================================================================================================

void steelcheck_fields_init ( SteelCheckFields* iter, SteelCheckView line )
{
  if ( iter == nullptr )
  {
    return;
  }

  // An empty line still has one (empty) field, so done starts false even with no data.

  *iter = (SteelCheckFields)
  {
    .curr   = line.data,
    .end    = line.data == nullptr ? nullptr : line.data + line.length,
    .column = 0,
    .done   = false,
  };
}

//==================================================================================================
//  steelcheck_fields_next
//==================================================================================================

bool steelcheck_fields_next ( SteelCheckFields* iter, SteelCheckView* field, size_t* column )
{
  if ( iter == nullptr || field == nullptr || column == nullptr )
  {
    return false;
  }

  *field  = (SteelCheckView) {};
  *column = 0;

  if ( iter->done )
  {
    return false;
  }

  // The field ends at the next ',' or at the end of the line. Unlike lines, a separator at the
  // very end ("a,") is followed by one more, empty, field: that is why the iterator needs the
  // done flag instead of stopping when curr reaches end.

  const char* start     = iter->curr;
  size_t      remaining = (size_t) ( iter->end - start );
  const char* comma     = remaining > 0 ? memchr( start, ',', remaining ) : nullptr;
  const char* stop      = nullptr;

  if ( comma != nullptr )
  {
    stop       = comma;
    iter->curr = comma + 1;
  }
  else
  {
    stop       = iter->end;
    iter->curr = iter->end;
    iter->done = true;
  }

  *field = steelcheck_view_trim ( (SteelCheckView) { .length = (size_t) ( stop - start ),
                                                     .data   = start } );

  ++iter->column;
  *column = iter->column;

  return true;
}

//==================================================================================================
//  steelcheck_csv_map_header
//==================================================================================================

SteelCheckStatus steelcheck_csv_map_header ( SteelCheckCsvHeader* header,
                                             const SteelCheckColumnSpec* specs,
                                             size_t spec_count,
                                             SteelCheckView line,
                                             size_t line_number,
                                             const SteelCheckReporter* reporter )
{
  if ( header == nullptr || specs == nullptr || spec_count == 0
       || spec_count > steelcheck_csv_max_columns )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  *header = (SteelCheckCsvHeader)
  {
    .specs      = specs,
    .spec_count = spec_count,
    .line       = line_number,
  };

  for ( size_t i = 0; i < steelcheck_csv_max_columns; i++ )
  {
    header->spec_of_column[i] = steelcheck_csv_none;
    header->column_of_spec[i] = steelcheck_csv_none;
  }

  // The names seen so far, to detect duplicates. They point into line, so no copies are made.

  SteelCheckView names[steelcheck_csv_max_columns] = {};

  bool             failed = false;
  SteelCheckFields iter   = {};
  SteelCheckView   name   = {};
  size_t           column = 0;

  steelcheck_fields_init ( &iter, line );

  while ( steelcheck_fields_next( &iter, &name, &column ) )
  {
    if ( column > steelcheck_csv_max_columns )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER,
                          line_number, column, "too many columns (at most %zu)",
                          steelcheck_csv_max_columns );
      return STEELCHECK_ERROR_HEADER;
    }

    size_t index = column - 1;

    names[index]         = name;
    header->column_count = column;

    const char* problem = field_problem_ ( name );

    if ( problem != nullptr )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_SYNTAX,
                          line_number, column, "%s", problem );
      failed = true;
      continue;
    }

    if ( name.length == 0 )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER,
                          line_number, column, "empty column name" );
      failed = true;
      continue;
    }

    // Duplicate of an earlier column? (At most 64 columns, so comparing with every earlier one
    // is cheap.) The first occurrence keeps its mapping; the duplicate is not mapped.

    bool duplicate = false;

    for ( size_t j = 0; j < index && !duplicate; j++ )
    {
      if ( steelcheck_view_equals( names[j], name ) )
      {
        steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER,
                            line_number, column, "duplicate column '%.*s' (also column %zu)",
                            steelcheck_view_print_length( name ), name.data, j + 1 );
        duplicate = true;
      }
    }

    if ( duplicate )
    {
      failed = true;
      continue;
    }

    // Find the spec with this name.

    size_t spec = steelcheck_csv_none;

    for ( size_t s = 0; s < spec_count && spec == steelcheck_csv_none; s++ )
    {
      if ( steelcheck_view_equals_cstr( name, specs[s].name ) )
      {
        spec = s;
      }
    }

    if ( spec == steelcheck_csv_none )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_HEADER,
                          line_number, column, "unknown column '%.*s' is ignored",
                          steelcheck_view_print_length( name ), name.data );
      continue;
    }

    header->spec_of_column[index] = spec;
    header->column_of_spec[spec]  = index;
  }

  // Every required spec must have been found. All missing ones are reported, not just the first.

  for ( size_t s = 0; s < spec_count; s++ )
  {
    if ( specs[s].required && header->column_of_spec[s] == steelcheck_csv_none )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER,
                          line_number, 0, "missing required column '%s'", specs[s].name );
      failed = true;
    }
  }

  return failed ? STEELCHECK_ERROR_HEADER : STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_csv_split_row
//==================================================================================================

SteelCheckStatus steelcheck_csv_split_row ( const SteelCheckCsvHeader* header,
                                            SteelCheckView line,
                                            size_t line_number,
                                            SteelCheckView* fields,
                                            size_t field_count,
                                            const SteelCheckReporter* reporter )
{
  if ( header == nullptr || fields == nullptr || field_count < header->spec_count )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  // Every spec starts empty: specs the file does not have, and fields of a rejected row, stay so.

  for ( size_t s = 0; s < header->spec_count; s++ )
  {
    fields[s] = (SteelCheckView) {};
  }

  bool             failed = false;
  size_t           count  = 0;
  SteelCheckFields iter   = {};
  SteelCheckView   field  = {};
  size_t           column = 0;

  steelcheck_fields_init ( &iter, line );

  while ( steelcheck_fields_next( &iter, &field, &column ) )
  {
    count = column;

    // Extra fields beyond the header are only counted; the count is reported below.

    if ( column > header->column_count )
    {
      continue;
    }

    const char* problem = field_problem_ ( field );

    if ( problem != nullptr )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_SYNTAX,
                          line_number, column, "%s", problem );
      failed = true;
      continue;
    }

    size_t spec = header->spec_of_column[column - 1];

    if ( spec != steelcheck_csv_none )
    {
      fields[spec] = field;
    }
  }

  if ( count != header->column_count )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_SYNTAX,
                        line_number, 0, "%zu fields, but the header (line %zu) has %zu columns",
                        count, header->line, header->column_count );
    failed = true;
  }

  // A rejected row must not leave half-filled fields behind.

  if ( failed )
  {
    for ( size_t s = 0; s < header->spec_count; s++ )
    {
      fields[s] = (SteelCheckView) {};
    }

    return STEELCHECK_ERROR_SYNTAX;
  }

  return STEELCHECK_OK;
}
