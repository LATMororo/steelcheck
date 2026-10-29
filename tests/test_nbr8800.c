#include "steelcheck/compat.h"
#include "steelcheck/code.h"
#include "steelcheck/database.h"
#include "steelcheck/material.h"
#include "steelcheck/member.h"
#include "steelcheck/nbr8800.h"
#include "steelcheck/result.h"
#include "steelcheck/trace.h"
#include "suites.h"
#include "test.h"

#include <string.h>

/*
 *  The expected values come from an independent Python script that implements the formulas of
 *  ABNT NBR 8800:2024 from the standard's text (not from this C code), with the catalog's
 *  properties in mm, N and MPa. Both agree to about 15 digits; the tolerance leaves room for
 *  rounding only.
 */

constexpr double tol_ = 1e-9;

//==================================================================================================
//  Helpers
//==================================================================================================

/*
 *  The Gerdau database and the A572 Gr.50 grade (fy = 345 MPa, fu = 450 MPa).
 */

typedef struct Fixture_
{
  SteelCheckDatabase        db;
  const SteelCheckMaterial* material;
} Fixture_;

static bool fixture_ ( Fixture_* f )
{
  *f = (Fixture_) {};

  if ( steelcheck_database_load_file( &f->db, "data/gerdau_w_hp.csv", nullptr ) != STEELCHECK_OK )
  {
    return false;
  }

  f->material = steelcheck_material_find ( "A572 Gr.50" );

  return f->material != nullptr;
}

/*
 *  A member of the given section and length [mm], with every default (see member.h).
 */

static bool member_ ( const Fixture_* f, SteelCheckMember* m, const char* designation,
                      double length )
{
  const SteelCheckSection* section = steelcheck_database_find ( &f->db, designation );

  return section != nullptr
         && steelcheck_member_init( m, "T", section, f->material, length ) == STEELCHECK_OK;
}

/*
 *  Checks m against NBR 8800 into *r. True on success.
 */

static bool check_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  return steelcheck_check_member( steelcheck_code_nbr8800(), m, r, nullptr ) == STEELCHECK_OK;
}

/*
 *  The limit state of a check, or nullptr if the check does not have it.
 */

static const SteelCheckLimitResult* state_ ( const SteelCheckResult* r, SteelCheckCheckKind kind,
                                             SteelCheckLimitState state )
{
  const SteelCheckCheck* check = &r->checks[kind];

  for ( size_t i = 0; i < check->state_count; i++ )
  {
    if ( check->states[i].state == state )
    {
      return &check->states[i];
    }
  }

  return nullptr;
}

/*
 *  The design resistance of a limit state, or -1 if it is missing (which fails any CHECK_NEAR).
 */

static double design_ ( const SteelCheckResult* r, SteelCheckCheckKind kind,
                        SteelCheckLimitState state )
{
  const SteelCheckLimitResult* s = state_ ( r, kind, state );

  return s != nullptr ? s->design : -1.0;
}

/*
 *  The regime of a limit state, or STEELCHECK_REGIME_NONE if it is missing.
 */

static SteelCheckRegime regime_ ( const SteelCheckResult* r, SteelCheckCheckKind kind,
                                  SteelCheckLimitState state )
{
  const SteelCheckLimitResult* s = state_ ( r, kind, state );

  return s != nullptr ? s->regime : STEELCHECK_REGIME_NONE;
}

//==================================================================================================
//  The code table
//==================================================================================================

