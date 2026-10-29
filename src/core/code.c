#include "steelcheck/code.h"

#include "steelcheck/nbr8800.h"

#include <string.h>

/*
 *  Every code the program knows.
 */

typedef const SteelCheckCode* ( *CodeGetter_ ) ();

static const CodeGetter_ codes_[] = { steelcheck_code_nbr8800 };

constexpr size_t key_size_ = 32;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  Comparison key of a code id: upper case, without spaces, tabs, '.', '-' and '_'.
 */

static bool key_ ( const char* id, char* key, size_t size )
{
  size_t length = 0;

  for ( const char* p = id; *p != '\0'; p++ )
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
 *  True if check kind applies to the member's design forces (see code.h).
 */

static bool applies_ ( const SteelCheckMember* m, SteelCheckCheckKind kind )
{
  if ( !m->has_actions )
  {
    return kind != STEELCHECK_CHECK_INTERACTION;
  }

  switch ( kind )
  {
    case STEELCHECK_CHECK_TENSION:
      return m->NSd > 0.0;

    case STEELCHECK_CHECK_COMPRESSION:
      return m->NSd < 0.0;

    case STEELCHECK_CHECK_BENDING_X:
      return m->MxSd != 0.0;

    case STEELCHECK_CHECK_BENDING_Y:
      return m->MySd != 0.0;

    case STEELCHECK_CHECK_SHEAR_Y:
      return m->VySd != 0.0;

    case STEELCHECK_CHECK_SHEAR_X:
      return m->VxSd != 0.0;

    case STEELCHECK_CHECK_INTERACTION:
      return ( m->NSd != 0.0 ) + ( m->MxSd != 0.0 ) + ( m->MySd != 0.0 ) >= 2;

    case STEELCHECK_CHECK_COUNT:
      break;
  }

  return false;
}

//==================================================================================================
//  steelcheck_code_find
//==================================================================================================

const SteelCheckCode* steelcheck_code_find ( const char* id )
{
  char key[key_size_];
  char other[key_size_];

  if ( id == nullptr || !key_( id, key, sizeof key ) )
  {
    return nullptr;
  }

  for ( size_t i = 0; i < sizeof codes_ / sizeof codes_[0]; i++ )
  {
    const SteelCheckCode* code = codes_[i] ();

    if ( key_( code->id, other, sizeof other ) && strcmp( key, other ) == 0 )
    {
      return code;
    }
  }

  return nullptr;
}

//==================================================================================================
//  steelcheck_code_default
//==================================================================================================

const SteelCheckCode* steelcheck_code_default ()
{
  return steelcheck_code_nbr8800 ();
}

//==================================================================================================
//  steelcheck_check_member
//==================================================================================================

SteelCheckStatus steelcheck_check_member ( const SteelCheckCode* code,
                                           const SteelCheckMember* member,
                                           SteelCheckResult* result,
                                           const SteelCheckReporter* reporter )
{
  if ( code == nullptr || member == nullptr || result == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  steelcheck_result_init ( result, code->name );

  SteelCheckStatus status = steelcheck_member_validate ( member, reporter );

  if ( status != STEELCHECK_OK )
  {
    return status;
  }

  if ( code->note != nullptr )
  {
    status = steelcheck_trace_add ( &result->trace, &(SteelCheckStepInput) {
                                      .kind = STEELCHECK_STEP_NOTE, .text = code->note } );
  }

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT && status == STEELCHECK_OK; k++ )
  {
    SteelCheckCheckKind kind  = (SteelCheckCheckKind) k;
    SteelCheckCheck*    check = &result->checks[k];

    if ( !applies_( member, kind ) )
    {
      check->status = STEELCHECK_CHECK_NOT_APPLICABLE;
      continue;
    }

    if ( code->checks[k] == nullptr )
    {
      check->status = STEELCHECK_CHECK_NOT_IMPLEMENTED;
      continue;
    }

    // The steps this check adds to the trace are the ones after the current end.

    check->trace_first = result->trace.count;

    status = code->checks[k] ( member, result );

    check->trace_count = result->trace.count - check->trace_first;

    steelcheck_check_finish ( check );
  }

  if ( status != STEELCHECK_OK )
  {
    steelcheck_result_free ( result );
    return status;
  }

  steelcheck_result_finish ( result );

  return STEELCHECK_OK;
}
