#ifndef STEELCHECK_TRACE_H
#define STEELCHECK_TRACE_H

#include "steelcheck/compat.h"
#include "steelcheck/status.h"

#include <stddef.h>

/*
 *  The calculation trace: every step of a design check, in order, as the reports will show it.
 *  Values are stored as numbers in internal units, never as formatted text: each output (terminal,
 *  HTML, LaTeX) chooses its own units and precision.
 *
 *  Memory: all the strings of a trace are copied into one growing block, the text arena, and the
 *  steps refer to them by offset, the position of the string in the arena. Offsets, not pointers:
 *  growing the arena with realloc may move it, which would invalidate every pointer into it, but
 *  never an offset. Offset 0 is always the empty string. One steelcheck_trace_free releases all.
 */

typedef enum SteelCheckUnit : int
{
  STEELCHECK_UNIT_NONE = 0,   // a ratio, a factor, a slenderness
  STEELCHECK_UNIT_MM,
  STEELCHECK_UNIT_MM2,
  STEELCHECK_UNIT_MM3,
  STEELCHECK_UNIT_MM4,
  STEELCHECK_UNIT_MM6,
  STEELCHECK_UNIT_MPA,
  STEELCHECK_UNIT_N,
  STEELCHECK_UNIT_NMM,        // N mm, moments
} SteelCheckUnit;

typedef enum SteelCheckStepKind : int
{
  STEELCHECK_STEP_HEADING  = 0,   // a title, e.g. "Lateral-torsional buckling"
  STEELCHECK_STEP_GIVEN    = 1,   // an input value, e.g. fy = 345 MPa
  STEELCHECK_STEP_EQUATION = 2,   // a computed value with its equation
  STEELCHECK_STEP_DECISION = 3,   // a choice, e.g. "lambda <= lambda_p: compact"
  STEELCHECK_STEP_NOTE     = 4,   // an explanation
  STEELCHECK_STEP_WARNING  = 5,   // something the engineer should look at
} SteelCheckStepKind;

/*
 *  One step. The strings are offsets into the trace's arena: read them with steelcheck_trace_text.
 */

typedef struct SteelCheckTraceStep
{
  SteelCheckStepKind kind;
  double             value;        // in internal units (unused for headings and notes)
  SteelCheckUnit     unit;
  size_t             symbol;       // plain text, e.g. "lambda_p"
  size_t             symbol_tex;   // LaTeX, e.g. "\lambda_p"
  size_t             expression;   // LaTeX equation, e.g. "0.38 \sqrt{E / f_y}"
  size_t             clause;       // e.g. "5.4.2"
  size_t             text;         // description, heading or note
} SteelCheckTraceStep;

/*
 *  What steelcheck_trace_add copies: the same fields, with ordinary strings (nullptr means "").
 *  Meant for compound literals with designated initializers, so a step lists only what it has.
 */

typedef struct SteelCheckStepInput
{
  SteelCheckStepKind kind;
  double             value;
  SteelCheckUnit     unit;
  const char*        symbol;
  const char*        symbol_tex;
  const char*        expression;
  const char*        clause;
  const char*        text;
} SteelCheckStepInput;

typedef struct SteelCheckTrace
{
  SteelCheckTraceStep* steps;          // owned
  size_t               count;
  size_t               capacity;

  char*                arena;          // owned: the strings, each followed by its '\0'
  size_t               arena_used;
  size_t               arena_capacity;
} SteelCheckTrace;

/*
 *  Appends a step, copying its strings. trace starts zero-initialized (SteelCheckTrace t = {}).
 *  Returns STEELCHECK_OK, STEELCHECK_ERROR_NO_MEMORY or STEELCHECK_ERROR_OVERFLOW (the trace is
 *  then unchanged, apart from possibly a larger capacity), or STEELCHECK_ERROR_INVALID_ARGUMENT.
 */

[[nodiscard]] SteelCheckStatus steelcheck_trace_add ( SteelCheckTrace* trace,
                                                      const SteelCheckStepInput* step );

/*
 *  The string at offset in the trace's arena; "" for offset 0, nullptr or an invalid offset. The
 *  pointer is valid until the next steelcheck_trace_add (which may move the arena) or free.
 */

[[nodiscard]] const char* steelcheck_trace_text ( const SteelCheckTrace* trace, size_t offset );

/*
 *  Frees everything and leaves trace empty. Safe on an empty trace, twice, and on nullptr.
 */

void steelcheck_trace_free ( SteelCheckTrace* trace );

/*
 *  The unit's symbol in plain text: "", "mm", "mm^2", ..., "MPa", "N", "N mm".
 */

[[nodiscard]] const char* steelcheck_unit_name ( SteelCheckUnit unit );

#endif // STEELCHECK_TRACE_H
