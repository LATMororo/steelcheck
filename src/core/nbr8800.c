#include "steelcheck/nbr8800.h"

#include <math.h>
#include <stdio.h>

/*
 *  ABNT NBR 8800 checks of rolled doubly symmetric I sections. Every input, equation, decision and
 *  result goes into the calculation trace, with its clause of the 2024 edition.
 *  Internal units: N, mm, MPa (N mm for moments).
 */

constexpr double gamma_a1_ = steelcheck_nbr8800_gamma_a1;
constexpr double gamma_a2_ = steelcheck_nbr8800_gamma_a2;
constexpr double pi_       = 3.14159265358979323846;

/*
 *  Slenderness limits: tension members (recommended) and compression members.
 */

constexpr double tension_slenderness_max_     = 300.0;
constexpr double compression_slenderness_max_ = 200.0;

/*
 *  Size of the buffers used to build symbols such as "\lambda_{0,x}".
 */

constexpr size_t symbol_size_ = 48;

//==================================================================================================
//  Writing the trace
//==================================================================================================

/*
 *  A calculation in progress. status is "sticky": the first failure to add a step is kept, and
 *  every later step is skipped, so the calculation itself needs no error handling; the check
 *  function returns status at the end.
 */

typedef struct Calc_
{
  SteelCheckTrace* trace;
  SteelCheckStatus status;
} Calc_;

static void add_ ( Calc_* c, const SteelCheckStepInput* step )
{
  if ( c->status == STEELCHECK_OK )
  {
    c->status = steelcheck_trace_add ( c->trace, step );
  }
}

static void heading_ ( Calc_* c, const char* text )
{
  add_ ( c, &(SteelCheckStepInput) { .kind = STEELCHECK_STEP_HEADING, .text = text } );
}

static void note_ ( Calc_* c, SteelCheckStepKind kind, const char* text )
{
  add_ ( c, &(SteelCheckStepInput) { .kind = kind, .text = text } );
}

/*
 *  An input value (of the section, material or member). Returns value.
 */

static double given_ ( Calc_* c, const char* tex, double value, SteelCheckUnit unit,
                       const char* text )
{
  add_ ( c, &(SteelCheckStepInput) { .kind = STEELCHECK_STEP_GIVEN, .symbol_tex = tex,
                                     .value = value, .unit = unit, .text = text } );
  return value;
}

/*
 *  A computed value with its equation and clause. Returns value, so a calculation reads
 *  "double x = equation_( ..., value of x, ... );".
 */

static double equation_ ( Calc_* c, const char* tex, const char* expression, double value,
                          SteelCheckUnit unit, const char* clause, const char* text )
{
  add_ ( c, &(SteelCheckStepInput) { .kind = STEELCHECK_STEP_EQUATION, .symbol_tex = tex,
                                     .expression = expression, .value = value, .unit = unit,
                                     .clause = clause, .text = text } );
  return value;
}

/*
 *  Adds a limit state to a check. A full check (more than steelcheck_limit_states_max states)
 *  would be a programming error; it is reported through the sticky status.
 */

static void state_ ( Calc_* c, SteelCheckCheck* check, SteelCheckLimitResult state )
{
  SteelCheckStatus status = steelcheck_check_add_state ( check, &state );

  if ( status != STEELCHECK_OK && c->status == STEELCHECK_OK )
  {
    c->status = status;
  }
}

/*
 *  The regime of a slenderness: compact, inelastic or elastic, recorded as a decision.
 */

static SteelCheckRegime regime_ ( Calc_* c, double lambda, double lambda_p, double lambda_r )
{
  if ( lambda <= lambda_p )
  {
    note_ ( c, STEELCHECK_STEP_DECISION, "lambda <= lambda_p: compact, plastic resistance" );
    return STEELCHECK_REGIME_COMPACT;
  }

  if ( lambda <= lambda_r )
  {
    note_ ( c, STEELCHECK_STEP_DECISION, "lambda_p < lambda <= lambda_r: inelastic buckling" );
    return STEELCHECK_REGIME_INELASTIC;
  }

  note_ ( c, STEELCHECK_STEP_DECISION, "lambda > lambda_r: elastic buckling" );
  return STEELCHECK_REGIME_ELASTIC;
}

/*
 *  The design force of a check, from the member (only if design forces were given).
 */

static void demand_ ( const SteelCheckMember* m, SteelCheckCheck* check, double force )
{
  check->has_demand = m->has_actions;
  check->demand     = m->has_actions ? fabs( force ) : 0.0;
}

//==================================================================================================
//  Tension (5.2)
//==================================================================================================

static SteelCheckStatus tension_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_TENSION];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Tension" );

  double Ag = given_ ( &c, "A_g", s->A, STEELCHECK_UNIT_MM2, "gross area" );
  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double fu = given_ ( &c, "f_u", m->material->fu, STEELCHECK_UNIT_MPA, "tensile strength" );

  // Yielding of the gross section.

  double yield = equation_ ( &c, "N_{t,Rd}", "A_g f_y / \\gamma_{a1}", Ag * fy / gamma_a1_,
                             STEELCHECK_UNIT_N, "5.2.2 a)", "yielding of the gross section" );

  state_ ( &c, check, (SteelCheckLimitResult) { .state = STEELCHECK_LIMIT_YIELDING,
                                                .nominal = Ag * fy, .design = yield } );

  // Rupture of the net section, if the connection is known.

  if ( m->An > 0.0 )
  {
    double An = given_ ( &c, "A_n", m->An, STEELCHECK_UNIT_MM2, "net area" );
    double Ct = given_ ( &c, "C_t", m->Ct, STEELCHECK_UNIT_NONE, "reduction coefficient" );
    double Ae = equation_ ( &c, "A_e", "C_t A_n", Ct * An, STEELCHECK_UNIT_MM2, "5.2.3",
                            "effective net area" );

    double rupture = equation_ ( &c, "N_{t,Rd}", "A_e f_u / \\gamma_{a2}", Ae * fu / gamma_a2_,
                                 STEELCHECK_UNIT_N, "5.2.2 b)", "rupture of the net section" );

    state_ ( &c, check, (SteelCheckLimitResult) { .state = STEELCHECK_LIMIT_RUPTURE,
                                                  .nominal = Ae * fu, .design = rupture } );
  }
  else
  {
    note_ ( &c, STEELCHECK_STEP_NOTE, "An and Ct not given: net section rupture not checked" );
  }

  // Slenderness (a recommendation for tension members).

  double slenderness = equation_ ( &c, "L / r", "\\max(L_x / r_x, L_y / r_y)",
                                   fmax( m->Lx / s->rx, m->Ly / s->ry ), STEELCHECK_UNIT_NONE,
                                   "5.2.8.1", "largest slenderness" );

  if ( slenderness > tension_slenderness_max_ )
  {
    note_ ( &c, STEELCHECK_STEP_WARNING, "L / r > 300: above the limit recommended for "
            "tension members" );
  }

  demand_ ( m, check, m->NSd );

  return c.status;
}

