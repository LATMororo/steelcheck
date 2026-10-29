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
  double r;     // fillet radius; computed as (h - d') / 2 when not given (see validate)

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

  // Slenderness ratios [-]. Read from the catalog, then replaced by steelcheck_section_validate
  // with the values computed from the dimensions (the printed ones are rounded); the printed
  // values are only compared with the computed ones.

  double lambda_f;  // bf / (2 tf)
  double lambda_w;  // d' / tw

  // Derived by steelcheck_section_validate (no CSV column).

  double h0;    // distance between the flange centroids, d - tf [mm]
  double r0;    // polar radius of gyration about the shear centre, sqrt(rx^2 + ry^2) [mm]

  // Bookkeeping.

  uint32_t present;      // bit f set: field f was given in the file (see steelcheck_section_has)
  uint32_t computed;     // bit f set: field f was computed by the program (r, lambda_f, lambda_w)
  size_t   source_line;  // line of the CSV file the section was read from
} SteelCheckSection;

/*
 *  Relative tolerances of the consistency checks of steelcheck_section_validate. Catalog values
 *  are rounded, so they never match the computed ones exactly. In the Gerdau catalog the largest
 *  differences are 0.5 % for exact relations, 0.9 % for Cw and 2.9 % for the mass.
 */

constexpr double steelcheck_tolerance_exact  = 0.01;  // h, A, Wx, Wy, rx, ry, lambda_f, lambda_w
constexpr double steelcheck_tolerance_approx = 0.03;  // Cw ~ Iy h0^2 / 4 (the fillets are ignored)
constexpr double steelcheck_tolerance_mass   = 0.05;  // mass ~ A x 7850 kg/m^3 (nominal masses)
constexpr double steelcheck_tolerance_name   = 0.005; // mass in the designation vs mass_kg_m

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
 *  True if field was computed by steelcheck_section_validate instead of read (r, lambda_f,
 *  lambda_w).
 */

[[nodiscard]] bool steelcheck_section_was_computed ( const SteelCheckSection* section,
                                                     SteelCheckField field );

/*
 *  "W" and "HP"
 */

[[nodiscard]] const char* steelcheck_family_name ( SteelCheckFamily family );

/*
 *  Writes the comparison key of a designation into key (of key_size bytes): without spaces and
 *  tabs, letters in upper case, ',' replaced by '.', and ".0" added if there is no '.', e.g.
 *  "W 150 x 13,0" -> "W150X13.0" and "W360x58" -> "W360X58.0". Two designations are the same
 *  if their keys are equal. Returns false (key then holds "") if the result is empty or does not
 *  fit.
 */

[[nodiscard]] bool steelcheck_designation_key ( const char* designation, char* key,
                                                size_t key_size );

/*
 *  Size of a buffer that always holds the key of a designation of a SteelCheckSection: its at
 *  most 31 characters, ".0", and the '\0'.
 */

constexpr size_t steelcheck_designation_key_size = steelcheck_designation_size + 2;

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

/*
 *  Checks a section built by steelcheck_section_from_row, and completes it.
 *
 *  1. Errors (the section cannot be used), all reported:
 *       - a value that is not positive;
 *       - impossible geometry: 2 tf >= d, tw >= bf, h >= d, d' > h;
 *       - Zx < Wx, Zy < Wy (plastic modulus below the elastic one), Iy > Ix (axes swapped?);
 *       - a designation that does not match the family (W... for W, HP... for HP).
 *  2. Only if there is no error, derived values: r = (h - d') / 2 if r was not given, h0, r0,
 *     lambda_f = bf / (2 tf), lambda_w = d' / tw.
 *  3. Warnings (the section is used anyway), for catalog values that differ from what the other
 *     values imply by more than the tolerances above: h, A, mass, Wx, Wy, rx, ry, Cw and the
 *     printed lambda_f and lambda_w; and the mass in the designation (e.g. 153.0 in W610x153.0)
 *     against mass_kg_m.
 *
 *  Messages start with the designation and name the catalog column, with values in the catalog's
 *  units, e.g. "W150x13.0: Wx_cm3 94.4 differs from 2 Ix / d = 85.81 (+10.0 %)". They carry the
 *  section's source line; the column is 0 (several columns are involved).
 *
 *  Returns STEELCHECK_OK if there was no error (warnings are allowed), STEELCHECK_ERROR_VALUE
 *  otherwise (the section is then left unchanged), or STEELCHECK_ERROR_INVALID_ARGUMENT for
 *  nullptr.
 */

[[nodiscard]] SteelCheckStatus steelcheck_section_validate ( SteelCheckSection* section,
                                                             const SteelCheckReporter* reporter );

#endif // STEELCHECK_SECTION_H
