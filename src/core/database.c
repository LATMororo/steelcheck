#include "steelcheck/database.h"

#include "steelcheck/buffer.h"
#include "steelcheck/builtin.h"
#include "steelcheck/csv.h"
#include "steelcheck/lines.h"

#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

/*
 *  Number of sections allocated by the first growth; every further growth doubles it.
 */

constexpr size_t initial_capacity_ = 16;

/*
 *  Size of a key buffer for designations typed by a user, which may be longer than a stored one
 *  (steelcheck_designation_key_size); a longer one cannot match any section anyway.
 */

constexpr size_t key_size_ = 64;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  The section whose designation key equals key, or nullptr. A linear search: a catalog has about
 *  a hundred sections, and each comparison is short.
 */

static SteelCheckSection* find_key_ ( const SteelCheckDatabase* db, const char* key )
{
  char other[key_size_];

  for ( size_t i = 0; i < db->count; i++ )
  {
    if ( steelcheck_designation_key( db->sections[i].designation, other, sizeof other )
         && strcmp( other, key ) == 0 )
    {
      return &db->sections[i];
    }
  }

  return nullptr;
}

/*
 *  Makes room for at least one more section: 16 slots at first, then twice as many each time.
 *  Doubling keeps the total copying work proportional to the number of sections (each one is
 *  moved only a few times on average), instead of growing by one slot per section.
 */

static SteelCheckStatus grow_ ( SteelCheckDatabase* db )
{
  size_t capacity = initial_capacity_;

  if ( db->capacity > 0 && ckd_mul( &capacity, db->capacity, 2 ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  // The size in bytes is a multiplication passed to realloc: an unchecked overflow here would
  // allocate too little memory, and the next write would go past its end.

  size_t bytes = 0;

  if ( ckd_mul( &bytes, capacity, sizeof *db->sections ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  // realloc ( nullptr, bytes ) behaves like malloc ( bytes ), so the first growth needs no special
  // case. On failure, the old array is still valid and still owned by db.

  SteelCheckSection* sections = realloc ( db->sections, bytes );

  if ( sections == nullptr )
  {
    return STEELCHECK_ERROR_NO_MEMORY;
  }

  db->sections = sections;
  db->capacity = capacity;

  return STEELCHECK_OK;
}

/*
 *  Appends a copy of section, unless a section with the same designation key is already there
 *  (reported as an error naming both lines).
 */

static SteelCheckStatus add_ ( SteelCheckDatabase* db, const SteelCheckSection* section,
                               const SteelCheckReporter* reporter )
{
  char key[key_size_];

  if ( !steelcheck_designation_key( section->designation, key, sizeof key ) )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE,
                        section->source_line, 0, "invalid designation '%s'",
                        section->designation );
    return STEELCHECK_ERROR_VALUE;
  }

  const SteelCheckSection* other = find_key_( db, key );

  if ( other != nullptr )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_DUPLICATE,
                        section->source_line, 0, "duplicate designation '%s' (also line %zu: '%s')",
                        section->designation, other->source_line, other->designation );
    return STEELCHECK_ERROR_DUPLICATE;
  }

  if ( db->count == db->capacity )
  {
    SteelCheckStatus status = grow_ ( db );

    if ( status != STEELCHECK_OK )
    {
      return status;
    }
  }

  db->sections[db->count] = *section;
  db->count++;

  return STEELCHECK_OK;
}

/*
 *  Copies text into dest (of size bytes). Returns false, copying nothing, if it does not fit.
 */

static bool copy_view_ ( char* dest, size_t size, SteelCheckView text )
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
 *  Keeps a "# key: value" line. Entries that do not fit, beyond the maximum, or with a key seen
 *  before, are ignored with a warning: metadata only describes the file, it is never needed.
 */

static void store_metadata_ ( SteelCheckDatabase* db, const SteelCheckMetadata* meta,
                              size_t line, const SteelCheckReporter* reporter )
{
  int length = steelcheck_view_print_length( meta->key );

  if ( db->metadata_count == steelcheck_metadata_max )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_VALUE, line, 0,
                        "metadata '%.*s' ignored (at most %zu entries)", length, meta->key.data,
                        steelcheck_metadata_max );
    return;
  }

  for ( size_t i = 0; i < db->metadata_count; i++ )
  {
    if ( steelcheck_view_equals_cstr( meta->key, db->metadata[i].key ) )
    {
      steelcheck_report ( reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_DUPLICATE,
                          line, 0, "metadata '%.*s' given twice; the first value is kept",
                          length, meta->key.data );
      return;
    }
  }

  SteelCheckMetadataEntry* entry = &db->metadata[db->metadata_count];

  if ( !copy_view_( entry->key, sizeof entry->key, meta->key )
       || !copy_view_( entry->value, sizeof entry->value, meta->value ) )
  {
    *entry = (SteelCheckMetadataEntry) {};

    steelcheck_report ( reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_VALUE, line, 0,
                        "metadata '%.*s' ignored (too long)", length, meta->key.data );
    return;
  }

  db->metadata_count++;
}

//==================================================================================================
//  steelcheck_database_load
//==================================================================================================