//==================================================================================================
//  Compression (5.3)
//==================================================================================================

/*
 *  The reduction factor chi of a reduced slenderness lambda_0 (5.3.3.1), recorded as an equation.
 */

static double chi_ ( Calc_* c, const char* tex, double lambda_0 )
{
  if ( lambda_0 <= 1.5 )
  {
    return equation_ ( c, tex, "0.658^{\\lambda_0^2}", pow( 0.658, lambda_0 * lambda_0 ),
                       STEELCHECK_UNIT_NONE, "5.3.3.1", "lambda_0 <= 1.5" );
  }

  return equation_ ( c, tex, "0.877 / \\lambda_0^2", 0.877 / ( lambda_0 * lambda_0 ),
                     STEELCHECK_UNIT_NONE, "5.3.3.1", "lambda_0 > 1.5" );
}

/*
 *  An element of the cross section, for local buckling (5.3.4): a half flange (AL, group 4 of
 *  Table 4) or the web (AA, group 2), with its width b (5.1.2.2.3 and 5.1.2.2.4), thickness t,
 *  limit (b/t)_lim (Table 4) and factors c1 and c2 (Table 5).
 */

typedef struct Element_
{
  const char* id;       // subscript in symbols: "f" or "w"
  const char* b_tex;    // the width, e.g. "b_f / 2"
  double      b;
  double      t;
  double      lim;
  double      c1;
  double      c2;
} Element_;

/*
 *  The effective width of an element for the reduction factor chi of one buckling mode (5.3.4.2).
 *  The formula gives slightly more than b just past the limit (c1 and c2 are rounded): b_ef is
 *  then taken as b.
 */

static double effective_width_ ( Calc_* c, const Element_* e, const char* axis, double chi,
                                 double fy )
{
  char bef_tex[symbol_size_];
  char sel_tex[symbol_size_];

  snprintf ( bef_tex, sizeof bef_tex, "b_{ef,%s,%s}", e->id, axis );
  snprintf ( sel_tex, sizeof sel_tex, "\\sigma_{el,%s}", e->id );

  if ( e->b / e->t <= e->lim / sqrt( chi ) )
  {
    return equation_ ( c, bef_tex, e->b_tex, e->b, STEELCHECK_UNIT_MM, "5.3.4.2",
                       "b/t <= (b/t)_lim / sqrt(chi): fully effective" );
  }

  double ratio = e->c2 * e->lim / ( e->b / e->t );
  double sel   = equation_ ( c, sel_tex, "[c_2 (b/t)_{lim} / (b/t)]^2 f_y", ratio * ratio * fy,
                             STEELCHECK_UNIT_MPA, "5.3.4.2", "local buckling stress" );
  double k     = sqrt ( sel / ( chi * fy ) );
  double bef   = fmin ( e->b * ( 1.0 - e->c1 * k ) * k, e->b );

  return equation_ ( c, bef_tex, "b [1 - c_1 \\sqrt{\\sigma_{el} / (\\chi f_y)}] "
                     "\\sqrt{\\sigma_{el} / (\\chi f_y)} \\le b", bef, STEELCHECK_UNIT_MM,
                     "5.3.4.2", "b/t > (b/t)_lim / sqrt(chi)" );
}

/*
 *  A buckling mode (5.3.5.1): its limit state, axis ("x", "y" or "z") and elastic force N_e.
 */

typedef struct Mode_
{
  SteelCheckLimitState state;
  const char*          axis;
  const char*          ne_tex;
  double               Ne;
} Mode_;

/*
 *  The design resistance for one buckling mode, added as a limit state. slender: some element
 *  has b/t > (b/t)_lim, so the effective area depends on the mode's chi (5.3.4.1).
 */

