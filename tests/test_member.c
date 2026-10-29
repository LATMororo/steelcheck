#include "steelcheck/compat.h"
#include "steelcheck/database.h"
#include "steelcheck/material.h"
#include "steelcheck/member.h"
#include "suites.h"
#include "test.h"

#include <math.h>
#include <string.h>

//==================================================================================================
//  Materials
//==================================================================================================

static void test_material_table ()
{
  size_t                    count     = 0;
  const SteelCheckMaterial* materials = steelcheck_materials ( &count );

  CHECK_SIZE_EQ ( count, 8 );

  // Every grade is plausible: fy < fu, the standard moduli, a name that finds itself.

  for ( size_t i = 0; i < count; i++ )
  {
    const SteelCheckMaterial* m = &materials[i];

    CHECK ( m->fy > 0.0 && m->fu > m->fy );
    CHECK_NEAR ( m->E, 200000.0, 0.0 );
    CHECK_NEAR ( m->G, 77000.0, 0.0 );
    CHECK ( steelcheck_material_find( m->name ) == m );
  }
}

static void test_material_find ()
{
  const SteelCheckMaterial* a572 = steelcheck_material_find ( "ASTM A572 Gr.50" );

  CHECK ( a572 != nullptr );

  if ( a572 != nullptr )
  {
    CHECK_NEAR ( a572->fy, 345.0, 0.0 );
    CHECK_NEAR ( a572->fu, 450.0, 0.0 );
  }

  // Case, spaces, '.', '-' and '_' do not matter, and the standard may be left out.

  CHECK ( steelcheck_material_find( "a572 gr50" ) == a572 );
  CHECK ( steelcheck_material_find( "A572-GR-50" ) == a572 );
  CHECK ( steelcheck_material_find( "astm_a572_gr.50" ) == a572 );
  CHECK ( steelcheck_material_find( "mr 250" ) == steelcheck_material_find( "NBR 7007 MR250" ) );
  CHECK ( steelcheck_material_find( "AR350COR" ) != nullptr );

  // Not a grade of the table, or not a name at all.

  CHECK ( steelcheck_material_find( "A572" ) == nullptr );
  CHECK ( steelcheck_material_find( "S355" ) == nullptr );
  CHECK ( steelcheck_material_find( "" ) == nullptr );
  CHECK ( steelcheck_material_find( nullptr ) == nullptr );
}

