#include "steelcheck/compat.h"
#include "steelcheck/result.h"
#include "steelcheck/trace.h"
#include "suites.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

//==================================================================================================
//  Trace
//==================================================================================================

static void test_trace_add ()
{
  SteelCheckTrace trace = {};

  // Compound literals with designated initializers: each step names only what it has.

  CHECK_INT_EQ ( steelcheck_trace_add( &trace, &(SteelCheckStepInput) {
                   .kind = STEELCHECK_STEP_HEADING, .text = "Flange local buckling" } ),
                 STEELCHECK_OK );

  CHECK_INT_EQ ( steelcheck_trace_add( &trace, &(SteelCheckStepInput) {
                   .kind       = STEELCHECK_STEP_EQUATION,
                   .value      = 9.15,
                   .unit       = STEELCHECK_UNIT_NONE,
                   .symbol     = "lambda_p",
                   .symbol_tex = "\\lambda_p",
                   .expression = "0.38 \\sqrt{E / f_y}",
                   .clause     = "G.2",
                   .text       = "limit of the compact regime" } ),
                 STEELCHECK_OK );

  CHECK_SIZE_EQ ( trace.count, 2 );

  const SteelCheckTraceStep* heading  = &trace.steps[0];
  const SteelCheckTraceStep* equation = &trace.steps[1];

  CHECK_INT_EQ ( heading->kind, STEELCHECK_STEP_HEADING );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, heading->text ), "Flange local buckling" );

  // Fields left out are empty strings, through offset 0.

  CHECK_SIZE_EQ ( heading->symbol, 0 );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, heading->symbol ), "" );

  CHECK_NEAR ( equation->value, 9.15, 0.0 );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, equation->symbol ),     "lambda_p" );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, equation->symbol_tex ), "\\lambda_p" );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, equation->expression ), "0.38 \\sqrt{E / f_y}" );
  CHECK_STR_EQ ( steelcheck_trace_text( &trace, equation->clause ),     "G.2" );

  // Invalid offsets and arguments.

  CHECK_STR_EQ ( steelcheck_trace_text( &trace, trace.arena_used + 10 ), "" );
  CHECK_STR_EQ ( steelcheck_trace_text( nullptr, 1 ), "" );
  CHECK_INT_EQ ( steelcheck_trace_add( &trace, nullptr ), STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_trace_add( nullptr, &(SteelCheckStepInput) {} ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  steelcheck_trace_free ( &trace );
  steelcheck_trace_free ( &trace );   // twice

  CHECK ( trace.steps == nullptr && trace.arena == nullptr && trace.count == 0 );
}

static void test_trace_growth ()
{
  // 500 steps with distinct texts: the steps array grows from 32, the arena from 1024 bytes, and
  // the arena moves in memory. Every text must still be found through its offset.

  SteelCheckTrace trace = {};
  bool            added = true;

  for ( int i = 0; i < 500; i++ )
  {
    char text[64];

    snprintf ( text, sizeof text, "step number %d of the growth test", i );

    added = added && steelcheck_trace_add( &trace, &(SteelCheckStepInput) {
                                             .kind = STEELCHECK_STEP_NOTE,
                                             .value = i, .text = text } ) == STEELCHECK_OK;
  }

  CHECK ( added );
  CHECK_SIZE_EQ ( trace.count, 500 );
  CHECK ( trace.capacity >= 500 && trace.arena_capacity >= trace.arena_used );

  bool all_found = true;

  for ( size_t i = 0; i < trace.count; i++ )
  {
    char expected[64];

    snprintf ( expected, sizeof expected, "step number %zu of the growth test", i );

    all_found = all_found && trace.steps[i].value == (double) i
             && strcmp( steelcheck_trace_text( &trace, trace.steps[i].text ), expected ) == 0;
  }

  CHECK ( all_found );

  steelcheck_trace_free ( &trace );
}

static void test_unit_names ()
{
  CHECK_STR_EQ ( steelcheck_unit_name( STEELCHECK_UNIT_NONE ), "" );
  CHECK_STR_EQ ( steelcheck_unit_name( STEELCHECK_UNIT_MM4 ),  "mm^4" );
  CHECK_STR_EQ ( steelcheck_unit_name( STEELCHECK_UNIT_MPA ),  "MPa" );
  CHECK_STR_EQ ( steelcheck_unit_name( STEELCHECK_UNIT_NMM ),  "N mm" );
}

//==================================================================================================
//  Results
//==================================================================================================

/*
 *  A limit state with the given design resistance (nominal = 1.1 x design).
 */

static SteelCheckLimitResult state_ ( SteelCheckLimitState state, double design )
{
  return (SteelCheckLimitResult) { .state = state, .nominal = 1.1 * design, .design = design };
}

static void test_result_init ()
{
  SteelCheckResult result;

  steelcheck_result_init ( &result, "ABNT NBR 8800:2024" );

  CHECK_STR_EQ ( result.code, "ABNT NBR 8800:2024" );
  CHECK ( !result.has_governing );

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    CHECK_INT_EQ ( result.checks[k].kind, k );
    CHECK_INT_EQ ( result.checks[k].status, STEELCHECK_CHECK_NOT_CHECKED );
  }

  steelcheck_result_free ( &result );
  steelcheck_result_free ( &result );
  steelcheck_result_free ( nullptr );
}