static void buckling_mode_ ( Calc_* c, SteelCheckCheck* check, const Mode_* mode,
                             const Element_* flange, const Element_* web, bool slender,
                             double Ag, double fy )
{
  char lambda_tex[symbol_size_];
  char lambda_expr[symbol_size_];
  char chi_tex[symbol_size_];
  char aef_tex[symbol_size_];
  char n_tex[symbol_size_];
  char n_expr[symbol_size_];

  snprintf ( lambda_tex,  sizeof lambda_tex,  "\\lambda_{0,%s}", mode->axis );
  snprintf ( lambda_expr, sizeof lambda_expr, "\\sqrt{A_g f_y / %s}", mode->ne_tex );
  snprintf ( chi_tex,     sizeof chi_tex,     "\\chi_%s", mode->axis );
  snprintf ( aef_tex,     sizeof aef_tex,     "A_{ef,%s}", mode->axis );
  snprintf ( n_tex,       sizeof n_tex,       "N_{c,Rd,%s}", mode->axis );
  snprintf ( n_expr,      sizeof n_expr,      "\\chi_%s A_{ef,%s} f_y / \\gamma_{a1}", mode->axis,
             mode->axis );

  double lambda_0 = equation_ ( c, lambda_tex, lambda_expr, sqrt( Ag * fy / mode->Ne ),
                                STEELCHECK_UNIT_NONE, "5.3.3.2", "reduced slenderness" );
  double chi      = chi_ ( c, chi_tex, lambda_0 );
  double Aef      = Ag;

  if ( slender )
  {
    // Four half flanges and the web; the area lost is (b - b_ef) t for each.

    double bf = effective_width_ ( c, flange, mode->axis, chi, fy );
    double bw = effective_width_ ( c, web, mode->axis, chi, fy );

    Aef = equation_ ( c, aef_tex, "A_g - 4 (b_f / 2 - b_{ef,f}) t_f - (d' - b_{ef,w}) t_w",
                      Ag - 4.0 * ( flange->b - bf ) * flange->t - ( web->b - bw ) * web->t,
                      STEELCHECK_UNIT_MM2, "5.3.4.1", "effective area" );
  }

  double design = equation_ ( c, n_tex, n_expr, chi * Aef * fy / gamma_a1_, STEELCHECK_UNIT_N,
                              "5.3.2", steelcheck_limit_state_name( mode->state ) );

  state_ ( c, check, (SteelCheckLimitResult) { .state = mode->state, .lambda = lambda_0,
                                               .nominal = chi * Aef * fy, .design = design } );
}

static SteelCheckStatus compression_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_COMPRESSION];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Compression" );

  double Ag = given_ ( &c, "A_g", s->A, STEELCHECK_UNIT_MM2, "gross area" );
  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double E  = given_ ( &c, "E", m->material->E, STEELCHECK_UNIT_MPA, "modulus of elasticity" );
  double G  = given_ ( &c, "G", m->material->G, STEELCHECK_UNIT_MPA, "shear modulus" );

  // Elastic buckling forces (5.3.5.1): flexural about x and y, torsional (doubly symmetric, so
  // the shear center is the centroid: x0 = y0 = 0).

  note_ ( &c, STEELCHECK_STEP_NOTE, "Elastic buckling (5.3.5.1)" );

  double KLx = m->Kx * m->Lx;
  double KLy = m->Ky * m->Ly;
  double KLz = m->Kz * m->Lz;

  double Nex = equation_ ( &c, "N_{ex}", "\\pi^2 E I_x / (K_x L_x)^2",
                           pi_ * pi_ * E * s->Ix / ( KLx * KLx ), STEELCHECK_UNIT_N, "5.3.5.1 a)",
                           "flexural buckling about x" );
  double Ney = equation_ ( &c, "N_{ey}", "\\pi^2 E I_y / (K_y L_y)^2",
                           pi_ * pi_ * E * s->Iy / ( KLy * KLy ), STEELCHECK_UNIT_N, "5.3.5.1 b)",
                           "flexural buckling about y" );
  double r0  = equation_ ( &c, "r_0", "\\sqrt{r_x^2 + r_y^2 + x_0^2 + y_0^2}", s->r0,
                           STEELCHECK_UNIT_MM, "5.3.5.1 c)", "polar radius of gyration" );
  double Nez = equation_ ( &c, "N_{ez}", "[\\pi^2 E C_w / (K_z L_z)^2 + G J] / r_0^2",
                           ( pi_ * pi_ * E * s->Cw / ( KLz * KLz ) + G * s->J ) / ( r0 * r0 ),
                           STEELCHECK_UNIT_N, "5.3.5.1 c)", "torsional buckling" );

  // Local buckling (5.3.4): half flanges (AL, group 4) and the web (AA, group 2).

  note_ ( &c, STEELCHECK_STEP_NOTE, "Local buckling (5.3.4)" );

  Element_ flange =
  {
    .id = "f", .b_tex = "b_f / 2", .b = s->bf / 2.0, .t = s->tf, .c1 = 0.22, .c2 = 1.49,
  };
  Element_ web =
  {
    .id = "w", .b_tex = "d'", .b = s->dp, .t = s->tw, .c1 = 0.18, .c2 = 1.31,
  };

  double bt_f = equation_ ( &c, "(b/t)_f", "b_f / (2 t_f)", flange.b / flange.t,
                            STEELCHECK_UNIT_NONE, "5.1.2.2.4 a)", "half flange (AL)" );

  flange.lim  = equation_ ( &c, "(b/t)_{lim,f}", "0.56 \\sqrt{E / f_y}", 0.56 * sqrt( E / fy ),
                            STEELCHECK_UNIT_NONE, "Table 4", "group 4: flanges of rolled I" );

  double bt_w = equation_ ( &c, "(b/t)_w", "d' / t_w", web.b / web.t, STEELCHECK_UNIT_NONE,
                            "5.1.2.2.3 a)", "web (AA)" );

  web.lim     = equation_ ( &c, "(b/t)_{lim,w}", "1.49 \\sqrt{E / f_y}", 1.49 * sqrt( E / fy ),
                            STEELCHECK_UNIT_NONE, "Table 4", "group 2: webs of I sections" );

  bool slender = bt_f > flange.lim || bt_w > web.lim;

  if ( slender )
  {
    note_ ( &c, STEELCHECK_STEP_DECISION, "b/t > (b/t)_lim: effective widths for each mode "
            "(c_1, c_2 from Table 5)" );
  }
  else
  {
    note_ ( &c, STEELCHECK_STEP_DECISION, "every b/t <= (b/t)_lim: A_ef = A_g" );
  }

  // Resistance of each buckling mode (5.3.2): the smallest governs.

  note_ ( &c, STEELCHECK_STEP_NOTE, "Resistance (5.3.2)" );

  const Mode_ modes[] =
  {
    { STEELCHECK_LIMIT_FLEXURAL_BUCKLING_X, "x", "N_{ex}", Nex },
    { STEELCHECK_LIMIT_FLEXURAL_BUCKLING_Y, "y", "N_{ey}", Ney },
    { STEELCHECK_LIMIT_TORSIONAL_BUCKLING,  "z", "N_{ez}", Nez },
  };

  for ( size_t i = 0; i < sizeof modes / sizeof modes[0]; i++ )
  {
    buckling_mode_ ( &c, check, &modes[i], &flange, &web, slender, Ag, fy );
  }

  // Slenderness limit recommended for compression members (5.3.7.1).

  double slenderness = equation_ ( &c, "K L / r", "\\max(K_x L_x / r_x, K_y L_y / r_y)",
                                   fmax( KLx / s->rx, KLy / s->ry ), STEELCHECK_UNIT_NONE,
                                   "5.3.7.1", "largest slenderness" );

  if ( slenderness > compression_slenderness_max_ )
  {
    note_ ( &c, STEELCHECK_STEP_WARNING, "K L / r > 200: above the limit recommended for "
            "compression members" );
  }

  demand_ ( m, check, m->NSd );

  return c.status;
}

