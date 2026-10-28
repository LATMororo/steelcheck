#ifndef STEELCHECK_VIEW_H
#define STEELCHECK_VIEW_H

#include <stddef.h>

/*
 *  SteelCheckView borrows memory owned by someone else (a SteelCheckBuffer, a string literal, the
 *  embedded database, ...). It never frees that memory, and it becomes invalid when the memory is
 *  freed, reallocated, or otherwise ceases to exist.
 *
 *  A view is not '\0'-terminated: its length is explicit. To print one, use "%.*s" with the length
 *  as an int.
 */

typedef struct SteelCheckView
{
  size_t      length;  // bytes visible through the view
  const char* data;    // borrowed, read-only memory; not necessarily '\0'-terminated
} SteelCheckView;

/*
 *  View of a '\0'-terminated string (not including the '\0'). nullptr gives an empty view.
 */

[[nodiscard]] SteelCheckView steelcheck_view_from_cstr ( const char* string );

/*
 *  The view without spaces and tabs at both ends. Other bytes, including '\r' and '\n', are kept.
 */

[[nodiscard]] SteelCheckView steelcheck_view_trim ( SteelCheckView view );

/*
 *  True if the view holds no bytes.
 */

[[nodiscard]] bool steelcheck_view_is_empty ( SteelCheckView view );

/*
 *  True if both views hold the same bytes (two empty views are equal).
 */

[[nodiscard]] bool steelcheck_view_equals ( SteelCheckView a, SteelCheckView b );

/*
 *  True if the view holds exactly the bytes of the '\0'-terminated string (nullptr counts as "").
 */

[[nodiscard]] bool steelcheck_view_equals_cstr ( SteelCheckView view, const char* string );

/*
 *  True if the view begins with the bytes of the '\0'-terminated prefix (nullptr counts as "").
 */

[[nodiscard]] bool steelcheck_view_starts_with ( SteelCheckView view, const char* prefix );

/*
 *  The view's length as an int, for "%.*s" (whose precision must be an int). Longer views, which
 *  never occur in this program, are clamped to INT_MAX.
 */

[[nodiscard]] int steelcheck_view_print_length ( SteelCheckView view );

#endif // STEELCHECK_VIEW_H