static void test_code_find ()
{
  const SteelCheckCode* nbr = steelcheck_code_nbr8800 ();

  CHECK_STR_EQ ( nbr->id, "NBR 8800" );
  CHECK_STR_EQ ( nbr->name, "ABNT NBR 8800" );
  CHECK ( steelcheck_code_default() == nbr );

  // Case, spaces, '.', '-' and '_' do not matter.

  CHECK ( steelcheck_code_find( "NBR 8800" ) == nbr );
  CHECK ( steelcheck_code_find( "nbr8800" ) == nbr );
  CHECK ( steelcheck_code_find( "Nbr-8800" ) == nbr );

  // Codes the program does not know (yet).

  CHECK ( steelcheck_code_find( "AISC 360" ) == nullptr );
  CHECK ( steelcheck_code_find( "NBR 880" ) == nullptr );
  CHECK ( steelcheck_code_find( "" ) == nullptr );
  CHECK ( steelcheck_code_find( nullptr ) == nullptr );

  // Every check of the table is implemented.

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    CHECK ( nbr->checks[k] != nullptr );
  }
}

//==================================================================================================
//  Resistances (no design forces)
//==================================================================================================

static void test_resistances_w310 ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  m.An = 0.85 * m.section->A;
  m.Ct = 0.90;

  CHECK ( check_( &m, &r ) );

  // Without forces, every check but the interaction gives its resistance.

  for ( int k = 0; k < STEELCHECK_CHECK_INTERACTION; k++ )
  {
    CHECK_INT_EQ ( r.checks[k].status, STEELCHECK_CHECK_RESISTANCE );
  }

  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_INTERACTION].status, STEELCHECK_CHECK_NOT_APPLICABLE );
  CHECK ( !r.has_governing );

  // Tension: yielding of the gross section, rupture of the net section (governs).

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_TENSION, STEELCHECK_LIMIT_YIELDING ),
               1558772.727272727, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_TENSION, STEELCHECK_LIMIT_RUPTURE ),
               1267350.0, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_TENSION].resistance, 1267350.0, tol_ );

  // Compression, 3 m: the web is slender (d'/tw = 46.7 > 35.9), so the effective area depends on
  // the chi of each mode (5.3.4.2); buckling about y governs.

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X ),
               1422630.6460979523, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y ),
               985670.5973553301, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_TORSIONAL_BUCKLING ),
               1134950.5433534784, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_COMPRESSION].resistance, 985670.5973553301, tol_ );

  // Bending about x, Lb = 3 m: inelastic FLT governs; compact flange and web.

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
               159393342.27208912, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_FLB ),
               193011818.18181816, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_WLB ),
               193011818.18181816, tol_ );
  CHECK_INT_EQ ( regime_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
                 STEELCHECK_REGIME_INELASTIC );
  CHECK_INT_EQ ( regime_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_FLB ),
                 STEELCHECK_REGIME_COMPACT );

  // Bending about y: the plastic moment, capped at 1.5 Wy fy.

  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_BENDING_Y].resistance, 41447045.45454545, tol_ );

  // Shear: the web (kv = 5.34, no transverse stiffeners) and the flanges (kv = 1.2), both
  // compact.

  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_SHEAR_Y].resistance, 338350.90909090906, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_SHEAR_Y].states[0].lambda_p, 61.20244382730794, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_SHEAR_X].resistance, 602369.9999999999, tol_ );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

/*
 *  Lateral-torsional buckling in its three regimes, and the effect of Cb, which enters through
 *  lambda_r and M_cr (D.2.8 a)), not as a factor of the inelastic interpolation.
 */

static void test_ltb_regimes ()
{
  static const struct
  {
    double           Lb;
    double           Cb;
    double           MRd;
    SteelCheckRegime regime;
  } cases[] =
  {
    { 1000.0, 1.00, 193011818.18181816, STEELCHECK_REGIME_COMPACT },
    { 3000.0, 1.00, 159393342.27208912, STEELCHECK_REGIME_INELASTIC },
    { 3000.0, 1.14, 163355772.2996942,  STEELCHECK_REGIME_INELASTIC },
    { 6000.0, 1.00, 79405267.72210887,  STEELCHECK_REGIME_ELASTIC },
    { 6000.0, 1.30, 103226848.03874156, STEELCHECK_REGIME_ELASTIC },
  };

  Fixture_ f = {};

  CHECK ( fixture_( &f ) );

  for ( size_t i = 0; i < sizeof cases / sizeof cases[0]; i++ )
  {
    SteelCheckMember m = {};
    SteelCheckResult r = {};

    CHECK ( member_( &f, &m, "W310x38.7", 6000.0 ) );

    m.Lb = cases[i].Lb;
    m.Cb = cases[i].Cb;

    CHECK ( check_( &m, &r ) );
    CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ), cases[i].MRd,
                 tol_ );
    CHECK_INT_EQ ( regime_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
                   cases[i].regime );

    steelcheck_result_free ( &r );
  }

  steelcheck_database_free ( &f.db );
}