//==================================================================================================
//  Bending (5.4.2, Annex D)
//==================================================================================================

/*
 *  One limit state of bending, given its regime (D.2.1 for FLT, D.2.2 for FLM and FLA), recorded
 *  in the trace and added to the check. Mcr is used for the elastic regime only. Every M_Rk is
 *  limited to cap = 1.50 W f_y (5.4.2.2).
 *
 *  Unlike the 2008 edition, C_b does not multiply the inelastic interpolation of FLT: it enters
 *  through lambda_r and M_cr (D.2.8 a)).
 */

typedef struct MomentState_
{
  SteelCheckLimitState state;
  SteelCheckRegime     regime;
  const char*          name;       // short name in symbols, e.g. "FLT"
  const char*          clause;     // "D.2.1" or "D.2.2"
  double               lambda;
  double               lambda_p;
  double               lambda_r;
  double               Mpl;
  double               Mr;
  double               Mcr;
  double               cap;
  bool                 has_demand;   // its own design moment, instead of the check's
  double               demand;
} MomentState_;

static void moment_state_ ( Calc_* c, SteelCheckCheck* check, const MomentState_* ms )
{
  char rk[symbol_size_];
  char rd[symbol_size_];

  snprintf ( rk, sizeof rk, "M_{Rk,%s}", ms->name );
  snprintf ( rd, sizeof rd, "M_{Rd,%s}", ms->name );

  double Mrk = ms->Mpl;

  switch ( ms->regime )
  {
    case STEELCHECK_REGIME_NONE:
    case STEELCHECK_REGIME_COMPACT:
      Mrk = equation_ ( c, rk, "M_{pl}", ms->Mpl, STEELCHECK_UNIT_NMM, ms->clause,
                        "plastic moment" );
      break;

    case STEELCHECK_REGIME_INELASTIC:
    {
      double ratio = ( ms->lambda - ms->lambda_p ) / ( ms->lambda_r - ms->lambda_p );

      Mrk = equation_ ( c, rk, "M_{pl} - (M_{pl} - M_r) \\frac{\\lambda - \\lambda_p}"
                        "{\\lambda_r - \\lambda_p}", ms->Mpl - ( ms->Mpl - ms->Mr ) * ratio,
                        STEELCHECK_UNIT_NMM, ms->clause, "inelastic buckling" );
      break;
    }

    case STEELCHECK_REGIME_ELASTIC:
      Mrk = equation_ ( c, rk, "M_{cr}", ms->Mcr, STEELCHECK_UNIT_NMM, ms->clause,
                        "elastic buckling" );
      break;
  }

  double design = equation_ ( c, rd, "\\min(M_{Rk}, 1.50 W f_y) / \\gamma_{a1}",
                              fmin( Mrk, ms->cap ) / gamma_a1_, STEELCHECK_UNIT_NMM, "5.4.2.2",
                              steelcheck_limit_state_name( ms->state ) );

  state_ ( c, check, (SteelCheckLimitResult) { .state = ms->state, .regime = ms->regime,
                                               .lambda = ms->lambda, .lambda_p = ms->lambda_p,
                                               .lambda_r = ms->lambda_r,
                                               .nominal = fmin( Mrk, ms->cap ),
                                               .design = design, .has_demand = ms->has_demand,
                                               .demand = ms->demand } );
}

/*
 *  Flange local buckling (FLM) of a rolled I section about either axis (Table D.1, D.2.8 f)),
 *  with W the elastic modulus about that axis.
 */

static void flange_buckling_ ( Calc_* c, SteelCheckCheck* check, const SteelCheckSection* s,
                               double W, double Mpl, double Mr, double cap, double fy, double sr,
                               double E )
{
  note_ ( c, STEELCHECK_STEP_NOTE, "Flange local buckling (FLM)" );

  double lambda = equation_ ( c, "\\lambda", "b_f / (2 t_f)", s->bf / ( 2.0 * s->tf ),
                              STEELCHECK_UNIT_NONE, "D.2.8 h)", "slenderness of the flange" );
  double lp     = equation_ ( c, "\\lambda_p", "0.38 \\sqrt{E / f_y}", 0.38 * sqrt( E / fy ),
                              STEELCHECK_UNIT_NONE, "Table D.1", "" );
  double lr     = equation_ ( c, "\\lambda_r", "0.83 \\sqrt{E / (f_y - \\sigma_r)}",
                              0.83 * sqrt( E / ( fy - sr ) ), STEELCHECK_UNIT_NONE, "D.2.8 f)",
                              "rolled section" );

  SteelCheckRegime regime = regime_ ( c, lambda, lp, lr );
  double           Mcr    = 0.0;

  if ( regime == STEELCHECK_REGIME_ELASTIC )
  {
    Mcr = equation_ ( c, "M_{cr}", "0.69 E W / \\lambda^2", 0.69 * E * W / ( lambda * lambda ),
                      STEELCHECK_UNIT_NMM, "D.2.8 f)", "rolled section" );
  }

  moment_state_ ( c, check, &(MomentState_) {
                    .state = STEELCHECK_LIMIT_FLB, .regime = regime, .name = "FLM",
                    .clause = "D.2.2", .lambda = lambda, .lambda_p = lp, .lambda_r = lr,
                    .Mpl = Mpl, .Mr = Mr, .Mcr = Mcr, .cap = cap } );
}

