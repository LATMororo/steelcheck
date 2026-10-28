#ifndef STEELCHECK_CSV_H
#define STEELCHECK_CSV_H

#include "steelcheck/compat.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>
#include <stdint.h>

/*
 *  The CSV layer: lines (from lines.h) become fields, and the header row is matched against a
 *  table of known columns supplied by the caller. It knows nothing about steel sections.
 *
 *  Restricted CSV format:
 *    - fields are separated by ',' and trimmed of spaces and tabs;
 *    - no quoting: a '"' anywhere in a field is a syntax error, as is a '\0' byte;
 *    - blank lines are ignored;
 *    - lines whose first non-blank character is '#' are comments; a comment of the form
 *      "# key: value", with a key made of letters, digits and '_', is metadata;
 *    - the first other line is the header; every following line is a data row with exactly as
 *      many fields as the header.
 */

//==================================================================================================
//  Line classification
//==================================================================================================

typedef enum SteelCheckLineKind : int
{
  STEELCHECK_LINE_BLANK    = 0,   // empty, or only spaces and tabs
  STEELCHECK_LINE_COMMENT  = 1,   // first non-blank character is '#'
  STEELCHECK_LINE_METADATA = 2,   // a comment of the form "# key: value"
  STEELCHECK_LINE_DATA     = 3,   // anything else: the header or a data row
} SteelCheckLineKind;

typedef struct SteelCheckMetadata
{
  SteelCheckView key;     // trimmed, e.g. "edition"
  SteelCheckView value;   // trimmed, may be empty, e.g. "08/25"
} SteelCheckMetadata;

/*
 *  Classifies line. For STEELCHECK_LINE_METADATA, *metadata receives the key and the value; for
 *  every other kind it is set to empty views. metadata may be nullptr.
 */

[[nodiscard]] SteelCheckLineKind steelcheck_csv_classify ( SteelCheckView line,
                                                           SteelCheckMetadata* metadata );

//==================================================================================================
//  Field iterator
//==================================================================================================

/*
 *  Goes through the comma-separated fields of one line, like SteelCheckLines goes through lines.
 *  Every line has at least one field: "" gives one empty field, "a," gives "a" and an empty field.
 *  It only splits: checking for '"' and '\0' is done by the header and row functions below.
 */

typedef struct SteelCheckFields
{
  const char* curr;     // beginning of the next field
  const char* end;      // one past the last byte of the line
  size_t      column;   // number of the most recently produced field (0 before the first)
  bool        done;     // true once the last field has been produced
} SteelCheckFields;

void steelcheck_fields_init ( SteelCheckFields* iter, SteelCheckView line );

/*
 *  Produces the next field, trimmed, and its 1-based column number. Returns false when there are
 *  no more fields (or an argument is nullptr); field and column are then set to empty and 0.
 */

[[nodiscard]] bool steelcheck_fields_next ( SteelCheckFields* iter, SteelCheckView* field,
                                            size_t* column );

//==================================================================================================
//  Header mapping
//==================================================================================================

/*
 *  Maximum number of columns in a file, and of column specs. Fixed-size arrays avoid allocation;
 *  the Gerdau catalog has 27 columns.
 */

constexpr size_t steelcheck_csv_max_columns = 64;

/*
 *  Index value meaning "no such column" / "no such spec".
 */

constexpr size_t steelcheck_csv_none = SIZE_MAX;

/*
 *  One known column, supplied by the caller in a table (an array of specs).
 */

typedef struct SteelCheckColumnSpec
{
  const char* name;       // exact, case-sensitive column name, e.g. "Ix_cm4"
  bool        required;   // a missing required column is a header error
} SteelCheckColumnSpec;

/*
 *  Result of mapping a header line against a spec table. Indices are 0-based.
 */

typedef struct SteelCheckCsvHeader
{
  const SteelCheckColumnSpec* specs;        // the spec table (borrowed, not copied)
  size_t                      spec_count;
  size_t                      column_count; // number of columns in the file's header
  size_t                      line;         // line number of the header, for messages

  // spec_of_column[c]: index of the spec that file column c is, or steelcheck_csv_none if the
  // column is unknown (ignored). column_of_spec[s]: file column of spec s, or steelcheck_csv_none
  // if the file does not have it.

  size_t spec_of_column[steelcheck_csv_max_columns];
  size_t column_of_spec[steelcheck_csv_max_columns];
} SteelCheckCsvHeader;

/*
 *  Maps the header line (found at line_number) against specs[0 .. spec_count-1].
 *
 *  Reports every problem to reporter (which may be nullptr), not only the first:
 *    error:   empty column name, duplicate column, '"' or '\0' in a name, too many columns,
 *             missing required column;
 *    warning: unknown column (it is ignored).
 *
 *  Returns STEELCHECK_OK if there was no error (warnings are allowed), STEELCHECK_ERROR_HEADER
 *  otherwise, or STEELCHECK_ERROR_INVALID_ARGUMENT for a nullptr argument, an empty spec table or
 *  more than steelcheck_csv_max_columns specs.
 */

[[nodiscard]] SteelCheckStatus steelcheck_csv_map_header ( SteelCheckCsvHeader* header,
                                                           const SteelCheckColumnSpec* specs,
                                                           size_t spec_count,
                                                           SteelCheckView line,
                                                           size_t line_number,
                                                           const SteelCheckReporter* reporter );

//==================================================================================================
//  Data rows
//==================================================================================================

/*
 *  Splits a data row into fields[0 .. header->spec_count-1], arranged by spec: fields[s] is the
 *  row's field for spec s. Fields of unknown columns are skipped; a spec the file does not have
 *  gets an empty view, exactly like an empty field. field_count is the capacity of fields and
 *  must be at least header->spec_count.
 *
 *  The views point into line. On a syntax error ('"' or '\0' in a field, or a field count that
 *  differs from the header), every problem is reported, all fields are set to empty views, and
 *  STEELCHECK_ERROR_SYNTAX is returned.
 */

[[nodiscard]] SteelCheckStatus steelcheck_csv_split_row ( const SteelCheckCsvHeader* header,
                                                          SteelCheckView line,
                                                          size_t line_number,
                                                          SteelCheckView* fields,
                                                          size_t field_count,
                                                          const SteelCheckReporter* reporter );

#endif // STEELCHECK_CSV_H
