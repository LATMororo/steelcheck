#ifndef STEELCHECK_BUFFER_H
#define STEELCHECK_BUFFER_H

#include "steelcheck/compat.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>

/*
 *  SteelCheckBuffer owns its memory; a SteelCheckView (view.h) of it borrows that memory and 
 *  becomes invalid when the buffer is freed.
 */

typedef struct SteelCheckBuffer
{
  size_t size;         // file bytes, excluding the terminating '\0'
  char*  data;         // file contents followed by '\0'; nullptr when the buffer is empty
} SteelCheckBuffer;

/*
 *  Reads the whole file at path into buff.
 *
 *  buff must be empty (zero-initialized, e.g. SteelCheckBuffer buff = {}, or freed). On success,
 *  buff->data holds size bytes plus a terminating '\0' (also for an empty file). On failure, buff
 *  is left empty. Files larger than 64 MiB are rejected with STEELCHECK_ERROR_TOO_LARGE.
 */

[[nodiscard]] SteelCheckStatus steelcheck_buffer_load ( SteelCheckBuffer* buff, const char* path );

/*
 *  Frees the memory owned by buff and leaves it empty. Safe on an empty buffer, on a buffer that
 *  was already freed, and on nullptr.
 */

void steelcheck_buffer_free ( SteelCheckBuffer* buff );

/*
 *  Returns a view of the whole buffer (an empty view for an empty buffer or nullptr). The view is
 *  valid only while buff is not freed.
 */

[[nodiscard]] SteelCheckView steelcheck_buffer_view ( const SteelCheckBuffer* buff );

#endif // STEELCHECK_BUFFER_H