/*
 *  The moment gradient factor Cb from the moment diagram over Lb (5.4.2.3 a)), with every moment
 *  taken in absolute value, and R_m = 1.0 for doubly symmetric sections. Without Mmax, the
 *  largest of MA, MB, MC and MxSd is taken.
 */

static double moment_gradient_ ( Calc_* c, const SteelCheckMember* m )
{
  double MA = given_ ( c, "M_A", fabs( m->MA ), STEELCHECK_UNIT_NMM, "moment at L_b / 4" );
  double MB = given_ ( c, "M_B", fabs( m->MB ), STEELCHECK_UNIT_NMM, "moment at L_b / 2" );
  double MC = given_ ( c, "M_C", fabs( m->MC ), STEELCHECK_UNIT_NMM, "moment at 3 L_b / 4" );

  double Mmax = m->Mmax != 0.0
              ? given_( c, "M_{max}", fabs( m->Mmax ), STEELCHECK_UNIT_NMM,
                        "largest moment in L_b" )
              : equation_( c, "M_{max}", "\\max(|M_{x,Sd}|, M_A, M_B, M_C)",
                           fmax( fabs( m->MxSd ), fmax( MA, fmax( MB, MC ) ) ),
                           STEELCHECK_UNIT_NMM, "5.4.2.3 a)", "largest moment in L_b" );

  double Rm = given_ ( c, "R_m", 1.0, STEELCHECK_UNIT_NONE, "doubly symmetric section" );

  return equation_ ( c, "C_b", "\\frac{12.5 M_{max}}{2.5 M_{max} + 3 M_A + 4 M_B + 3 M_C} R_m",
                     12.5 * Mmax / ( 2.5 * Mmax + 3.0 * MA + 4.0 * MB + 3.0 * MC ) * Rm,
                     STEELCHECK_UNIT_NONE, "5.4.2.3 a)", "moment gradient factor" );
}

/*
 *  Cb of a member with one flange continuously restrained and the other free (5.4.2.4 a)). The
 *  signs are those of the member: negative compresses the free flange. M2 counts with its sign in
 *  the second term; in the third term only if it compresses the free flange (else as zero).
 */

static double free_flange_gradient_ ( Calc_* c, const SteelCheckMember* m )
{
  note_ ( c, STEELCHECK_STEP_NOTE, "One flange continuously restrained (5.4.2.4 a)); "
          "negative moments compress the free flange" );

  double M1  = given_ ( c, "M_1", m->M1, STEELCHECK_UNIT_NMM,
                        "largest end moment compressing the free flange" );
  double M2  = given_ ( c, "M_2", m->M2, STEELCHECK_UNIT_NMM, "moment at the other end" );
  double MCL = given_ ( c, "M_{CL}", m->MCL, STEELCHECK_UNIT_NMM, "moment at the middle of L_b" );

  // M2 that tensions the free flange is taken as zero in the third term.

  double M2_3 = M2 < 0.0 ? M2 : 0.0;

  if ( M2 > 0.0 )
  {
    note_ ( c, STEELCHECK_STEP_DECISION, "M_2 tensions the free flange: zero in the third term" );
  }

  return equation_ ( c, "C_b", "3.00 - \\frac{2}{3} \\frac{M_2}{M_1} - \\frac{8}{3} "
                     "\\frac{M_{CL}}{M_1 + M_2}",
                     3.00 - 2.0 / 3.0 * M2 / M1 - 8.0 / 3.0 * MCL / ( M1 + M2_3 ),
                     STEELCHECK_UNIT_NONE, "5.4.2.4 a)", "moment gradient factor" );
}

