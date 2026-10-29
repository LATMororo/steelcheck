#include "steelcheck/material.h"

#include <string.h>

/*
 *  The built-in grades. fy and fu are the minimum values of the material standards for rolled
 *  sections (ASTM A36, A572, A588, A992; ABNT NBR 7007 for the Brazilian grades).
 */

static const SteelCheckMaterial materials_[] =
{
  { "ASTM A36",           250.0, 400.0, steelcheck_steel_E, steelcheck_steel_G },
  { "ASTM A572 Gr.50",    345.0, 450.0, steelcheck_steel_E, steelcheck_steel_G },
  { "ASTM A588",          345.0, 485.0, steelcheck_steel_E, steelcheck_steel_G },
  { "ASTM A992",          345.0, 450.0, steelcheck_steel_E, steelcheck_steel_G },
  { "NBR 7007 MR250",     250.0, 400.0, steelcheck_steel_E, steelcheck_steel_G },
  { "NBR 7007 AR350",     350.0, 450.0, steelcheck_steel_E, steelcheck_steel_G },
  { "NBR 7007 AR350 COR", 350.0, 485.0, steelcheck_steel_E, steelcheck_steel_G },
  { "NBR 7007 AR415",     415.0, 520.0, steelcheck_steel_E, steelcheck_steel_G },
};

constexpr size_t material_count_ = sizeof materials_ / sizeof materials_[0];

/*
 *  Prefixes that may be left out when a grade is looked up (as keys, see key_).
 */

static const char* const standards_[] = { "ASTM", "NBR7007" };

/*
 *  Size of a key buffer: longer names cannot match any grade anyway.
 */

constexpr size_t key_size_ = 64;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  The comparison key of a grade name: in upper case, without spaces, tabs, '.', '-' and '_',
 *  e.g. "ASTM A572 Gr.50" -> "ASTMA572GR50". Returns false if it does not fit.
 */

static bool key_ ( const char* name, char* key, size_t size )
{
  size_t length = 0;

  for ( const char* p = name; *p != '\0'; p++ )
  {
    char c = *p;

    if ( c == ' ' || c == '\t' || c == '.' || c == '-' || c == '_' )
    {
      continue;
    }

    if ( length + 1 >= size )
    {
      return false;
    }

    key[length++] = c >= 'a' && c <= 'z' ? (char) ( c - 'a' + 'A' ) : c;
  }

  key[length] = '\0';

  return length > 0;
}

/*
 *  True if the key of a grade's full name matches key, with or without its standard's prefix.
 */

static bool matches_ ( const SteelCheckMaterial* material, const char* key )
{
  char full[key_size_];

  if ( !key_( material->name, full, sizeof full ) )
  {
    return false;
  }

  if ( strcmp( full, key ) == 0 )
  {
    return true;
  }

  for ( size_t i = 0; i < sizeof standards_ / sizeof standards_[0]; i++ )
  {
    size_t length = strlen ( standards_[i] );

    if ( strncmp( full, standards_[i], length ) == 0 && strcmp( full + length, key ) == 0 )
    {
      return true;
    }
  }

  return false;
}

//==================================================================================================
//  steelcheck_materials
//==================================================================================================

const SteelCheckMaterial* steelcheck_materials ( size_t* count )
{
  if ( count != nullptr )
  {
    *count = material_count_;
  }

  return materials_;
}

//==================================================================================================
//  steelcheck_material_find
//==================================================================================================

const SteelCheckMaterial* steelcheck_material_find ( const char* name )
{
  char key[key_size_];

  if ( name == nullptr || !key_( name, key, sizeof key ) )
  {
    return nullptr;
  }

  for ( size_t i = 0; i < material_count_; i++ )
  {
    if ( matches_( &materials_[i], key ) )
    {
      return &materials_[i];
    }
  }

  return nullptr;
}

//==================================================================================================
//  steelcheck_material_make
//==================================================================================================

SteelCheckStatus steelcheck_material_make ( SteelCheckMaterial* material, const char* name,
                                            double fy, double fu,
                                            const SteelCheckReporter* reporter )
{
  if ( material == nullptr || name == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  bool failed = false;

  // Every check runs, so every problem is reported. !( x > 0 ) also rejects NaN.

  if ( strlen( name ) >= steelcheck_material_name_size )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE, 0, 0,
                        "material name '%s' is longer than %zu characters", name,
                        steelcheck_material_name_size - 1 );
    failed = true;
  }

  if ( !( fy > 0.0 ) )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE, 0, 0,
                        "material '%s': fy must be positive, got %g MPa", name, fy );
    failed = true;
  }

  if ( !( fu > 0.0 ) )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE, 0, 0,
                        "material '%s': fu must be positive, got %g MPa", name, fu );
    failed = true;
  }
  else if ( fu < fy )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE, 0, 0,
                        "material '%s': fu %g MPa is less than fy %g MPa", name, fu, fy );
    failed = true;
  }

  if ( failed )
  {
    *material = (SteelCheckMaterial) {};
    return STEELCHECK_ERROR_VALUE;
  }

  *material = (SteelCheckMaterial) { .fy = fy, .fu = fu, .E = steelcheck_steel_E,
                                     .G = steelcheck_steel_G };

  memcpy ( material->name, name, strlen( name ) + 1 );

  return STEELCHECK_OK;
}
