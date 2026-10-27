#include "steelcheck/buffer.h"

#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 *  Private constants: maximum file size and the initial capacity of the data area.
 */

constexpr size_t max_file_size_    = 64u * 1024u * 1024u; // 64 MiB
constexpr size_t initial_capacity_ = 4u * 1024u;          // 4 KiB

//==================================================================================================
//  steelcheck_buffer_load
//==================================================================================================

SteelCheckStatus steelcheck_buffer_load ( SteelCheckBuffer* buff, const char* path )
{
  // First, take a shortcut: check if the given arguments are valid.

  if ( buff == nullptr || path == nullptr || path[0] == '\0' )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  // Precondition: buff is empty. It stays empty until the read succeeds completely.

  if ( buff->data != nullptr || buff->size != 0 )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  // Locals released by the cleanup path.

  FILE* file              = nullptr;

  size_t size             = 0;
  size_t capacity         = 0;
  size_t alloc_size       = 0;

  char* data              = nullptr;

  SteelCheckStatus status = STEELCHECK_OK;

  // Limitation: on Windows, fopen does not correctly support all non-ASCII file paths. A function
  // can later use the appropriate wide-character Windows API. For now, on Linux, pass the path
  // through unchanged.

  file = fopen ( path, "rb" );

  if ( file == nullptr )
  {
    status = STEELCHECK_ERROR_OPEN;
    goto cleanup;
  }

  // Initialize current capacity.

  capacity = initial_capacity_;

  if ( capacity > max_file_size_ )
  {
    capacity = max_file_size_;
  }

  // Allocate memory for data (one extra byte for the terminator), but first check for overflow.

  if ( ckd_add( &alloc_size, capacity, 1 ) )
  {
    status = STEELCHECK_ERROR_OVERFLOW;
    goto cleanup;
  }

  data = malloc ( alloc_size );

  if ( data == nullptr )
  {
    status = STEELCHECK_ERROR_NO_MEMORY;
    goto cleanup;
  }

  // Loop over all the file and store its content in data.

  for ( ; ; )
  {
    size_t bytes_read = 0;

    // Grow before reading when the data area is full.

    if ( size == capacity )
    {
      if ( capacity == max_file_size_ )
      {
        // The limit has been reached. Read one extra byte to distinguish an exact-size file from a
        // file that exceeds the limit.

        int ch = fgetc ( file );

        if ( ch != EOF )
        {
          status = STEELCHECK_ERROR_TOO_LARGE;
          goto cleanup;
        }

        if ( ferror( file ) )
        {
          status = STEELCHECK_ERROR_READ;
          goto cleanup;
        }

        break; // normal EOF at exactly the size limit
      }

      // Double the capacity, up to the size limit.

      size_t new_capacity = 0;

      if ( ckd_mul( &new_capacity, capacity, 2 ) )
      {
        status = STEELCHECK_ERROR_OVERFLOW;
        goto cleanup;
      }

      if ( new_capacity > max_file_size_ )
      {
        new_capacity = max_file_size_;
      }

      if ( ckd_add( &alloc_size, new_capacity, 1 ) )
      {
        status = STEELCHECK_ERROR_OVERFLOW;
        goto cleanup;
      }

      // Realloc data. Failure leaves data valid, so cleanup can still free it.

      char* new_data = realloc ( data, alloc_size );

      if ( new_data == nullptr )
      {
        status = STEELCHECK_ERROR_NO_MEMORY;
        goto cleanup;
      }

      // Update data and capacity.

      data     = new_data;
      capacity = new_capacity;
    }

    // Read file content.

    bytes_read  = fread ( data + size, 1, capacity - size, file );
    size       += bytes_read;

    if ( bytes_read == 0 )
    {
      if ( ferror( file ) )
      {
        status = STEELCHECK_ERROR_READ;
        goto cleanup;
      }

      if ( feof( file ) )
      {
        break;
      }

      // No progress, neither EOF nor an error: avoid an infinite loop.

      status = STEELCHECK_ERROR_READ;
      goto cleanup;
    }
  }

  // Terminate the data. The extra byte was reserved by every allocation; it is not part of size.

  data[size] = '\0';

cleanup:
  if ( file != nullptr )
  {
    // The file was only read, so a failing fclose cannot lose data: its result is ignored.
    // (Files that are written, e.g. reports, must check it.)

    fclose ( file );
  }

  // On failure, release everything and leave buff empty.

  if ( status != STEELCHECK_OK )
  {
    free ( data );
    return status;
  }

  // Return the complete buffer only after success.

  buff->data = data;
  buff->size = size;

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_buffer_free
//==================================================================================================

void steelcheck_buffer_free ( SteelCheckBuffer* buff )
{
  if ( buff == nullptr )
  {
    return;
  }

  free ( buff->data );

  buff->data = nullptr;
  buff->size = 0;
}

//==================================================================================================
//  steelcheck_buffer_view
//==================================================================================================

SteelCheckView steelcheck_buffer_view ( const SteelCheckBuffer* buff )
{
  SteelCheckView view = {};

  if ( buff != nullptr && buff->data != nullptr )
  {
    view.data   = buff->data;
    view.length = buff->size;
  }

  return view;
}