static void test_material_make ()
{
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );
  SteelCheckMaterial m         = {};

  CHECK_INT_EQ ( steelcheck_material_make( &m, "S355", 355.0, 490.0, &reporter ), STEELCHECK_OK );
  CHECK_STR_EQ ( m.name, "S355" );
  CHECK_NEAR ( m.fy, 355.0, 0.0 );
  CHECK_NEAR ( m.E, steelcheck_steel_E, 0.0 );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // fu below fy, then a negative fy and NaN fu: every problem is reported, m is zeroed.

  CHECK_INT_EQ ( steelcheck_material_make( &m, "weak", 355.0, 300.0, &reporter ),
                 STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "material 'weak': fu 300 MPa is less than fy 355 MPa" ) );
  CHECK_STR_EQ ( m.name, "" );

  collector = (TestCollector) {};

  CHECK_INT_EQ ( steelcheck_material_make( &m, "bad", -1.0, (double) NAN, &reporter ),
                 STEELCHECK_ERROR_VALUE );
  CHECK_SIZE_EQ ( collector.errors, 2 );

  CHECK_INT_EQ ( steelcheck_material_make( &m, "a name that is far too long for it", 250.0,
                                           400.0, &reporter ),
                 STEELCHECK_ERROR_VALUE );
  CHECK_INT_EQ ( steelcheck_material_make( nullptr, "x", 250.0, 400.0, &reporter ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
}

//==================================================================================================
//  Members
//==================================================================================================

/*
 *  A W310x38.7 of A572 Gr.50, 6 m long, from the built-in database or the file.
 */

typedef struct Fixture_
{
  SteelCheckDatabase        db;
  const SteelCheckSection*  section;
  const SteelCheckMaterial* material;
} Fixture_;

static bool fixture_ ( Fixture_* f )
{
  *f = (Fixture_) {};

  if ( steelcheck_database_load_file( &f->db, "data/gerdau_w_hp.csv", nullptr ) != STEELCHECK_OK )
  {
    return false;
  }

  f->section  = steelcheck_database_find ( &f->db, "W310x38.7" );
  f->material = steelcheck_material_find ( "A572 Gr.50" );

  return f->section != nullptr && f->material != nullptr;
}

static void test_member_init ()
{
  Fixture_         f = {};
  SteelCheckMember m = {};

  CHECK ( fixture_( &f ) );
  CHECK_INT_EQ ( steelcheck_member_init( &m, "B1", f.section, f.material, 6000.0 ), STEELCHECK_OK );

  // Defaults: every length = L, every K = 1, Cb = 1, no net section, no forces.

  CHECK_STR_EQ ( m.label, "B1" );
  CHECK ( m.section == f.section && m.material == f.material );
  CHECK_NEAR ( m.Lx, 6000.0, 0.0 );
  CHECK_NEAR ( m.Ly, 6000.0, 0.0 );
  CHECK_NEAR ( m.Lz, 6000.0, 0.0 );
  CHECK_NEAR ( m.Lb, 6000.0, 0.0 );
  CHECK_NEAR ( m.Kx * m.Ky * m.Kz, 1.0, 0.0 );
  CHECK_NEAR ( m.Cb, 1.0, 0.0 );
  CHECK ( m.An == 0.0 && m.Ct == 0.0 && !m.has_actions );

  CHECK_INT_EQ ( steelcheck_member_validate( &m, nullptr ), STEELCHECK_OK );

  // A label that does not fit, and nullptr.

  CHECK_INT_EQ ( steelcheck_member_init( &m, "a label that is far too long for it", f.section,
                                         f.material, 6000.0 ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );
  CHECK_INT_EQ ( steelcheck_member_init( nullptr, "B1", f.section, f.material, 6000.0 ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  steelcheck_database_free ( &f.db );
}

static void test_member_validate ()
{
  Fixture_           f         = {};
  SteelCheckMember   m         = {};
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK ( fixture_( &f ) );
  CHECK_INT_EQ ( steelcheck_member_init( &m, "C1", f.section, f.material, 3000.0 ), STEELCHECK_OK );

  // A braced compression flange (Lb = 0) and a net section are fine.

  m.Lb = 0.0;
  m.An = 0.85 * f.section->A;
  m.Ct = 0.9;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );

  // Six problems at once, all reported in order.

  m.Ky = 0.0;
  m.Lz = -1.0;
  m.Lb = -1.0;
  m.Cb = -1.0;
  m.An = 2.0 * f.section->A;
  m.Ct = 1.2;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK_SIZE_EQ ( collector.errors, 6 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "member 'C1': Lz must be positive, got -1" ) );
  CHECK ( test_diagnostic_is( &collector, 1, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "member 'C1': Ky must be positive, got 0" ) );
  CHECK ( test_diagnostic_is( &collector, 2, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "Lb must not be negative, got -1" ) );
  CHECK ( test_diagnostic_is( &collector, 3, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "Cb must be positive, got -1" ) );
  CHECK ( test_diagnostic_is( &collector, 4, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "An must be positive and at most A = 4970 mm^2, got 9940" ) );
  CHECK ( test_diagnostic_is( &collector, 5, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "Ct must be greater than 0 and at most 1, got 1.2" ) );

  // No section and no material; An without Ct.

  collector = (TestCollector) {};

  CHECK_INT_EQ ( steelcheck_member_init( &m, "", nullptr, nullptr, 3000.0 ), STEELCHECK_OK );

  m.An = 1000.0;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK_SIZE_EQ ( collector.errors, 3 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "member '': no section" ) );
  CHECK ( test_diagnostic_is( &collector, 2, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "An and Ct must be given together (An = 1000, Ct = 0)" ) );

  CHECK_INT_EQ ( steelcheck_member_validate( nullptr, &reporter ),
                 STEELCHECK_ERROR_INVALID_ARGUMENT );

  steelcheck_database_free ( &f.db );
}

/*
 *  The moment diagram for Cb (has_moments).
 */

static void test_member_moments ()
{
  Fixture_           f         = {};
  SteelCheckMember   m         = {};
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK ( fixture_( &f ) );
  CHECK_INT_EQ ( steelcheck_member_init( &m, "B1", f.section, f.material, 6000.0 ), STEELCHECK_OK );
  CHECK ( !m.has_moments );

  // A valid diagram; signs do not matter; Mmax 0 means "the largest one".

  m.has_moments = true;
  m.MA          = -75e6;
  m.MB          = 100e6;
  m.MC          = 75e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );

  m.Mmax = -100e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // Mmax smaller than MB.

  m.Mmax = 90e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "member 'B1': Mmax 9e+07 N mm is smaller than one of MA, MB, MC" ) );

  // All zeros, and no design moment either.

  collector = (TestCollector) {};
  m.Mmax    = 0.0;
  m.MA      = 0.0;
  m.MB      = 0.0;
  m.MC      = 0.0;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "MA, MB, MC and Mmax are all zero" ) );

  // Not a number.

  collector = (TestCollector) {};
  m.MB      = (double) NAN;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "MA, MB, MC and Mmax must be finite numbers" ) );

  steelcheck_database_free ( &f.db );
}