/*
 *  W150x22.5, an H shape: its flange is in the inelastic FLM range.
 */

static void test_inelastic_flange ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W150x22.5", 2000.0 ) );

  m.Lb = 500.0;

  CHECK ( check_( &m, &r ) );

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_FLB ),
               52985156.58602723, tol_ );
  CHECK_INT_EQ ( regime_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_FLB ),
                 STEELCHECK_REGIME_INELASTIC );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
               56329090.90909091, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_BENDING_X].resistance, 52985156.58602723, tol_ );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_BENDING_Y].resistance, 22303856.768292125, tol_ );

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X ),
               848863.3641666097, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y ),
               730458.5707983876, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_TORSIONAL_BUCKLING ),
               767462.734898563, tol_ );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

static void test_heavy_column ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W610x174.0", 4000.0 ) );
  CHECK ( check_( &m, &r ) );

  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X ),
               6762819.921489992, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y ),
               5659983.772667825, tol_ );
  CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_COMPRESSION, STEELCHECK_LIMIT_TORSIONAL_BUCKLING ),
               6029667.524726521, tol_ );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

/*
 *  A web too slender for Annex D makes the member a beam with a slender web (Annex E): bending
 *  about x is then not implemented, rather than computed without its FLA limit.
 */

static void test_slender_web ()
{
  Fixture_          f       = {};
  SteelCheckMember  m       = {};
  SteelCheckResult  r       = {};
  SteelCheckSection section = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  // No rolled section has such a web: make one, d'/tw = 150 > 5.70 sqrt(E/fy) = 137.

  section    = *m.section;
  section.dp = 150.0 * section.tw;
  m.section  = &section;

  CHECK ( check_( &m, &r ) );

  const SteelCheckCheck* check = &r.checks[STEELCHECK_CHECK_BENDING_X];

  CHECK_INT_EQ ( check->status, STEELCHECK_CHECK_NOT_IMPLEMENTED );
  CHECK_SIZE_EQ ( check->state_count, 0 );

  // The trace says why.

  bool warned = false;

  for ( size_t i = 0; i < check->trace_count; i++ )
  {
    const SteelCheckTraceStep* step = &r.trace.steps[check->trace_first + i];

    if ( step->kind == STEELCHECK_STEP_WARNING
         && strstr( steelcheck_trace_text( &r.trace, step->text ), "Annex E" ) != nullptr )
    {
      warned = true;
    }
  }

  CHECK ( warned );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

/*
 *  Cb computed from the moment diagram (5.4.2.3 a)): the same FLT resistance as with that Cb
 *  given directly.
 */

static void test_cb_from_moments ()
{
  static const struct
  {
    double MA;
    double MB;
    double MC;
    double Mmax;
    double Cb;
  } cases[] =
  {
    { 100e6, 100e6, 100e6, 100e6, 1.0 },                  // uniform moment
    { 75e6,  100e6, 75e6,  100e6, 12.5 / 11.0 },          // uniform load, simply supported
    { 25e6,  50e6,  75e6,  100e6, 12.5 / 7.5 },           // linear, zero at one end
    { -25e6, 0.0,   25e6,  0.0,   625.0 / 275.0 },        // reverse curvature; Mmax = MxSd
    { 0.0,   0.0,   0.0,   100e6, 5.0 },                  // no upper limit (5.4.2.3)
  };

  Fixture_ f = {};

  CHECK ( fixture_( &f ) );

  for ( size_t i = 0; i < sizeof cases / sizeof cases[0]; i++ )
  {
    SteelCheckMember m = {};
    SteelCheckResult r = {};
    SteelCheckResult g = {};

    // Lb = 4 m: inelastic FLT, where Cb changes lambda_r.

    CHECK ( member_( &f, &m, "W310x38.7", 4000.0 ) );

    m.has_actions = true;
    m.MxSd        = 50e6;
    m.has_moments = true;
    m.MA          = cases[i].MA;
    m.MB          = cases[i].MB;
    m.MC          = cases[i].MC;
    m.Mmax        = cases[i].Mmax;

    CHECK ( check_( &m, &r ) );

    // The same member with that Cb given.

    m.has_moments = false;
    m.Cb          = cases[i].Cb;

    CHECK ( check_( &m, &g ) );
    CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
                 design_( &g, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ), tol_ );

    // The value of Cb, from the trace.

    bool found = false;

    for ( size_t s = 0; s < r.trace.count; s++ )
    {
      const SteelCheckTraceStep* step = &r.trace.steps[s];

      if ( strcmp( steelcheck_trace_text( &r.trace, step->symbol_tex ), "C_b" ) == 0 )
      {
        CHECK_NEAR ( step->value, cases[i].Cb, tol_ );
        found = true;
      }
    }

    CHECK ( found );

    steelcheck_result_free ( &r );
    steelcheck_result_free ( &g );
  }

  steelcheck_database_free ( &f.db );
}

