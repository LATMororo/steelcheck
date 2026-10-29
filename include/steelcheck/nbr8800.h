#ifndef STEELCHECK_NBR8800_H
#define STEELCHECK_NBR8800_H

#include "steelcheck/compat.h"
#include "steelcheck/code.h"

/*
 *  ABNT NBR 8800:2024: design of steel structures, for rolled doubly symmetric I sections (W and
 *  HP). The trace cites the clauses of the 2024 edition: 5.2 tension; 5.3 compression, with the
 *  effective area of 5.3.4; 5.4.2 and Annex D bending; 5.4.3 shear; 5.5.1.2 interaction.
 *
 *  Partial factors: gamma_a1 = 1.10 (yielding, buckling), gamma_a2 = 1.35 (rupture), for normal
 *  combinations.
 */

constexpr double steelcheck_nbr8800_gamma_a1 = 1.10;
constexpr double steelcheck_nbr8800_gamma_a2 = 1.35;

/*
 *  The NBR 8800 code table (see code.h).
 */

[[nodiscard]] const SteelCheckCode* steelcheck_code_nbr8800 ();

#endif // STEELCHECK_NBR8800_H