static void test_check_governing ()
{
  // Bending about x: LTB 150 kN m, FLB 200 kN m, WLB 210 kN m. LTB governs.

  SteelCheckResult result;

  steelcheck_result_init ( &result, "test" );

  SteelCheckCheck* bending = &result.checks[STEELCHECK_CHECK_BENDING_X];

  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &(SteelCheckLimitResult) {
                   .state = STEELCHECK_LIMIT_FLB, .design = 200.0e6 } ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &(SteelCheckLimitResult) {
                   .state = STEELCHECK_LIMIT_LTB, .design = 150.0e6 } ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &(SteelCheckLimitResult) {
                   .state = STEELCHECK_LIMIT_WLB, .design = 210.0e6 } ), STEELCHECK_OK );

  // Without a design moment: only the resistance.

  steelcheck_check_finish ( bending );

  CHECK_SIZE_EQ ( bending->governing, 1 );
  CHECK_INT_EQ ( bending->states[bending->governing].state, STEELCHECK_LIMIT_LTB );
  CHECK_NEAR ( bending->resistance, 150.0e6, 0.0 );
  CHECK_INT_EQ ( bending->status, STEELCHECK_CHECK_RESISTANCE );

  // With a design moment of -120 kN m (the sign does not matter): 120 / 150 = 0.8, OK.

  bending->has_demand = true;
  bending->demand     = -120.0e6;

  steelcheck_check_finish ( bending );

  CHECK_NEAR ( bending->demand, 120.0e6, 0.0 );
  CHECK_NEAR ( bending->utilization, 0.8, 1e-15 );
  CHECK_INT_EQ ( bending->status, STEELCHECK_CHECK_OK );

  // Exactly 1.0 is still OK; anything above fails.

  bending->demand = 150.0e6;
  steelcheck_check_finish ( bending );
  CHECK_INT_EQ ( bending->status, STEELCHECK_CHECK_OK );

  bending->demand = 150.1e6;
  steelcheck_check_finish ( bending );
  CHECK_INT_EQ ( bending->status, STEELCHECK_CHECK_FAIL );

  // A fifth limit state does not fit.

  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &(SteelCheckLimitResult) {} ),
                 STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &(SteelCheckLimitResult) {} ),
                 STEELCHECK_ERROR_OVERFLOW );

  steelcheck_result_free ( &result );
}

static void test_check_zero_resistance ()
{
  SteelCheckCheck check = { .has_demand = true, .demand = 10.0 };

  CHECK_INT_EQ ( steelcheck_check_add_state( &check, &(SteelCheckLimitResult) {} ),
                 STEELCHECK_OK );

  steelcheck_check_finish ( &check );

  CHECK ( isinf( check.utilization ) );
  CHECK_INT_EQ ( check.status, STEELCHECK_CHECK_FAIL );

  // No force on no resistance: nothing to fail.

  check.demand = 0.0;
  steelcheck_check_finish ( &check );
  CHECK_INT_EQ ( check.status, STEELCHECK_CHECK_OK );

  // A check without limit states keeps its status.

  SteelCheckCheck skipped = { .status = STEELCHECK_CHECK_NOT_APPLICABLE };

  steelcheck_check_finish ( &skipped );
  CHECK_INT_EQ ( skipped.status, STEELCHECK_CHECK_NOT_APPLICABLE );
}

