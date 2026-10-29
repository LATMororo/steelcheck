#ifndef STEELCHECK_MATERIAL_H
#define STEELCHECK_MATERIAL_H

#include "steelcheck/compat.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/status.h"

#include <stddef.h>

/*
 *  Structural steel grades. Stresses and moduli in MPa (N/mm^2).
 */

/*
 *  Moduli of structural steel, the same for every grade (ABNT NBR 8800).
 */

constexpr double steelcheck_steel_E = 200000.0;   // modulus of elasticity [MPa]
constexpr double steelcheck_steel_G = 77000.0;    // shear modulus [MPa]

/*
 *  Size of the name array, including the '\0'.
 */

constexpr size_t steelcheck_material_name_size = 32;

typedef struct SteelCheckMaterial
{
  char   name[steelcheck_material_name_size];   // e.g. "ASTM A572 Gr.50"
  double fy;   // yield strength [MPa]
  double fu;   // tensile (ultimate) strength [MPa]
  double E;    // modulus of elasticity [MPa]
  double G;    // shear modulus [MPa]
} SteelCheckMaterial;

/*
 *  The built-in grades: *count receives their number. The array lives for the whole program.
 */

[[nodiscard]] const SteelCheckMaterial* steelcheck_materials ( size_t* count );

/*
 *  The built-in grade with the given name, or nullptr. The comparison ignores letter case,
 *  spaces, '.', '-' and '_', and the standard's name may be left out: "ASTM A572 Gr.50",
 *  "a572 gr50" and "A572-GR-50" all find ASTM A572 Gr.50; "MR250" finds NBR 7007 MR250.
 */

[[nodiscard]] const SteelCheckMaterial* steelcheck_material_find ( const char* name );

/*
 *  Fills *material with a grade of the user's: name, fy and fu, with the standard E and G.
 *  Reports every problem: fy or fu not positive, fu below fy, a name too long. Returns
 *  STEELCHECK_OK, or STEELCHECK_ERROR_VALUE (material is then all zeros), or
 *  STEELCHECK_ERROR_INVALID_ARGUMENT for nullptr.
 */

[[nodiscard]] SteelCheckStatus steelcheck_material_make ( SteelCheckMaterial* material,
                                                          const char* name, double fy, double fu,
                                                          const SteelCheckReporter* reporter );

#endif // STEELCHECK_MATERIAL_H
