#ifndef STEELCHECK_LINES_H
#define STEELCHECK_LINES_H

#include "steelcheck/compat.h"
#include "steelcheck/buffer.h"

#include <stddef.h>

/*
 *  The line iterator, SteelCheckLines, works on top of a SteelCheckView. It is non-owning,
 *  allocation-free, and stateful: initialization records where to start, and each call to
 *  steelcheck_lines_next() returns a view of the next line into the original memory.
 *
 *  The current position is a pointer into the original view, so no string is created for any
 *  line. Line ends are found with memchr(), and lengths are explicit, so a '\0' byte inside a
 *  line does not end it.
 *
 *  Rules: lines end at '\n'; one '\r' right before it (CRLF), or at the very end of the input, is
 *  removed. A last line without '\n' is still a line. A UTF-8 BOM is skipped at the start only.
 *  Line numbers start at 1 and count every line, including blank ones.
 *
 *  The views returned are valid only while the memory of the original view exists.
 */

typedef struct SteelCheckLines
{
  size_t      line_number; // number of the most recently produced line (0 before the first)
  const char* curr;        // beginning of the next line to process
  const char* end;         // one past the final byte of the input
} SteelCheckLines;

/*
 *  Prepares iter to go through the lines of view.
 */

void steelcheck_lines_init ( SteelCheckLines* iter, SteelCheckView view );

/*
 *  Produces the next line and its number. Returns false when there are no more lines (or an
 *  argument is nullptr); line and line_number are then set to an empty view and 0.
 */

[[nodiscard]] bool steelcheck_lines_next ( SteelCheckLines* iter, SteelCheckView* line,
                                           size_t* line_number );

#endif // STEELCHECK_LINES_H
