#ifndef STEELCHECK_DATABASE_H
#define STEELCHECK_DATABASE_H

#include "steelcheck/compat.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/section.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>

/*
 *  A section database: every section of one CSV file, validated, in a growable array, plus the
 *  file's metadata ("# key: value" lines).
 *
 *  Lifetime: the database owns its sections. Pointers returned by steelcheck_database_find, and
 *  &db->sections[i], are valid until the database is freed. A database never keeps pointers into
 *  the text it was loaded from: designations are copied into the sections, metadata into the
 *  database, so the text (a file buffer) can be freed right after loading.
 */

//==================================================================================================
//  Metadata
//==================================================================================================

constexpr size_t steelcheck_metadata_max        = 16;   // entries kept; later ones are ignored
constexpr size_t steelcheck_metadata_key_size   = 32;   // including the '\0'
constexpr size_t steelcheck_metadata_value_size = 128;  // including the '\0'

typedef struct SteelCheckMetadataEntry
{
  char key[steelcheck_metadata_key_size];       // e.g. "edition"
  char value[steelcheck_metadata_value_size];   // e.g. "08/25"
} SteelCheckMetadataEntry;

//==================================================================================================
//  Database
//==================================================================================================

typedef struct SteelCheckDatabase
{
  SteelCheckSection*      sections;   // owned array of count sections, in file order
  size_t                  count;
  size_t                  capacity;   // allocated slots; count <= capacity

  SteelCheckMetadataEntry metadata[steelcheck_metadata_max];
  size_t                  metadata_count;
} SteelCheckDatabase;

/*
 *  Loads every section of a CSV text (e.g. a file buffer, or the built-in database) into db.
 *
 *  db must be empty (zero-initialized, e.g. SteelCheckDatabase db = {}, or freed). Each data row
 *  goes through steelcheck_csv_split_row, steelcheck_section_from_row and
 *  steelcheck_section_validate; designations must be unique (compared as by
 *  steelcheck_database_find, so "W150x13.0" and "w150X13.0" are duplicates).
 *
 *  Every problem of the text is reported, then:
 *    - if there was any error, db is left empty and the status of the first error is returned
 *      (STEELCHECK_ERROR_HEADER if there is no header line at all);
 *    - otherwise STEELCHECK_OK is returned, warnings included. A text with a header but no rows
 *      gives an empty database and a warning.
 *  STEELCHECK_ERROR_NO_MEMORY and STEELCHECK_ERROR_OVERFLOW stop the load at once.
 *  STEELCHECK_ERROR_INVALID_ARGUMENT for nullptr or a db that is not empty.
 */

[[nodiscard]] SteelCheckStatus steelcheck_database_load ( SteelCheckDatabase* db,
                                                          SteelCheckView text,
                                                          const SteelCheckReporter* reporter );

/*
 *  Reads the file at path and loads it like steelcheck_database_load. A file that cannot be read
 *  is reported with the status message (e.g. "cannot open file") at line 0, and its status is
 *  returned. Messages do not repeat path: the reporter knows which file it is printing for.
 */

[[nodiscard]] SteelCheckStatus steelcheck_database_load_file ( SteelCheckDatabase* db,
                                                               const char* path,
                                                               const SteelCheckReporter* reporter );

/*
 *  Loads the built-in database (see builtin.h) like steelcheck_database_load. Returns
 *  STEELCHECK_ERROR_UNAVAILABLE if this build has none (a compiler without #embed).
 */

[[nodiscard]] SteelCheckStatus
steelcheck_database_load_builtin ( SteelCheckDatabase* db, const SteelCheckReporter* reporter );

/*
 *  Frees the sections and leaves db empty. Safe on an empty database, twice, and on nullptr.
 */

void steelcheck_database_free ( SteelCheckDatabase* db );

/*
 *  The section with the given designation, or nullptr. Designations are compared through their
 *  keys (see steelcheck_designation_key): "W 150 x 13,0" and "w150X13.0" find W150x13.0, and
 *  "W360x58" finds W360x58.0.
 */

[[nodiscard]] const SteelCheckSection* steelcheck_database_find ( const SteelCheckDatabase* db,
                                                                  const char* designation );

/*
 *  The value of the metadata entry key (exact, case-sensitive), or nullptr if there is none.
 */

[[nodiscard]] const char* steelcheck_database_metadata ( const SteelCheckDatabase* db,
                                                         const char* key );

#endif // STEELCHECK_DATABASE_H