static void test_result_governing ()
{
  // Tension 0.4 (OK), bending 1.2 (FAIL), compression only a resistance: bending governs.

  SteelCheckResult result;

  steelcheck_result_init ( &result, "test" );

  SteelCheckCheck* tension     = &result.checks[STEELCHECK_CHECK_TENSION];
  SteelCheckCheck* bending     = &result.checks[STEELCHECK_CHECK_BENDING_X];
  SteelCheckCheck* compression = &result.checks[STEELCHECK_CHECK_COMPRESSION];

  SteelCheckLimitResult yielding = state_ ( STEELCHECK_LIMIT_YIELDING, 1000.0 );
  SteelCheckLimitResult ltb      = state_ ( STEELCHECK_LIMIT_LTB, 100.0 );
  SteelCheckLimitResult buckling = state_ ( STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y, 5.0 );

  CHECK_INT_EQ ( steelcheck_check_add_state( tension, &yielding ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( bending, &ltb ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( compression, &buckling ), STEELCHECK_OK );

  tension->has_demand = true;
  tension->demand     = 400.0;
  bending->has_demand = true;
  bending->demand     = 120.0;

  steelcheck_check_finish ( tension );
  steelcheck_check_finish ( bending );
  steelcheck_check_finish ( compression );
  steelcheck_result_finish ( &result );

  CHECK ( result.has_governing );
  CHECK_INT_EQ ( result.governing, STEELCHECK_CHECK_BENDING_X );
  CHECK_NEAR ( result.utilization, 1.2, 1e-15 );

  steelcheck_result_free ( &result );
}

static void test_result_names ()
{
  CHECK_STR_EQ ( steelcheck_check_kind_name( STEELCHECK_CHECK_BENDING_X ), "bending about x" );
  CHECK_STR_EQ ( steelcheck_limit_state_name( STEELCHECK_LIMIT_LTB ),
                 "lateral-torsional buckling" );
  CHECK_STR_EQ ( steelcheck_regime_name( STEELCHECK_REGIME_INELASTIC ), "inelastic" );
  CHECK_STR_EQ ( steelcheck_check_status_name( STEELCHECK_CHECK_FAIL ), "FAIL" );
  CHECK_STR_EQ ( steelcheck_check_kind_name( STEELCHECK_CHECK_COUNT ), "unknown check" );
}

/*
 *  Limit states with their own design forces: the largest utilization governs, which here is not
 *  the smallest resistance, and the own forces survive a second finish.
 */

static void test_check_own_demands ()
{
  SteelCheckCheck check = { .has_demand = true, .demand = 150.0 };

  // FLT-like: resistance 100 against its own force -50 (the sign does not matter); FLM-like:
  // resistance 200 against the check's 150.

  CHECK_INT_EQ ( steelcheck_check_add_state( &check, &(SteelCheckLimitResult) {
                   .state = STEELCHECK_LIMIT_LTB, .design = 100.0, .has_demand = true,
                   .demand = -50.0 } ), STEELCHECK_OK );
  CHECK_INT_EQ ( steelcheck_check_add_state( &check, &(SteelCheckLimitResult) {
                   .state = STEELCHECK_LIMIT_FLB, .design = 200.0 } ), STEELCHECK_OK );

  steelcheck_check_finish ( &check );

  CHECK_NEAR ( check.states[0].applied, 50.0, 0.0 );
  CHECK_NEAR ( check.states[0].utilization, 0.5, 1e-15 );
  CHECK_NEAR ( check.states[1].applied, 150.0, 0.0 );
  CHECK_NEAR ( check.states[1].utilization, 0.75, 1e-15 );
  CHECK_SIZE_EQ ( check.governing, 1 );
  CHECK_NEAR ( check.resistance, 200.0, 0.0 );
  CHECK_NEAR ( check.utilization, 0.75, 1e-15 );
  CHECK_NEAR ( check.demand, 150.0, 0.0 );
  CHECK_INT_EQ ( check.status, STEELCHECK_CHECK_OK );

  // A larger force for the check: only the state without its own force changes.

  check.demand = 250.0;
  steelcheck_check_finish ( &check );

  CHECK_NEAR ( check.states[0].applied, 50.0, 0.0 );
  CHECK_NEAR ( check.states[1].utilization, 1.25, 1e-15 );
  CHECK_INT_EQ ( check.status, STEELCHECK_CHECK_FAIL );

  // Without a design force, the smallest resistance governs, as before.

  check.has_demand = false;
  steelcheck_check_finish ( &check );

  CHECK_SIZE_EQ ( check.governing, 0 );
  CHECK_INT_EQ ( check.status, STEELCHECK_CHECK_RESISTANCE );
}

//==================================================================================================
//  test_result_suite
//==================================================================================================

void test_result_suite ()
{
  RUN_TEST ( test_trace_add );
  RUN_TEST ( test_trace_growth );
  RUN_TEST ( test_unit_names );
  RUN_TEST ( test_result_init );
  RUN_TEST ( test_check_governing );
  RUN_TEST ( test_check_zero_resistance );
  RUN_TEST ( test_check_own_demands );
  RUN_TEST ( test_result_governing );
  RUN_TEST ( test_result_names );
}
