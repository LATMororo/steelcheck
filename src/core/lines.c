#include "steelcheck/lines.h"

#include <string.h>

//==================================================================================================
//  steelcheck_lines_init
//==================================================================================================

void steelcheck_lines_init ( SteelCheckLines* iter, SteelCheckView view )
{
  // Check if the line iterator is valid.

  if ( iter == nullptr )
  {
    return;
  }

  iter->curr        = view.data;
  iter->end         = view.data;
  iter->line_number = 0;

  // An empty view (no data) has no lines.

  if ( view.data == nullptr )
  {
    return;
  }

  // Set end pointer.

  iter->end = view.data + view.length;

  // Skip a UTF-8 BOM (Byte Order Mark) only at the beginning of the view.
  // NOTE: UTF-8 has no byte-order issue (unlike UTF-16), but some Windows editors still write
  //       these 3 bytes at the start of a file to mark it as UTF-8.

  if ( view.length >= 3 &&
       (unsigned char) view.data[0] == 0xEF &&
       (unsigned char) view.data[1] == 0xBB &&
       (unsigned char) view.data[2] == 0xBF )
  {
    iter->curr += 3;
  }
}

//==================================================================================================
//  steelcheck_lines_next
//==================================================================================================

bool steelcheck_lines_next ( SteelCheckLines* iter, SteelCheckView* line, size_t* line_number )
{
  // Check the validity of the arguments.

  if ( iter == nullptr || line == nullptr || line_number == nullptr )
  {
    return false;
  }

  *line        = (SteelCheckView) {};
  *line_number = 0;

  // No remaining bytes means no more lines.

  if ( iter->curr == iter->end )
  {
    return false;
  }

  // Search the next '\n' from the current position to the end of the view.

  const char* start    = iter->curr;
  const char* new_line = memchr ( start, '\n', (size_t) ( iter->end - start ) );

  // The line ends at the '\n', or at the end of the view for a last line without '\n'.

  const char* end_line = nullptr;

  if ( new_line != nullptr )
  {
    end_line   = new_line;
    iter->curr = new_line + 1;
  }
  else
  {
    end_line   = iter->end;
    iter->curr = iter->end;
  }

  // Remove one CR immediately before LF, or at the end of the view.

  if ( end_line > start && end_line[-1] == '\r' )
  {
    --end_line;
  }

  *line = (SteelCheckView) {
    .length = (size_t) ( end_line - start ),
    .data   = start,
  };

  ++iter->line_number;
  *line_number = iter->line_number;

  return true;
}