static SteelCheckStatus bending_x_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_BENDING_X];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Bending about x" );

  double Zx = given_ ( &c, "Z_x", s->Zx, STEELCHECK_UNIT_MM3, "plastic section modulus" );
  double Wx = given_ ( &c, "W_x", s->Wx, STEELCHECK_UNIT_MM3, "elastic section modulus" );
  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double E  = given_ ( &c, "E", m->material->E, STEELCHECK_UNIT_MPA, "modulus of elasticity" );

  double Mpl = equation_ ( &c, "M_{pl}", "Z_x f_y", Zx * fy, STEELCHECK_UNIT_NMM, "D.3",
                           "plastic moment" );
  double cap = equation_ ( &c, "M_{Rk,max}", "1.50 W_x f_y", 1.50 * Wx * fy, STEELCHECK_UNIT_NMM,
                           "5.4.2.2", "upper limit of M_Rk" );
  double sr  = equation_ ( &c, "\\sigma_r", "0.30 f_y", 0.30 * fy, STEELCHECK_UNIT_MPA, "D.2.8 e)",
                           "residual stress" );
  double Mr  = equation_ ( &c, "M_r", "(f_y - \\sigma_r) W_x", ( fy - sr ) * Wx,
                           STEELCHECK_UNIT_NMM, "Table D.1",
                           "moment at the start of yielding (FLT, FLM)" );

  // Lateral-torsional buckling (FLT).

  note_ ( &c, STEELCHECK_STEP_NOTE, "Lateral-torsional buckling (FLT)" );

  if ( m->Lb > 0.0 )
  {
    double Lb = given_ ( &c, "L_b", m->Lb, STEELCHECK_UNIT_MM, "unbraced length" );
    double Cb = 0.0;

    if ( m->has_moments )
    {
      Cb = moment_gradient_ ( &c, m );
    }
    else if ( m->has_free_flange )
    {
      Cb = free_flange_gradient_ ( &c, m );
    }
    else
    {
      Cb = given_ ( &c, "C_b", m->Cb, STEELCHECK_UNIT_NONE, "moment gradient factor" );
    }

    // 5.4.2.4 a) gives Cb <= 0 only when its conditions do not hold (for example, when the
    // middle moment compresses the free flange more than the end moments): Cb must then come
    // from an analysis (5.4.2.5), and the check cannot be done.

    if ( !( Cb > 0.0 ) )
    {
      note_ ( &c, STEELCHECK_STEP_WARNING, "C_b <= 0: 5.4.2.4 a) does not apply to these moments; "
              "give C_b from an analysis (5.4.2.5)" );

      check->status = STEELCHECK_CHECK_NOT_CHECKED;

      return c.status;
    }

    double lambda = equation_ ( &c, "\\lambda", "L_b / r_y", Lb / s->ry, STEELCHECK_UNIT_NONE,
                                "Table D.1", "slenderness" );
    double lp     = equation_ ( &c, "\\lambda_p", "1.76 \\sqrt{E / f_y}", 1.76 * sqrt( E / fy ),
                                STEELCHECK_UNIT_NONE, "Table D.1", "" );
    double beta1  = ( fy - sr ) * Wx / ( E * s->J );

    beta1 = equation_ ( &c, "\\beta_1", "(f_y - \\sigma_r) W_x / (E J)", beta1,
                        STEELCHECK_UNIT_NONE, "D.2.8 a)", "[1/mm]" );

    double lr = 1.38 * Cb * sqrt( s->Iy * s->J ) / ( s->ry * s->J * beta1 )
              * sqrt( 1.0 + sqrt( 1.0 + 27.0 * s->Cw * beta1 * beta1 / ( Cb * Cb * s->Iy ) ) );

    lr = equation_ ( &c, "\\lambda_r", "\\frac{1.38 C_b \\sqrt{I_y J}}{r_y J \\beta_1} "
                     "\\sqrt{1 + \\sqrt{1 + \\frac{27 C_w \\beta_1^2}{C_b^2 I_y}}}", lr,
                     STEELCHECK_UNIT_NONE, "D.2.8 a)", "" );

    SteelCheckRegime regime = regime_ ( &c, lambda, lp, lr );
    double           Mcr    = 0.0;

    if ( regime == STEELCHECK_REGIME_ELASTIC )
    {
      Mcr = Cb * pi_ * pi_ * E * s->Iy / ( Lb * Lb )
          * sqrt( s->Cw / s->Iy * ( 1.0 + 0.039 * s->J * Lb * Lb / s->Cw ) );

      Mcr = equation_ ( &c, "M_{cr}", "\\frac{C_b \\pi^2 E I_y}{L_b^2} "
                        "\\sqrt{\\frac{C_w}{I_y} (1 + 0.039 J L_b^2 / C_w)}", Mcr,
                        STEELCHECK_UNIT_NMM, "D.2.8 a)", "elastic critical moment" );
    }

    // With one flange restrained, FLT is checked against the largest moment that compresses the
    // free flange, M1 (5.4.2.4); the other limit states against MxSd.

    bool   own_demand = m->has_free_flange && m->has_actions;
    double demand     = 0.0;

    if ( own_demand )
    {
      demand = equation_ ( &c, "M_{Sd,FLT}", "|M_1|", fabs( m->M1 ), STEELCHECK_UNIT_NMM,
                           "5.4.2.4", "design moment of FLT: compresses the free flange" );
    }

    moment_state_ ( &c, check, &(MomentState_) {
                      .state = STEELCHECK_LIMIT_LTB, .regime = regime, .name = "FLT",
                      .clause = "D.2.1", .lambda = lambda, .lambda_p = lp, .lambda_r = lr,
                      .Mpl = Mpl, .Mr = Mr, .Mcr = Mcr, .cap = cap, .has_demand = own_demand,
                      .demand = demand } );
  }
  else
  {
    note_ ( &c, STEELCHECK_STEP_DECISION, "Lb = 0: compression flange braced, no FLT" );
  }

  // Flange local buckling (FLM), rolled section.

  flange_buckling_ ( &c, check, s, Wx, Mpl, Mr, cap, fy, sr, E );

  // Web local buckling (FLA).

  note_ ( &c, STEELCHECK_STEP_NOTE, "Web local buckling (FLA)" );

  double lambda_w = equation_ ( &c, "\\lambda", "h / t_w = d' / t_w", s->dp / s->tw,
                                STEELCHECK_UNIT_NONE, "Table D.1", "slenderness of the web" );
  double lp_w     = equation_ ( &c, "\\lambda_p", "3.76 \\sqrt{E / f_y}", 3.76 * sqrt( E / fy ),
                                STEELCHECK_UNIT_NONE, "Table D.1", "" );
  double lr_w     = equation_ ( &c, "\\lambda_r", "5.70 \\sqrt{E / f_y}", 5.70 * sqrt( E / fy ),
                                STEELCHECK_UNIT_NONE, "Table D.1", "" );

  SteelCheckRegime regime_w = regime_ ( &c, lambda_w, lp_w, lr_w );

  if ( regime_w == STEELCHECK_REGIME_ELASTIC )
  {
    // A slender web makes the beam one of Annex E (beams with slender webs), which is not
    // implemented. Without its resistance the check would be unsafe, so it is not done at all.

    note_ ( &c, STEELCHECK_STEP_WARNING, "slender web: beam of Annex E, not implemented" );

    check->state_count = 0;
    check->status      = STEELCHECK_CHECK_NOT_IMPLEMENTED;

    return c.status;
  }

  double Mr_w = equation_ ( &c, "M_r", "f_y W_x", fy * Wx, STEELCHECK_UNIT_NMM, "Table D.1",
                            "moment at the start of yielding (FLA)" );

  moment_state_ ( &c, check, &(MomentState_) {
                    .state = STEELCHECK_LIMIT_WLB, .regime = regime_w, .name = "FLA",
                    .clause = "D.2.2", .lambda = lambda_w, .lambda_p = lp_w, .lambda_r = lr_w,
                    .Mpl = Mpl, .Mr = Mr_w, .cap = cap } );

  demand_ ( m, check, m->MxSd );

  return c.status;
}