SteelCheckStatus steelcheck_database_load ( SteelCheckDatabase* db, SteelCheckView text,
                                            const SteelCheckReporter* reporter )
{
  if ( db == nullptr || db->sections != nullptr || db->count != 0 || db->metadata_count != 0 )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  SteelCheckLines     lines       = {};
  SteelCheckView      line        = {};
  size_t              line_number = 0;
  SteelCheckMetadata  meta        = {};
  SteelCheckCsvHeader header      = {};
  SteelCheckView      fields[STEELCHECK_FIELD_COUNT];
  SteelCheckSection   section     = {};

  bool             have_header = false;
  SteelCheckStatus first       = STEELCHECK_OK;   // status of the first error
  SteelCheckStatus status      = STEELCHECK_OK;

  steelcheck_lines_init ( &lines, text );

  while ( steelcheck_lines_next( &lines, &line, &line_number ) )
  {
    switch ( steelcheck_csv_classify( line, &meta ) )
    {
      case STEELCHECK_LINE_BLANK:
      case STEELCHECK_LINE_COMMENT:
        break;

      case STEELCHECK_LINE_METADATA:
        store_metadata_ ( db, &meta, line_number, reporter );
        break;

      case STEELCHECK_LINE_DATA:
        if ( !have_header )
        {
          // Without a valid header, no row can be read: stop here.

          status = steelcheck_csv_map_header ( &header, steelcheck_section_columns(),
                                               STEELCHECK_FIELD_COUNT, line, line_number,
                                               reporter );
          if ( status != STEELCHECK_OK )
          {
            steelcheck_database_free ( db );
            return status;
          }

          have_header = true;
          break;
        }

        // A data row: each stage runs only if the previous one succeeded.

        status = steelcheck_csv_split_row ( &header, line, line_number, fields,
                                            STEELCHECK_FIELD_COUNT, reporter );

        if ( status == STEELCHECK_OK )
        {
          status = steelcheck_section_from_row ( &section, &header, fields, line_number,
                                                 reporter );
        }

        if ( status == STEELCHECK_OK )
        {
          status = steelcheck_section_validate ( &section, reporter );
        }

        if ( status == STEELCHECK_OK )
        {
          status = add_ ( db, &section, reporter );
        }

        // Out of memory or overflow: nothing more can be done.

        if ( status == STEELCHECK_ERROR_NO_MEMORY || status == STEELCHECK_ERROR_OVERFLOW )
        {
          steelcheck_database_free ( db );
          return status;
        }

        if ( status != STEELCHECK_OK && first == STEELCHECK_OK )
        {
          first = status;
        }
        break;
    }
  }

  if ( !have_header )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_HEADER, 0, 0,
                        "no header line (the text has no data lines)" );
    steelcheck_database_free ( db );
    return STEELCHECK_ERROR_HEADER;
  }

  // Every row has been read and every problem reported; one error rejects the whole text.

  if ( first != STEELCHECK_OK )
  {
    steelcheck_database_free ( db );
    return first;
  }

  if ( db->count == 0 )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_VALUE,
                        header.line, 0, "no sections after the header" );
  }

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_database_load_file
//==================================================================================================

SteelCheckStatus steelcheck_database_load_file ( SteelCheckDatabase* db, const char* path,
                                                 const SteelCheckReporter* reporter )
{
  if ( db == nullptr || path == nullptr || db->sections != nullptr || db->count != 0
       || db->metadata_count != 0 )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  SteelCheckBuffer buff   = {};
  SteelCheckStatus status = steelcheck_buffer_load ( &buff, path );

  if ( status != STEELCHECK_OK )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, status, 0, 0, "%s",
                        steelcheck_status_message( status ) );
    return status;
  }

  // The database copies what it keeps, so the buffer can be freed right after loading.

  status = steelcheck_database_load ( db, steelcheck_buffer_view( &buff ), reporter );

  steelcheck_buffer_free ( &buff );

  return status;
}

//==================================================================================================
//  steelcheck_database_load_builtin
//==================================================================================================

SteelCheckStatus steelcheck_database_load_builtin ( SteelCheckDatabase* db,
                                                    const SteelCheckReporter* reporter )
{
  if ( !steelcheck_builtin_available() )
  {
    return STEELCHECK_ERROR_UNAVAILABLE;
  }

  return steelcheck_database_load ( db, steelcheck_builtin_text(), reporter );
}

//==================================================================================================
//  steelcheck_database_free
//==================================================================================================

void steelcheck_database_free ( SteelCheckDatabase* db )
{
  if ( db == nullptr )
  {
    return;
  }

  free ( db->sections );

  *db = (SteelCheckDatabase) {};
}

//==================================================================================================
//  steelcheck_database_find
//==================================================================================================

const SteelCheckSection* steelcheck_database_find ( const SteelCheckDatabase* db,
                                                    const char* designation )
{
  char key[key_size_];

  if ( db == nullptr || !steelcheck_designation_key( designation, key, sizeof key ) )
  {
    return nullptr;
  }

  return find_key_( db, key );
}

//==================================================================================================
//  steelcheck_database_metadata
//==================================================================================================

const char* steelcheck_database_metadata ( const SteelCheckDatabase* db, const char* key )
{
  if ( db == nullptr || key == nullptr )
  {
    return nullptr;
  }

  for ( size_t i = 0; i < db->metadata_count; i++ )
  {
    if ( strcmp( db->metadata[i].key, key ) == 0 )
    {
      return db->metadata[i].value;
    }
  }

  return nullptr;
}