/*
 *  Cb of a member with one flange continuously restrained (5.4.2.4 a)), signs as in member.h:
 *  negative compresses the free flange.
 */

static void test_cb_free_flange ()
{
  static const struct
  {
    double M1;
    double M2;
    double MCL;
    double Cb;
  } cases[] =
  {
    { -100e6, -100e6, 50e6,   3.0 },
    { -100e6, 50e6,   0.0,    10.0 / 3.0 },     // M2 tensions the free flange: 0 in third term
    { -100e6, 50e6,   20e6,   58.0 / 15.0 },
    { -100e6, -50e6,  -100e6, 8.0 / 9.0 },
  };

  Fixture_ f = {};

  CHECK ( fixture_( &f ) );

  for ( size_t i = 0; i < sizeof cases / sizeof cases[0]; i++ )
  {
    SteelCheckMember m = {};
    SteelCheckResult r = {};
    SteelCheckResult g = {};

    CHECK ( member_( &f, &m, "W310x38.7", 4000.0 ) );

    m.has_actions     = true;
    m.MxSd            = 100e6;
    m.has_free_flange = true;
    m.M1              = cases[i].M1;
    m.M2              = cases[i].M2;
    m.MCL             = cases[i].MCL;

    CHECK ( check_( &m, &r ) );

    // The same member with that Cb given.

    m.has_free_flange = false;
    m.Cb              = cases[i].Cb;

    CHECK ( check_( &m, &g ) );
    CHECK_NEAR ( design_( &r, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ),
                 design_( &g, STEELCHECK_CHECK_BENDING_X, STEELCHECK_LIMIT_LTB ), tol_ );

    bool found = false;

    for ( size_t s = 0; s < r.trace.count; s++ )
    {
      const SteelCheckTraceStep* step = &r.trace.steps[s];

      if ( strcmp( steelcheck_trace_text( &r.trace, step->symbol_tex ), "C_b" ) == 0 )
      {
        CHECK_NEAR ( step->value, cases[i].Cb, tol_ );
        CHECK_STR_EQ ( steelcheck_trace_text( &r.trace, step->clause ), "5.4.2.4 a)" );
        found = true;
      }
    }

    CHECK ( found );

    steelcheck_result_free ( &r );
    steelcheck_result_free ( &g );
  }

  // FLT against |M1| = 80 kN m, the other limit states against MxSd = 120 kN m.

  {
    SteelCheckMember m = {};
    SteelCheckResult r = {};

    CHECK ( member_( &f, &m, "W310x38.7", 4000.0 ) );

    m.has_actions     = true;
    m.MxSd            = 120e6;
    m.has_free_flange = true;
    m.M1              = -80e6;
    m.M2              = -40e6;
    m.MCL             = 120e6;

    CHECK ( check_( &m, &r ) );

    const SteelCheckCheck*       check = &r.checks[STEELCHECK_CHECK_BENDING_X];
    const SteelCheckLimitResult* ltb   = state_ ( &r, STEELCHECK_CHECK_BENDING_X,
                                                  STEELCHECK_LIMIT_LTB );
    const SteelCheckLimitResult* flb   = state_ ( &r, STEELCHECK_CHECK_BENDING_X,
                                                  STEELCHECK_LIMIT_FLB );

    CHECK ( ltb != nullptr && flb != nullptr );

    if ( ltb != nullptr && flb != nullptr )
    {
      CHECK_NEAR ( ltb->applied, 80e6, 0.0 );
      CHECK_NEAR ( flb->applied, 120e6, 0.0 );
      CHECK_NEAR ( ltb->utilization, 80e6 / ltb->design, tol_ );

      // The governing state has the largest utilization.

      for ( size_t i = 0; i < check->state_count; i++ )
      {
        CHECK ( check->states[i].utilization <= check->utilization );
      }
    }

    steelcheck_result_free ( &r );
  }

  // Moments outside the conditions of 5.4.2.4 a): Cb = -5/3, bending about x is not checked,
  // and neither is the interaction that needs its resistance.

  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( member_( &f, &m, "W310x38.7", 4000.0 ) );

  m.has_actions     = true;
  m.NSd             = -100e3;
  m.MxSd            = 300e6;
  m.has_free_flange = true;
  m.M1              = -100e6;
  m.M2              = -100e6;
  m.MCL             = -300e6;

  CHECK ( check_( &m, &r ) );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_BENDING_X].status, STEELCHECK_CHECK_NOT_CHECKED );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_INTERACTION].status, STEELCHECK_CHECK_NOT_CHECKED );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