static SteelCheckStatus bending_y_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_BENDING_Y];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Bending about y" );

  double Zy = given_ ( &c, "Z_y", s->Zy, STEELCHECK_UNIT_MM3, "plastic section modulus" );
  double Wy = given_ ( &c, "W_y", s->Wy, STEELCHECK_UNIT_MM3, "elastic section modulus" );
  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double E  = given_ ( &c, "E", m->material->E, STEELCHECK_UNIT_MPA, "modulus of elasticity" );

  double Mpl = equation_ ( &c, "M_{pl}", "Z_y f_y", Zy * fy, STEELCHECK_UNIT_NMM, "D.3",
                           "plastic moment" );
  double cap = equation_ ( &c, "M_{Rk,max}", "1.50 W_y f_y", 1.50 * Wy * fy, STEELCHECK_UNIT_NMM,
                           "5.4.2.2", "upper limit of M_Rk" );
  double sr  = equation_ ( &c, "\\sigma_r", "0.30 f_y", 0.30 * fy, STEELCHECK_UNIT_MPA, "D.2.8 e)",
                           "residual stress" );
  double Mr  = equation_ ( &c, "M_r", "(f_y - \\sigma_r) W_y", ( fy - sr ) * Wy,
                           STEELCHECK_UNIT_NMM, "Table D.1", "moment at the start of yielding" );

  // Of the limit states of Table D.1, only FLM applies: FLT only about the major axis
  // (D.2.8 g)), FLA only to U sections (D.2.8 c)).

  note_ ( &c, STEELCHECK_STEP_NOTE, "Only FLM applies: FLT and FLA do not (D.2.8 c), g))" );

  flange_buckling_ ( &c, check, s, Wy, Mpl, Mr, cap, fy, sr, E );

  demand_ ( m, check, m->MySd );

  return c.status;
}

//==================================================================================================
//  Shear (5.4.3)
//==================================================================================================

/*
 *  The shear resistance of a web (or of the flanges), given its area, slenderness and kv.
 */

static void shear_resistance_ ( Calc_* c, SteelCheckCheck* check, double Aw, double lambda,
                                double kv, double fy, double E )
{
  double lp  = equation_ ( c, "\\lambda_p", "1.10 \\sqrt{k_v E / f_y}", 1.10 * sqrt( kv * E / fy ),
                           STEELCHECK_UNIT_NONE, "5.4.3.1.1", "" );
  double lr  = equation_ ( c, "\\lambda_r", "1.37 \\sqrt{k_v E / f_y}", 1.37 * sqrt( kv * E / fy ),
                           STEELCHECK_UNIT_NONE, "5.4.3.1.1", "" );
  double Vpl = equation_ ( c, "V_{pl}", "0.60 A_w f_y", 0.60 * Aw * fy, STEELCHECK_UNIT_N,
                           "5.4.3.1.2", "plastic shear force" );

  SteelCheckRegime regime = regime_ ( c, lambda, lp, lr );
  double           Vrk    = Vpl;

  if ( regime == STEELCHECK_REGIME_INELASTIC )
  {
    Vrk = equation_ ( c, "V_{Rk}", "(\\lambda_p / \\lambda) V_{pl}", lp / lambda * Vpl,
                      STEELCHECK_UNIT_N, "5.4.3.1.1", "inelastic shear buckling" );
  }
  else if ( regime == STEELCHECK_REGIME_ELASTIC )
  {
    Vrk = equation_ ( c, "V_{Rk}", "1.24 (\\lambda_p / \\lambda)^2 V_{pl}",
                      1.24 * ( lp / lambda ) * ( lp / lambda ) * Vpl, STEELCHECK_UNIT_N,
                      "5.4.3.1.1", "elastic shear buckling" );
  }

  double design = equation_ ( c, "V_{Rd}", "V_{Rk} / \\gamma_{a1}", Vrk / gamma_a1_,
                              STEELCHECK_UNIT_N, "5.4.3.1.1", "shear resistance" );

  state_ ( c, check, (SteelCheckLimitResult) { .state = STEELCHECK_LIMIT_SHEAR, .regime = regime,
                                               .lambda = lambda, .lambda_p = lp, .lambda_r = lr,
                                               .nominal = Vrk, .design = design } );
}

static SteelCheckStatus shear_y_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_SHEAR_Y];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Shear along y (web)" );

  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double E  = given_ ( &c, "E", m->material->E, STEELCHECK_UNIT_MPA, "modulus of elasticity" );
  double Aw = equation_ ( &c, "A_w", "d t_w", s->d * s->tw, STEELCHECK_UNIT_MM2, "5.4.3.1.2",
                          "shear area of the web" );
  double lambda = equation_ ( &c, "\\lambda", "h / t_w = d' / t_w", s->dp / s->tw,
                              STEELCHECK_UNIT_NONE, "5.4.3.1.1", "slenderness of the web" );
  double kv = equation_ ( &c, "k_v", "5.34", 5.34, STEELCHECK_UNIT_NONE, "5.4.3.1.1",
                          "web without transverse stiffeners" );

  shear_resistance_ ( &c, check, Aw, lambda, kv, fy, E );
  demand_ ( m, check, m->VySd );

  return c.status;
}

static SteelCheckStatus shear_x_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_                    c     = { .trace = &r->trace };
  SteelCheckCheck*         check = &r->checks[STEELCHECK_CHECK_SHEAR_X];
  const SteelCheckSection* s     = m->section;

  heading_ ( &c, "Shear along x (flanges)" );

  double fy = given_ ( &c, "f_y", m->material->fy, STEELCHECK_UNIT_MPA, "yield strength" );
  double E  = given_ ( &c, "E", m->material->E, STEELCHECK_UNIT_MPA, "modulus of elasticity" );
  double Aw = equation_ ( &c, "A_w", "2 b_f t_f", 2.0 * s->bf * s->tf, STEELCHECK_UNIT_MM2,
                          "5.4.3.5", "shear area of the flanges" );
  double lambda = equation_ ( &c, "\\lambda", "b_f / (2 t_f)", s->bf / ( 2.0 * s->tf ),
                              STEELCHECK_UNIT_NONE, "5.4.3.5", "slenderness of the flange" );
  double kv = equation_ ( &c, "k_v", "1.2", 1.2, STEELCHECK_UNIT_NONE, "5.4.3.5", "flanges" );

  shear_resistance_ ( &c, check, Aw, lambda, kv, fy, E );
  demand_ ( m, check, m->VxSd );

  return c.status;
}

