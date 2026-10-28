#include "steelcheck/view.h"

#include <string.h>
#include <limits.h>

//==================================================================================================
//  is_blank_
//==================================================================================================

/*
 *  Space or tab. isspace() from <ctype.h> is deliberately not used: it is undefined behavior for
 *  negative char values (bytes >= 0x80, e.g. UTF-8 "ç", when char is signed), and its result
 *  depends on the locale.
 */

static bool is_blank_ ( char c )
{
  return c == ' ' || c == '\t';
}

//==================================================================================================
//  steelcheck_view_from_cstr
//==================================================================================================

SteelCheckView steelcheck_view_from_cstr ( const char* string )
{
  if ( string == nullptr )
  {
    return (SteelCheckView) {};
  }

  return (SteelCheckView) { .length = strlen( string ), .data = string };
}

//==================================================================================================
//  steelcheck_view_trim
//==================================================================================================

SteelCheckView steelcheck_view_trim ( SteelCheckView view )
{
   // An empty view may have data == nullptr: return it untouched, never index into it.

  if ( view.length == 0 )
  {
    return view;
  }

  const char* begin = view.data;
  const char* end   = view.data + view.length;

  while ( begin < end && is_blank_( *begin ) )
  {
    ++begin;
  }

  while ( end > begin && is_blank_( end[-1] ) )
  {
    --end;
  }

  return (SteelCheckView) { .length = (size_t) ( end - begin ), .data = begin };
}

//==================================================================================================
//  steelcheck_view_is_empty
//==================================================================================================

bool steelcheck_view_is_empty ( SteelCheckView view )
{
  return view.length == 0;
}

//==================================================================================================
//  steelcheck_view_equals
//==================================================================================================

bool steelcheck_view_equals ( SteelCheckView a, SteelCheckView b )
{
  // memcmp must not receive nullptr, even with a length of 0, so empty views are handled first.

  if ( a.length != b.length )
  {
    return false;
  }

  return a.length == 0 || memcmp( a.data, b.data, a.length ) == 0;
}

//==================================================================================================
//  steelcheck_view_equals_cstr
//==================================================================================================

bool steelcheck_view_equals_cstr ( SteelCheckView view, const char* string )
{
  return steelcheck_view_equals ( view, steelcheck_view_from_cstr( string ) );
}

//==================================================================================================
//  steelcheck_view_starts_with
//==================================================================================================

bool steelcheck_view_starts_with ( SteelCheckView view, const char* prefix )
{
  SteelCheckView p = steelcheck_view_from_cstr ( prefix );

  if ( p.length > view.length )
  {
    return false;
  }

  return p.length == 0 || memcmp( view.data, p.data, p.length ) == 0;
}

//==================================================================================================
//  steelcheck_view_print_length
//==================================================================================================

int steelcheck_view_print_length ( SteelCheckView view )
{
  return view.length > (size_t)INT_MAX ? INT_MAX : (int)view.length;
}