//==================================================================================================
//  Design forces
//==================================================================================================

/*
 *  Only the checks the forces call for are done.
 */

static void test_applicability ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  m.has_actions = true;
  m.VySd        = 100e3;

  CHECK ( check_( &m, &r ) );

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    CHECK_INT_EQ ( r.checks[k].status, k == STEELCHECK_CHECK_SHEAR_Y
                                       ? STEELCHECK_CHECK_OK : STEELCHECK_CHECK_NOT_APPLICABLE );
  }

  CHECK ( r.has_governing );
  CHECK_INT_EQ ( r.governing, STEELCHECK_CHECK_SHEAR_Y );
  CHECK_NEAR ( r.utilization, 100e3 / 338350.90909090906, tol_ );

  steelcheck_result_free ( &r );

  // One force and a moment: the interaction too. Tension is positive.

  m.VySd = 0.0;
  m.NSd  = 200e3;
  m.MxSd = 50e6;

  CHECK ( check_( &m, &r ) );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_TENSION].status, STEELCHECK_CHECK_OK );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_COMPRESSION].status, STEELCHECK_CHECK_NOT_APPLICABLE );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_BENDING_X].status, STEELCHECK_CHECK_OK );
  CHECK_INT_EQ ( r.checks[STEELCHECK_CHECK_INTERACTION].status, STEELCHECK_CHECK_OK );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

