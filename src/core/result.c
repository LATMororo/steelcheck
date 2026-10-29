#include "steelcheck/result.h"

#include <math.h>

//==================================================================================================
//  steelcheck_result_init
//==================================================================================================

void steelcheck_result_init ( SteelCheckResult* result, const char* code )
{
  if ( result == nullptr )
  {
    return;
  }

  *result = (SteelCheckResult) { .code = code };

  // Every check knows its own kind, so a SteelCheckCheck* is enough to describe it.

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    result->checks[k].kind   = (SteelCheckCheckKind) k;
    result->checks[k].status = STEELCHECK_CHECK_NOT_CHECKED;
  }
}

//==================================================================================================
//  steelcheck_result_free
//==================================================================================================

void steelcheck_result_free ( SteelCheckResult* result )
{
  if ( result == nullptr )
  {
    return;
  }

  steelcheck_trace_free ( &result->trace );

  *result = (SteelCheckResult) {};
}

//==================================================================================================
//  steelcheck_check_add_state
//==================================================================================================

SteelCheckStatus steelcheck_check_add_state ( SteelCheckCheck* check,
                                              const SteelCheckLimitResult* state )
{
  if ( check == nullptr || state == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  if ( check->state_count == steelcheck_limit_states_max )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  check->states[check->state_count] = *state;
  check->state_count++;

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_check_finish
//==================================================================================================

void steelcheck_check_finish ( SteelCheckCheck* check )
{
  if ( check == nullptr || check->state_count == 0 )
  {
    return;
  }

  // Without design forces, the governing limit state is the weakest one: the smallest design
  // resistance.

  if ( !check->has_demand )
  {
    size_t governing = 0;

    for ( size_t i = 1; i < check->state_count; i++ )
    {
      if ( check->states[i].design < check->states[governing].design )
      {
        governing = i;
      }
    }

    check->governing  = governing;
    check->resistance = check->states[governing].design;
    check->status     = STEELCHECK_CHECK_RESISTANCE;
    return;
  }

  // With design forces, every limit state is compared with its own force (or the check's), and
  // the one with the largest utilization governs. With one force for all, that is also the one
  // with the smallest resistance.

  check->demand = fabs ( check->demand );

  size_t governing = 0;

  for ( size_t i = 0; i < check->state_count; i++ )
  {
    SteelCheckLimitResult* state = &check->states[i];

    state->applied = state->has_demand ? fabs( state->demand ) : check->demand;

    // A resistance of zero (or less) can only fail, unless there is no force at all.

    if ( state->design > 0.0 )
    {
      state->utilization = state->applied / state->design;
    }
    else
    {
      state->utilization = state->applied > 0.0 ? (double) INFINITY : 0.0;   // INFINITY: float
    }

    if ( state->utilization > check->states[governing].utilization )
    {
      governing = i;
    }
  }

  const SteelCheckLimitResult* g = &check->states[governing];

  check->governing   = governing;
  check->resistance  = g->design;
  check->utilization = g->utilization;
  check->status      = g->utilization <= 1.0 ? STEELCHECK_CHECK_OK : STEELCHECK_CHECK_FAIL;
}

//==================================================================================================
//  steelcheck_result_finish
//==================================================================================================

void steelcheck_result_finish ( SteelCheckResult* result )
{
  if ( result == nullptr )
  {
    return;
  }

  result->has_governing = false;
  result->utilization   = 0.0;

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck* check = &result->checks[k];

    if ( check->status != STEELCHECK_CHECK_OK && check->status != STEELCHECK_CHECK_FAIL )
    {
      continue;
    }

    if ( !result->has_governing || check->utilization > result->utilization )
    {
      result->has_governing = true;
      result->governing     = check->kind;
      result->utilization   = check->utilization;
    }
  }
}

//==================================================================================================
//  Names
//==================================================================================================

const char* steelcheck_check_kind_name ( SteelCheckCheckKind kind )
{
  switch ( kind )
  {
    case STEELCHECK_CHECK_TENSION:
      return "tension";

    case STEELCHECK_CHECK_COMPRESSION:
      return "compression";

    case STEELCHECK_CHECK_BENDING_X:
      return "bending about x";

    case STEELCHECK_CHECK_BENDING_Y:
      return "bending about y";

    case STEELCHECK_CHECK_SHEAR_Y:
      return "shear along y";

    case STEELCHECK_CHECK_SHEAR_X:
      return "shear along x";

    case STEELCHECK_CHECK_INTERACTION:
      return "axial force and bending";

    case STEELCHECK_CHECK_COUNT:
      break;
  }

  return "unknown check";
}

const char* steelcheck_limit_state_name ( SteelCheckLimitState state )
{
  switch ( state )
  {
    case STEELCHECK_LIMIT_YIELDING:
      return "yielding of the gross section";

    case STEELCHECK_LIMIT_RUPTURE:
      return "rupture of the net section";

    case STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X:
      return "flexural buckling about x";

    case STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y:
      return "flexural buckling about y";

    case STEELCHECK_LIMIT_TORSIONAL_BUCKLING:
      return "torsional buckling";

    case STEELCHECK_LIMIT_LTB:
      return "lateral-torsional buckling";

    case STEELCHECK_LIMIT_FLB:
      return "flange local buckling";

    case STEELCHECK_LIMIT_WLB:
      return "web local buckling";

    case STEELCHECK_LIMIT_SHEAR:
      return "shear";

    case STEELCHECK_LIMIT_INTERACTION:
      return "interaction";
  }

  return "unknown limit state";
}

const char* steelcheck_regime_name ( SteelCheckRegime regime )
{
  switch ( regime )
  {
    case STEELCHECK_REGIME_NONE:
      return "";

    case STEELCHECK_REGIME_COMPACT:
      return "compact";

    case STEELCHECK_REGIME_INELASTIC:
      return "inelastic";

    case STEELCHECK_REGIME_ELASTIC:
      return "elastic";
  }

  return "unknown regime";
}

const char* steelcheck_check_status_name ( SteelCheckCheckStatus status )
{
  switch ( status )
  {
    case STEELCHECK_CHECK_NOT_CHECKED:
      return "not checked";

    case STEELCHECK_CHECK_NOT_APPLICABLE:
      return "not applicable";

    case STEELCHECK_CHECK_NOT_IMPLEMENTED:
      return "not implemented";

    case STEELCHECK_CHECK_RESISTANCE:
      return "resistance";

    case STEELCHECK_CHECK_OK:
      return "OK";

    case STEELCHECK_CHECK_FAIL:
      return "FAIL";
  }

  return "unknown status";
}
