#ifndef STEELCHECK_SECTION_H
#define STEELCHECK_SECTION_H

#include "steelcheck/compat.h"
#include "steelcheck/csv.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>
#include <stdint.h>

/*
 *  Rolled W and HP steel cross sections.
 *
 *  Internal units are N, mm and MPa: lengths in mm, areas in mm^2, section moduli in mm^3, second
 *  moments of area and the torsion constant in mm^4, the warping constant in mm^6. The catalog's
 *  units (cm, cm^2, cm^4, cm^6) are converted when a row is read, and nowhere else.
 */

//==================================================================================================
//  Fields
//==================================================================================================

/*
 *  Every field of a section that can come from a CSV column, in the order of the Gerdau catalog
 *  (plus r, which the catalog does not have). The value of each enumerator is used three ways:
 *  as the index into the column table, as the index of the field in a split CSV row, and as the
 *  bit number in SteelCheckSection.present.
 */

typedef enum SteelCheckField : int
{
  STEELCHECK_FIELD_DESIGNATION,           // text, e.g. "W150x13.0"
  STEELCHECK_FIELD_FAMILY,                // text, "W" or "HP"
  STEELCHECK_FIELD_H_SHAPE,               // flag, marked (H) in the catalog
  STEELCHECK_FIELD_ON_REQUEST,            // flag, marked * in the catalog
  STEELCHECK_FIELD_MASS,
  STEELCHECK_FIELD_D,
  STEELCHECK_FIELD_BF,
  STEELCHECK_FIELD_TW,
  STEELCHECK_FIELD_TF,
  STEELCHECK_FIELD_H,
  STEELCHECK_FIELD_DP,
  STEELCHECK_FIELD_R,
  STEELCHECK_FIELD_A,
  STEELCHECK_FIELD_IX,
  STEELCHECK_FIELD_WX,
  STEELCHECK_FIELD_RX,
  STEELCHECK_FIELD_ZX,
  STEELCHECK_FIELD_IY,
  STEELCHECK_FIELD_WY,
  STEELCHECK_FIELD_RY,
  STEELCHECK_FIELD_ZY,
  STEELCHECK_FIELD_RT,
  STEELCHECK_FIELD_J,
  STEELCHECK_FIELD_LAMBDA_F,
  STEELCHECK_FIELD_LAMBDA_W,
  STEELCHECK_FIELD_CW,
  STEELCHECK_FIELD_U,
  STEELCHECK_FIELD_DESIGNATION_IMPERIAL,  // text, e.g. "W6x8.5"

  STEELCHECK_FIELD_COUNT                  // number of fields, not a field
} SteelCheckField;

static_assert ( STEELCHECK_FIELD_COUNT <= 32, "SteelCheckSection.present has 32 bits" );
static_assert ( STEELCHECK_FIELD_COUNT <= steelcheck_csv_max_columns, "too many fields for csv.h" );

//==================================================================================================
//  Section
//==================================================================================================

typedef enum SteelCheckFamily : int
{
  STEELCHECK_FAMILY_W  = 0,
  STEELCHECK_FAMILY_HP = 1,
} SteelCheckFamily;

/*
 *  Size of the designation arrays, including the '\0': designations are at most 31 characters.
 */

constexpr size_t steelcheck_designation_size = 32;

typedef struct SteelCheckSection
{
  // Identity.

  char             designation[steelcheck_designation_size];          // e.g. "W150x13.0"
  char             designation_imperial[steelcheck_designation_size]; // e.g. "W6x8.5", or ""
  SteelCheckFamily family;
  bool             h_shape;      // wide-flange H shape, marked (H) in the catalog
  bool             on_request;   // produced on request, marked * in the catalog
  double           mass;         // kg/m

  // Geometry [mm].

  double d;     // total depth
  double bf;    // flange width
  double tw;    // web thickness
  double tf;    // flange thickness
  double h;     // clear distance between flanges, d - 2 tf
  double dp;    // d': flat part of the web, without the fillets
  double r;     // fillet radius (not in the Gerdau catalog)

  // Properties.

  double A;     // gross area [mm^2]
  double Ix;    // second moment of area, major axis [mm^4]
  double Wx;    // elastic section modulus, major axis [mm^3]
  double rx;    // radius of gyration, major axis [mm]
  double Zx;    // plastic section modulus, major axis [mm^3]
  double Iy;    // second moment of area, minor axis [mm^4]
  double Wy;    // elastic section modulus, minor axis [mm^3]
  double ry;    // radius of gyration, minor axis [mm]
  double Zy;    // plastic section modulus, minor axis [mm^3]
  double rt;    // radius of gyration of the compression flange + 1/3 of the compressed web [mm]
  double J;     // torsion constant (I_t in the catalog) [mm^4]
  double Cw;    // warping constant [mm^6]
  double u;     // surface area per length [m^2/m]

  // Slenderness ratios as printed in the catalog [-].

  double lambda_f;  // bf / (2 tf)
  double lambda_w;  // d' / tw

  // Bookkeeping.

  uint32_t present;      // bit f set: field f was given in the file (see steelcheck_section_has)
  size_t   source_line;  // line of the CSV file the section was read from
} SteelCheckSection;

//==================================================================================================
//  Functions
//==================================================================================================

/*
 *  The column table of the section CSV format: STEELCHECK_FIELD_COUNT specs, indexed by
 *  SteelCheckField, e.g. columns[STEELCHECK_FIELD_IX] is { "Ix_cm4", required }. Pass it to
 *  steelcheck_csv_map_header().
 */

[[nodiscard]] const SteelCheckColumnSpec* steelcheck_section_columns ();

/*
 *  True if field was given in the file the section was read from.
 */

[[nodiscard]] bool steelcheck_section_has ( const SteelCheckSection* section,
                                            SteelCheckField field );

/*
 *  "W" and "HP"
 */

[[nodiscard]] const char* steelcheck_family_name ( SteelCheckFamily family );

/*
 *  Builds a section from one split CSV row.
 *
 *  header must have been mapped with steelcheck_section_columns(), and fields is the row split by
 *  steelcheck_csv_split_row() (STEELCHECK_FIELD_COUNT views, indexed by SteelCheckField).
 *
 *  Every problem of the row is reported, with the column it comes from, e.g.
 *  "column 'd_mm': invalid number '12mm'". Required fields must not be empty; optional fields may
 *  be. Numbers are converted to internal units. Plausibility (positive values, consistency) is
 *  checked later.
 *
 *  Returns STEELCHECK_OK, or the status of the first problem (with *section set to all zeros);
 *  STEELCHECK_ERROR_INVALID_ARGUMENT for a nullptr argument or a header mapped with another table.
 */

[[nodiscard]] SteelCheckStatus steelcheck_section_from_row ( SteelCheckSection* section,
                                                             const SteelCheckCsvHeader* header,
                                                             const SteelCheckView* fields,
                                                             size_t line_number,
                                                             const SteelCheckReporter* reporter );

#endif // STEELCHECK_SECTION_H