static void test_interaction ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  // n = 300 / 985.7 >= 0.2: equation a).

  m.has_actions = true;
  m.NSd         = -300e3;
  m.MxSd        = 100e6;

  CHECK ( check_( &m, &r ) );

  const SteelCheckCheck* check = &r.checks[STEELCHECK_CHECK_INTERACTION];

  CHECK_INT_EQ ( check->status, STEELCHECK_CHECK_OK );
  CHECK_NEAR ( check->utilization, 0.8620313390823839, tol_ );
  CHECK ( r.has_governing );
  CHECK_INT_EQ ( r.governing, STEELCHECK_CHECK_INTERACTION );
  CHECK_NEAR ( r.utilization, 0.8620313390823839, tol_ );

  steelcheck_result_free ( &r );

  // n = 50 / 985.7 < 0.2: equation b), with a moment about y too.

  m.NSd  = -50e3;
  m.MySd = 10e6;

  CHECK ( check_( &m, &r ) );
  CHECK_NEAR ( r.checks[STEELCHECK_CHECK_INTERACTION].utilization, 0.8940139404392092, tol_ );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

static void test_failing_tension ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  // Just over the rupture resistance of the net section.

  m.An          = 0.85 * m.section->A;
  m.Ct          = 0.90;
  m.has_actions = true;
  m.NSd         = 1300e3;

  CHECK ( check_( &m, &r ) );

  const SteelCheckCheck* check = &r.checks[STEELCHECK_CHECK_TENSION];

  CHECK_INT_EQ ( check->status, STEELCHECK_CHECK_FAIL );
  CHECK_INT_EQ ( check->states[check->governing].state, STEELCHECK_LIMIT_RUPTURE );
  CHECK_NEAR ( r.utilization, 1300e3 / 1267350.0, tol_ );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

//==================================================================================================
//  The trace and invalid members
//==================================================================================================

static void test_trace_note ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};
  SteelCheckResult r = {};

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );
  CHECK ( check_( &m, &r ) );

  // The first step is the code's note about the edition of the formulas.

  CHECK ( r.trace.count > 0 );
  CHECK_INT_EQ ( r.trace.steps[0].kind, STEELCHECK_STEP_NOTE );
  CHECK ( strstr( steelcheck_trace_text( &r.trace, r.trace.steps[0].text ), "2024" ) != nullptr );
  CHECK_STR_EQ ( r.code, "ABNT NBR 8800" );

  // The steps of the checks follow each other, without gaps or overlaps.

  size_t next = r.checks[STEELCHECK_CHECK_TENSION].trace_first;

  CHECK_SIZE_EQ ( next, 1 );

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    if ( r.checks[k].trace_count > 0 )
    {
      CHECK_SIZE_EQ ( r.checks[k].trace_first, next );
      next = r.checks[k].trace_first + r.checks[k].trace_count;
    }
  }

  CHECK_SIZE_EQ ( next, r.trace.count );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

static void test_invalid_member ()
{
  Fixture_           f         = {};
  SteelCheckMember   m         = {};
  SteelCheckResult   r         = {};
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK ( fixture_( &f ) );
  CHECK ( member_( &f, &m, "W310x38.7", 3000.0 ) );

  m.Cb = 0.0;

  CHECK_INT_EQ ( steelcheck_check_member( steelcheck_code_nbr8800(), &m, &r, &reporter ),
                 STEELCHECK_ERROR_VALUE );
  CHECK_SIZE_EQ ( collector.errors, 1 );
  CHECK_SIZE_EQ ( r.trace.count, 0 );

  CHECK_INT_EQ ( steelcheck_check_member( nullptr, &m, &r, nullptr ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  steelcheck_result_free ( &r );
  steelcheck_database_free ( &f.db );
}

//==================================================================================================
//  test_nbr8800_suite
//==================================================================================================

void test_nbr8800_suite ()
{
  RUN_TEST ( test_code_find );
  RUN_TEST ( test_resistances_w310 );
  RUN_TEST ( test_ltb_regimes );
  RUN_TEST ( test_inelastic_flange );
  RUN_TEST ( test_heavy_column );
  RUN_TEST ( test_slender_web );
  RUN_TEST ( test_cb_from_moments );
  RUN_TEST ( test_cb_free_flange );
  RUN_TEST ( test_applicability );
  RUN_TEST ( test_interaction );
  RUN_TEST ( test_failing_tension );
  RUN_TEST ( test_trace_note );
  RUN_TEST ( test_invalid_member );
}