//==================================================================================================
//  Axial force and bending (5.5.1.2)
//==================================================================================================

/*
 *  The design resistance of a finished check, or 0 if it has none (not done, not implemented).
 */

static double resistance_of_ ( const SteelCheckCheck* check )
{
  bool done = check->status == STEELCHECK_CHECK_OK || check->status == STEELCHECK_CHECK_FAIL
           || check->status == STEELCHECK_CHECK_RESISTANCE;

  return done ? check->resistance : 0.0;
}

static SteelCheckStatus interaction_ ( const SteelCheckMember* m, SteelCheckResult* r )
{
  Calc_            c     = { .trace = &r->trace };
  SteelCheckCheck* check = &r->checks[STEELCHECK_CHECK_INTERACTION];

  heading_ ( &c, "Axial force and bending" );

  // The resistances come from the other checks, which are already finished.

  const SteelCheckCheck* axial = &r->checks[ m->NSd > 0.0 ? STEELCHECK_CHECK_TENSION
                                                          : STEELCHECK_CHECK_COMPRESSION ];

  double NRd  = m->NSd != 0.0  ? resistance_of_( axial ) : 1.0;
  double MxRd = m->MxSd != 0.0 ? resistance_of_( &r->checks[STEELCHECK_CHECK_BENDING_X] ) : 1.0;
  double MyRd = m->MySd != 0.0 ? resistance_of_( &r->checks[STEELCHECK_CHECK_BENDING_Y] ) : 1.0;

  if ( NRd <= 0.0 || MxRd <= 0.0 || MyRd <= 0.0 )
  {
    note_ ( &c, STEELCHECK_STEP_WARNING, "a resistance is missing: interaction not checked" );
    check->status = STEELCHECK_CHECK_NOT_CHECKED;
    return c.status;
  }

  double NSd  = given_ ( &c, "N_{Sd}", fabs( m->NSd ), STEELCHECK_UNIT_N, "axial force" );
  double MxSd = given_ ( &c, "M_{x,Sd}", fabs( m->MxSd ), STEELCHECK_UNIT_NMM, "moment about x" );
  double MySd = given_ ( &c, "M_{y,Sd}", fabs( m->MySd ), STEELCHECK_UNIT_NMM, "moment about y" );

  note_ ( &c, STEELCHECK_STEP_NOTE, "Design moments must include second-order effects" );

  double n  = m->NSd != 0.0 ? NSd / given_( &c, "N_{Rd}", NRd, STEELCHECK_UNIT_N, "" ) : 0.0;
  double mx = m->MxSd != 0.0 ? MxSd / given_( &c, "M_{x,Rd}", MxRd, STEELCHECK_UNIT_NMM, "" ) : 0.0;
  double my = m->MySd != 0.0 ? MySd / given_( &c, "M_{y,Rd}", MyRd, STEELCHECK_UNIT_NMM, "" ) : 0.0;

  n = equation_ ( &c, "n", "N_{Sd} / N_{Rd}", n, STEELCHECK_UNIT_NONE, "5.5.1.2", "" );

  double value = 0.0;

  if ( n >= 0.2 )
  {
    value = equation_ ( &c, "\\eta", "\\frac{N_{Sd}}{N_{Rd}} + \\frac{8}{9} \\left( "
                        "\\frac{M_{x,Sd}}{M_{x,Rd}} + \\frac{M_{y,Sd}}{M_{y,Rd}} \\right) \\le 1",
                        n + 8.0 / 9.0 * ( mx + my ), STEELCHECK_UNIT_NONE, "5.5.1.2 a)",
                        "n >= 0.2" );
  }
  else
  {
    value = equation_ ( &c, "\\eta", "\\frac{N_{Sd}}{2 N_{Rd}} + \\frac{M_{x,Sd}}{M_{x,Rd}} + "
                        "\\frac{M_{y,Sd}}{M_{y,Rd}} \\le 1", n / 2.0 + mx + my,
                        STEELCHECK_UNIT_NONE, "5.5.1.2 b)", "n < 0.2" );
  }

  // The interaction is a ratio: resistance 1, demand the value of the equation.

  state_ ( &c, check, (SteelCheckLimitResult) { .state = STEELCHECK_LIMIT_INTERACTION,
                                                .nominal = 1.0, .design = 1.0 } );

  check->has_demand = true;
  check->demand     = value;

  return c.status;
}

//==================================================================================================
//  The code table
//==================================================================================================

static const SteelCheckCode nbr8800_ =
{
  .id     = "NBR 8800",
  .name   = "ABNT NBR 8800",
  .note   = "Formulas and clause numbers of ABNT NBR 8800:2024 "
            "(gamma_a1 = 1.10, gamma_a2 = 1.35).",
  .checks =
  {
    [STEELCHECK_CHECK_TENSION]     = tension_,
    [STEELCHECK_CHECK_COMPRESSION] = compression_,
    [STEELCHECK_CHECK_BENDING_X]   = bending_x_,
    [STEELCHECK_CHECK_BENDING_Y]   = bending_y_,
    [STEELCHECK_CHECK_SHEAR_Y]     = shear_y_,
    [STEELCHECK_CHECK_SHEAR_X]     = shear_x_,
    [STEELCHECK_CHECK_INTERACTION] = interaction_,
  },
};

//==================================================================================================
//  steelcheck_code_nbr8800
//==================================================================================================

const SteelCheckCode* steelcheck_code_nbr8800 ()
{
  return &nbr8800_;
}
