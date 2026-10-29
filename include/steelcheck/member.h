#ifndef STEELCHECK_MEMBER_H
#define STEELCHECK_MEMBER_H

#include "steelcheck/compat.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/material.h"
#include "steelcheck/section.h"
#include "steelcheck/status.h"

#include <stddef.h>

/*
 *  A member to check: a section of a material, with its lengths, buckling factors and, optionally,
 *  its design forces. Internal units: mm, N, N·mm, MPa.
 *
 *  Axes: x is the major axis (parallel to the flanges), y the minor axis (parallel to the web).
 *  Bending about x is resisted mainly by the flanges; the shear force that goes with it, Vy, acts
 *  along y, in the plane of the web.
 */

/*
 *  Size of the label array, including the '\0'.
 */

constexpr size_t steelcheck_member_label_size = 32;

typedef struct SteelCheckMember
{
  char label[steelcheck_member_label_size];   // e.g. "C1" (may be empty)

  // What the member is made of. Borrowed: the database and the material must outlive the member.

  const SteelCheckSection*  section;
  const SteelCheckMaterial* material;

  // Lengths [mm] and effective length factors: the buckling lengths are Kx Lx, Ky Ly and Kz Lz.

  double Lx;   // unbraced length for buckling about x
  double Ly;   // unbraced length for buckling about y
  double Lz;   // unbraced length for torsional buckling
  double Kx;
  double Ky;
  double Kz;

  // Lateral-torsional buckling.

  double Lb;   // length between lateral bracings of the compression flange [mm]; 0: braced
  double Cb;   // moment gradient factor: 1.0 for a uniform moment (5.4.2.3), no upper limit

  // Or the moment diagram over Lb, from which the code computes Cb instead (NBR 8800:2024,
  // 5.4.2.3 a)). Values [N mm]; their signs are ignored.

  bool   has_moments;   // MA, MB and MC are given: Cb is computed, the Cb above is not used
  double Mmax;          // largest moment in Lb; 0: the largest of MA, MB, MC and MxSd
  double MA;            // moment at a quarter of Lb
  double MB;            // moment at the middle of Lb
  double MC;            // moment at three quarters of Lb

  // Or, when one flange has continuous lateral restraint and the other is free (5.4.2.4 a)), the
  // moments from which the code computes Cb [N mm]. Signs: negative compresses the free flange,
  // positive tensions it. Lb is then the unbraced length of the free flange.

  bool   has_free_flange;   // M1, M2 and MCL are given: Cb is computed from them
  double M1;                // largest moment at the ends that compresses the free flange (< 0)
  double M2;                // moment at the other end
  double MCL;               // moment at the middle of Lb

  // Tension: the net section at connections. 0 means not given; the check is then not done.

  double An;   // net area [mm^2]
  double Ct;   // reduction coefficient of the net area (shear lag), 0 < Ct <= 1

  // Design forces (factored), all optional together. Signs: tension positive.

  bool   has_actions;
  double NSd;    // axial force [N]
  double MxSd;   // bending moment about x [N mm]
  double MySd;   // bending moment about y [N mm]
  double VySd;   // shear force along y, in the web [N]
  double VxSd;   // shear force along x, in the flanges [N]
} SteelCheckMember;

/*
 *  Fills *member with the defaults: the given label, section and material; every unbraced length
 *  (Lx, Ly, Lz and Lb) equal to length; every K = 1.0; Cb = 1.0; no net section; no forces.
 *  Returns STEELCHECK_ERROR_INVALID_ARGUMENT for nullptr or a label that does not fit (the member
 *  is then all zeros).
 */

[[nodiscard]] SteelCheckStatus steelcheck_member_init ( SteelCheckMember* member,
                                                        const char* label,
                                                        const SteelCheckSection* section,
                                                        const SteelCheckMaterial* material,
                                                        double length );

/*
 *  Checks the member's data (not its strength) and reports every problem, prefixed with
 *  "member '<label>': ". Errors: no section or material; Lx, Ly, Lz, Kx, Ky or Kz not positive;
 *  Lb negative; Cb not positive; with has_moments, MA, MB, MC or Mmax not finite, Mmax
 *  smaller than one of them, or all of them zero; with has_free_flange, M1, M2 or MCL not
 *  finite, M1 not negative, or M2 compressing the free flange more than M1; both has_moments
 *  and has_free_flange; An not in (0, A]; Ct not in (0, 1]; An given
 *  without Ct or the other way round. Warning: a given Cb below 1.0 (valid only from a
 *  stability analysis). Returns STEELCHECK_OK (also with warnings) or STEELCHECK_ERROR_VALUE
 *  (STEELCHECK_ERROR_INVALID_ARGUMENT for nullptr).
 */

[[nodiscard]] SteelCheckStatus steelcheck_member_validate ( const SteelCheckMember* member,
                                                            const SteelCheckReporter* reporter );

#endif // STEELCHECK_MEMBER_H
