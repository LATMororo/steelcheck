#ifndef STEELCHECK_RESULT_H
#define STEELCHECK_RESULT_H

#include "steelcheck/compat.h"
#include "steelcheck/status.h"
#include "steelcheck/trace.h"

#include <stddef.h>

/*
 *  The result of checking a member against a design code. Three levels:
 *
 *    - a limit state: one way the member can fail within a check, with its slenderness, its
 *      regime and its resistance, e.g. lateral-torsional buckling within bending about x;
 *    - a check: one kind of force (tension, compression, bending about x, ...), with its limit
 *      states; the one with the smallest design resistance governs;
 *    - the result: every check, the overall utilization, and the calculation trace.
 */

typedef enum SteelCheckCheckKind : int
{
  STEELCHECK_CHECK_TENSION     = 0,
  STEELCHECK_CHECK_COMPRESSION = 1,
  STEELCHECK_CHECK_BENDING_X   = 2,
  STEELCHECK_CHECK_BENDING_Y   = 3,
  STEELCHECK_CHECK_SHEAR_Y     = 4,   // shear in the web (with bending about x)
  STEELCHECK_CHECK_SHEAR_X     = 5,   // shear in the flanges (with bending about y)
  STEELCHECK_CHECK_INTERACTION = 6,   // axial force and bending together

  STEELCHECK_CHECK_COUNT              // number of kinds, not a kind
} SteelCheckCheckKind;

typedef enum SteelCheckLimitState : int
{
  STEELCHECK_LIMIT_YIELDING            = 0,   // gross section yielding (tension)
  STEELCHECK_LIMIT_RUPTURE             = 1,   // net section rupture (tension)
  STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X = 2,
  STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y = 3,
  STEELCHECK_LIMIT_TORSIONAL_BUCKLING  = 4,
  STEELCHECK_LIMIT_LTB                 = 5,   // lateral-torsional buckling (FLT in NBR 8800)
  STEELCHECK_LIMIT_FLB                 = 6,   // flange local buckling (FLM)
  STEELCHECK_LIMIT_WLB                 = 7,   // web local buckling (FLA)
  STEELCHECK_LIMIT_SHEAR               = 8,   // shear yielding or shear buckling
  STEELCHECK_LIMIT_INTERACTION         = 9,   // the interaction equation
} SteelCheckLimitState;

typedef enum SteelCheckRegime : int
{
  STEELCHECK_REGIME_NONE      = 0,   // no slenderness involved (e.g. yielding)
  STEELCHECK_REGIME_COMPACT   = 1,   // lambda <= lambda_p: the full plastic resistance
  STEELCHECK_REGIME_INELASTIC = 2,   // lambda_p < lambda <= lambda_r
  STEELCHECK_REGIME_ELASTIC   = 3,   // lambda > lambda_r
} SteelCheckRegime;

typedef enum SteelCheckCheckStatus : int
{
  STEELCHECK_CHECK_NOT_CHECKED     = 0,   // not done (yet), or missing input (e.g. no An)
  STEELCHECK_CHECK_NOT_APPLICABLE  = 1,   // does not apply to this member
  STEELCHECK_CHECK_NOT_IMPLEMENTED = 2,   // not supported yet for this code
  STEELCHECK_CHECK_RESISTANCE      = 3,   // resistance computed, no design force given
  STEELCHECK_CHECK_OK              = 4,   // design force <= resistance
  STEELCHECK_CHECK_FAIL            = 5,   // design force > resistance
} SteelCheckCheckStatus;

/*
 *  One limit state of a check. Resistances and forces in internal units (N or N mm).
 *
 *  A limit state may have its own design force (has_demand, demand): the standard can compare
 *  different limit states of one check with different forces (FLT of a member with one flange
 *  restrained is checked against M1, 5.4.2.4). Without one, the check's design force applies.
 *  steelcheck_check_finish fills applied (the force used) and utilization of every state, and
 *  never changes has_demand and demand, so a check can be finished again.
 */

