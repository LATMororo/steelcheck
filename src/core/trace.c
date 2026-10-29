#include "steelcheck/trace.h"

#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

/*
 *  First capacities; every growth doubles them.
 */

constexpr size_t initial_steps_ = 32;
constexpr size_t initial_arena_ = 1024;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  Makes room in the arena for needed more bytes. The first time, it also stores the empty string
 *  at offset 0.
 */

static SteelCheckStatus reserve_arena_ ( SteelCheckTrace* trace, size_t needed )
{
  // The bytes needed in total, including the '\0' of offset 0 for a new arena.

  size_t used  = trace->arena == nullptr ? 1 : trace->arena_used;
  size_t total = 0;

  if ( ckd_add( &total, used, needed ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  if ( trace->arena != nullptr && total <= trace->arena_capacity )
  {
    return STEELCHECK_OK;
  }

  size_t capacity = trace->arena_capacity > 0 ? trace->arena_capacity : initial_arena_;

  while ( capacity < total )
  {
    if ( ckd_mul( &capacity, capacity, 2 ) )
    {
      return STEELCHECK_ERROR_OVERFLOW;
    }
  }

  char* arena = realloc ( trace->arena, capacity );

  if ( arena == nullptr )
  {
    return STEELCHECK_ERROR_NO_MEMORY;
  }

  if ( trace->arena == nullptr )
  {
    arena[0]          = '\0';
    trace->arena_used = 1;
  }

  trace->arena          = arena;
  trace->arena_capacity = capacity;

  return STEELCHECK_OK;
}

/*
 *  Makes room for one more step.
 */

static SteelCheckStatus reserve_step_ ( SteelCheckTrace* trace )
{
  if ( trace->count < trace->capacity )
  {
    return STEELCHECK_OK;
  }

  size_t capacity = initial_steps_;
  size_t bytes    = 0;

  if ( ( trace->capacity > 0 && ckd_mul( &capacity, trace->capacity, 2 ) )
       || ckd_mul( &bytes, capacity, sizeof *trace->steps ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  SteelCheckTraceStep* steps = realloc ( trace->steps, bytes );

  if ( steps == nullptr )
  {
    return STEELCHECK_ERROR_NO_MEMORY;
  }

  trace->steps    = steps;
  trace->capacity = capacity;

  return STEELCHECK_OK;
}

/*
 *  The arena bytes a string needs: its characters and its '\0'; nothing for "" (offset 0).
 */

static size_t bytes_for_ ( const char* text )
{
  return text != nullptr && text[0] != '\0' ? strlen( text ) + 1 : 0;
}

/*
 *  Copies text into the arena, which has room for it, and returns its offset (0 for "").
 */

static size_t store_ ( SteelCheckTrace* trace, const char* text )
{
  size_t bytes = bytes_for_( text );

  if ( bytes == 0 )
  {
    return 0;
  }

  size_t offset = trace->arena_used;

  memcpy ( trace->arena + offset, text, bytes );
  trace->arena_used += bytes;

  return offset;
}

//==================================================================================================
//  steelcheck_trace_add
//==================================================================================================

SteelCheckStatus steelcheck_trace_add ( SteelCheckTrace* trace, const SteelCheckStepInput* step )
{
  if ( trace == nullptr || step == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  // Reserve all the memory first, so that a failure leaves the trace exactly as it was.

  const char* strings[] = { step->symbol, step->symbol_tex, step->expression, step->clause,
                            step->text };
  size_t      needed    = 0;

  for ( size_t i = 0; i < sizeof strings / sizeof strings[0]; i++ )
  {
    if ( ckd_add( &needed, needed, bytes_for_( strings[i] ) ) )
    {
      return STEELCHECK_ERROR_OVERFLOW;
    }
  }

  SteelCheckStatus status = reserve_step_ ( trace );

  if ( status == STEELCHECK_OK )
  {
    status = reserve_arena_ ( trace, needed );
  }

  if ( status != STEELCHECK_OK )
  {
    return status;
  }

  // Nothing can fail from here on. C does not fix the order in which the store_ calls of this
  // initializer run, but that only changes where each string lands in the arena: every call
  // returns the offset of its own string, whatever the order.

  trace->steps[trace->count] = (SteelCheckTraceStep)
  {
    .kind       = step->kind,
    .value      = step->value,
    .unit       = step->unit,
    .symbol     = store_( trace, step->symbol ),
    .symbol_tex = store_( trace, step->symbol_tex ),
    .expression = store_( trace, step->expression ),
    .clause     = store_( trace, step->clause ),
    .text       = store_( trace, step->text ),
  };

  trace->count++;

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_trace_text
//==================================================================================================

const char* steelcheck_trace_text ( const SteelCheckTrace* trace, size_t offset )
{
  if ( trace == nullptr || trace->arena == nullptr || offset >= trace->arena_used )
  {
    return "";
  }

  return trace->arena + offset;
}

//==================================================================================================
//  steelcheck_trace_free
//==================================================================================================

void steelcheck_trace_free ( SteelCheckTrace* trace )
{
  if ( trace == nullptr )
  {
    return;
  }

  free ( trace->steps );
  free ( trace->arena );

  *trace = (SteelCheckTrace) {};
}

//==================================================================================================
//  steelcheck_unit_name
//==================================================================================================

const char* steelcheck_unit_name ( SteelCheckUnit unit )
{
  switch ( unit )
  {
    case STEELCHECK_UNIT_NONE:
      return "";

    case STEELCHECK_UNIT_MM:
      return "mm";

    case STEELCHECK_UNIT_MM2:
      return "mm^2";

    case STEELCHECK_UNIT_MM3:
      return "mm^3";

    case STEELCHECK_UNIT_MM4:
      return "mm^4";

    case STEELCHECK_UNIT_MM6:
      return "mm^6";

    case STEELCHECK_UNIT_MPA:
      return "MPa";

    case STEELCHECK_UNIT_N:
      return "N";

    case STEELCHECK_UNIT_NMM:
      return "N mm";
  }

  return "";
}