/*
 *  Cb has no upper limit in NBR 8800:2024; below 1.0 it is accepted with a warning.
 */

static void test_member_cb ()
{
  Fixture_           f         = {};
  SteelCheckMember   m         = {};
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK ( fixture_( &f ) );
  CHECK_INT_EQ ( steelcheck_member_init( &m, "B1", f.section, f.material, 6000.0 ), STEELCHECK_OK );

  m.Cb = 3.5;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  m.Cb = 0.8;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.warnings, 1 );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_WARNING, 0, 0,
                              "member 'B1': Cb 0.8 is below 1.0: only valid from an elastic "
                              "stability analysis (5.4.2.3, 5.4.2.5)" ) );

  steelcheck_database_free ( &f.db );
}

/*
 *  The moments of a member with one flange continuously restrained (has_free_flange).
 */

static void test_member_free_flange ()
{
  Fixture_           f         = {};
  SteelCheckMember   m         = {};
  TestCollector      collector = {};
  SteelCheckReporter reporter  = test_reporter ( &collector );

  CHECK ( fixture_( &f ) );
  CHECK_INT_EQ ( steelcheck_member_init( &m, "B1", f.section, f.material, 6000.0 ), STEELCHECK_OK );

  m.has_free_flange = true;
  m.M1              = -100e6;
  m.M2              = 40e6;
  m.MCL             = 20e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_OK );
  CHECK_SIZE_EQ ( collector.count, 0 );

  // M1 must compress the free flange.

  m.M1 = 100e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "M1 must be negative (it compresses the free flange)" ) );

  // M2 compresses the free flange more than M1: they are swapped.

  collector = (TestCollector) {};
  m.M1      = -100e6;
  m.M2      = -150e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "compresses the free flange more than M1" ) );

  // Both ways of computing Cb.

  collector     = (TestCollector) {};
  m.M2          = 0.0;
  m.has_moments = true;
  m.MA          = 1e6;
  m.MB          = 1e6;
  m.MC          = 1e6;

  CHECK_INT_EQ ( steelcheck_member_validate( &m, &reporter ), STEELCHECK_ERROR_VALUE );
  CHECK ( test_diagnostic_is( &collector, 0, STEELCHECK_SEVERITY_ERROR, 0, 0,
                              "give either MA, MB, MC or M1, M2, MCL for Cb, not both" ) );

  steelcheck_database_free ( &f.db );
}

//==================================================================================================
//  test_member_suite
//==================================================================================================

void test_member_suite ()
{
  RUN_TEST ( test_material_table );
  RUN_TEST ( test_material_find );
  RUN_TEST ( test_material_make );
  RUN_TEST ( test_member_init );
  RUN_TEST ( test_member_validate );
  RUN_TEST ( test_member_moments );
  RUN_TEST ( test_member_cb );
  RUN_TEST ( test_member_free_flange );
}