typedef struct SteelCheckLimitResult
{
  SteelCheckLimitState state;
  SteelCheckRegime     regime;
  double               lambda;        // slenderness (0 if not applicable)
  double               lambda_p;      // limit of the compact regime
  double               lambda_r;      // limit of the inelastic regime
  double               nominal;       // characteristic resistance, e.g. N_Rk or M_Rk
  double               design;        // design resistance, e.g. M_Rd = M_Rk / gamma_a1
  bool                 has_demand;    // it has its own design force (set by the code)
  double               demand;        // its own design force (its sign is ignored)
  double               applied;       // the force used: its own, or the check's (>= 0)
  double               utilization;   // applied / design (when the check has a design force)
} SteelCheckLimitResult;

/*
 *  Most limit states one check can have (bending about x has three: LTB, FLB, WLB).
 */

constexpr size_t steelcheck_limit_states_max = 4;

typedef struct SteelCheckCheck
{
  SteelCheckCheckKind   kind;
  SteelCheckCheckStatus status;

  SteelCheckLimitResult states[steelcheck_limit_states_max];
  size_t                state_count;
  size_t                governing;     // index of the governing state (valid if state_count > 0)

  double                resistance;    // design resistance of the governing state
  bool                  has_demand;
  double                demand;        // design force of the check, as a magnitude (>= 0)
  double                utilization;   // of the governing state, whose force is its applied

  size_t                trace_first;   // the steps of this check in the result's trace
  size_t                trace_count;
} SteelCheckCheck;

typedef struct SteelCheckResult
{
  const char*         code;          // e.g. "ABNT NBR 8800:2024" (a string literal, not owned)
  SteelCheckCheck     checks[STEELCHECK_CHECK_COUNT];   // indexed by SteelCheckCheckKind

  bool                has_governing;
  SteelCheckCheckKind governing;     // the check with the largest utilization
  double              utilization;   // the largest utilization of all checks

  SteelCheckTrace     trace;         // owned
} SteelCheckResult;

/*
 *  Prepares an empty result for code: every check NOT_CHECKED, an empty trace.
 */

void steelcheck_result_init ( SteelCheckResult* result, const char* code );

/*
 *  Frees the trace and leaves the result empty. Safe twice and on nullptr.
 */

void steelcheck_result_free ( SteelCheckResult* result );

/*
 *  Adds a limit state to a check. Returns STEELCHECK_ERROR_OVERFLOW if the check already has
 *  steelcheck_limit_states_max, STEELCHECK_ERROR_INVALID_ARGUMENT for nullptr.
 */

[[nodiscard]] SteelCheckStatus steelcheck_check_add_state ( SteelCheckCheck* check,
                                                            const SteelCheckLimitResult* state );

/*
 *  Completes a check from its limit states. With a design force (has_demand): every state gets
 *  its applied force (its own, or the check's) and utilization, the governing state is the one
 *  with the largest utilization, and the check takes its resistance and utilization, and OK or
 *  FAIL (OK up to exactly 1.0). Without one: the governing state is the one with the smallest
 *  design resistance, and the status is RESISTANCE. A check without limit states keeps its status
 *  (e.g. NOT_APPLICABLE).
 */

void steelcheck_check_finish ( SteelCheckCheck* check );

/*
 *  Completes the result: the largest utilization over the checks that are OK or FAIL, and the
 *  check it belongs to (has_governing is false if there is none).
 */

void steelcheck_result_finish ( SteelCheckResult* result );

/*
 *  Names for reports: "tension", "lateral-torsional buckling", "inelastic", "OK", ...
 */

[[nodiscard]] const char* steelcheck_check_kind_name   ( SteelCheckCheckKind kind );
[[nodiscard]] const char* steelcheck_limit_state_name  ( SteelCheckLimitState state );
[[nodiscard]] const char* steelcheck_regime_name       ( SteelCheckRegime regime );
[[nodiscard]] const char* steelcheck_check_status_name ( SteelCheckCheckStatus status );

#endif // STEELCHECK_RESULT_H
