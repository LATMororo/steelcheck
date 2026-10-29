#include "steelcheck/report.h"

#include "steelcheck/section.h"

#include <math.h>
#include <string.h>

/*
 *  Size of one line of the text report, including its '\0'.
 */

constexpr size_t step_line_size_ = 256;

/*
 *  Size of a formatted quantity, a number and its unit ("988.1 kN"), including its '\0'.
 */

constexpr size_t quantity_size_ = steelcheck_number_text_size + 16;

/*
 *  Version of KaTeX loaded by the HTML reports, from cdnjs.
 */

#define KATEX_ "https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.16.9/"

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  The unit a check's resistance and design force are reported in.
 */

static SteelCheckUnit check_unit_ ( SteelCheckCheckKind kind )
{
  switch ( kind )
  {
    case STEELCHECK_CHECK_TENSION:
    case STEELCHECK_CHECK_COMPRESSION:
    case STEELCHECK_CHECK_SHEAR_Y:
    case STEELCHECK_CHECK_SHEAR_X:
      return STEELCHECK_UNIT_N;

    case STEELCHECK_CHECK_BENDING_X:
    case STEELCHECK_CHECK_BENDING_Y:
      return STEELCHECK_UNIT_NMM;

    case STEELCHECK_CHECK_INTERACTION:
    case STEELCHECK_CHECK_COUNT:
      break;
  }

  return STEELCHECK_UNIT_NONE;
}

/*
 *  A value in internal units, formatted in display units: "988.1 kN".
 */

static void format_quantity_ ( double value, SteelCheckUnit unit, char* text, size_t size )
{
  SteelCheckDisplayUnit display = steelcheck_display_unit ( unit );
  char                  number[steelcheck_number_text_size];

  steelcheck_format_number ( value * display.factor, number, sizeof number );
  snprintf ( text, size, "%s%s%s", number, display.text[0] != '\0' ? " " : "", display.text );
}

/*
 *  A LaTeX symbol made readable as plain text: without '\', '{' and '}', e.g.
 *  "N_{c,Rd,x}" -> "N_c,Rd,x" and "\lambda_p" -> "lambda_p".
 */

static void tex_to_text_ ( const char* tex, char* text, size_t size )
{
  size_t length = 0;

  for ( const char* p = tex; *p != '\0' && length + 1 < size; p++ )
  {
    if ( *p != '\\' && *p != '{' && *p != '}' )
    {
      text[length++] = *p;
    }
  }

  text[length] = '\0';
}

/*
 *  Writes text with the characters that are special in HTML escaped.
 */

static void html_escape_ ( FILE* out, const char* text )
{
  for ( const char* p = text; *p != '\0'; p++ )
  {
    switch ( *p )
    {
      case '&':
        fputs ( "&amp;", out );
        break;

      case '<':
        fputs ( "&lt;", out );
        break;

      case '>':
        fputs ( "&gt;", out );
        break;

      case '"':
        fputs ( "&quot;", out );
        break;

      default:
        fputc ( *p, out );
        break;
    }
  }
}

/*
 *  The member's lengths and design forces, as lines of text.
 */

static void member_data_text_ ( FILE* out, const SteelCheckMember* m )
{
  char a[quantity_size_];
  char b[quantity_size_];
  char c[quantity_size_];
  char d[quantity_size_];

  format_quantity_ ( m->Lx, STEELCHECK_UNIT_MM, a, sizeof a );
  format_quantity_ ( m->Ly, STEELCHECK_UNIT_MM, b, sizeof b );
  format_quantity_ ( m->Lz, STEELCHECK_UNIT_MM, c, sizeof c );
  format_quantity_ ( m->Lb, STEELCHECK_UNIT_MM, d, sizeof d );

  fprintf ( out, "  lengths  Lx = %s (Kx = %g), Ly = %s (Ky = %g), Lz = %s (Kz = %g)\n",
            a, m->Kx, b, m->Ky, c, m->Kz );
  if ( m->has_moments )
  {
    fprintf ( out, "           Lb = %s, Cb from the moments in Lb:\n", d );

    format_quantity_ ( m->MA, STEELCHECK_UNIT_NMM, a, sizeof a );
    format_quantity_ ( m->MB, STEELCHECK_UNIT_NMM, b, sizeof b );
    format_quantity_ ( m->MC, STEELCHECK_UNIT_NMM, c, sizeof c );

    if ( m->Mmax != 0.0 )
    {
      format_quantity_ ( m->Mmax, STEELCHECK_UNIT_NMM, d, sizeof d );
    }
    else
    {
      snprintf ( d, sizeof d, "(largest)" );
    }

    fprintf ( out, "           MA = %s, MB = %s, MC = %s, Mmax = %s\n", a, b, c, d );
  }
  else if ( m->has_free_flange )
  {
    fprintf ( out, "           Lb = %s (free flange), Cb from the moments (5.4.2.4 a)):\n", d );

    format_quantity_ ( m->M1, STEELCHECK_UNIT_NMM, a, sizeof a );
    format_quantity_ ( m->M2, STEELCHECK_UNIT_NMM, b, sizeof b );
    format_quantity_ ( m->MCL, STEELCHECK_UNIT_NMM, c, sizeof c );

    fprintf ( out, "           M1 = %s, M2 = %s, MCL = %s\n", a, b, c );
  }
  else
  {
    fprintf ( out, "           Lb = %s, Cb = %g\n", d, m->Cb );
  }

  if ( !m->has_actions )
  {
    fprintf ( out, "  forces   none given: resistances only\n" );
    return;
  }

  format_quantity_ ( m->NSd,  STEELCHECK_UNIT_N,   a, sizeof a );
  format_quantity_ ( m->MxSd, STEELCHECK_UNIT_NMM, b, sizeof b );
  format_quantity_ ( m->MySd, STEELCHECK_UNIT_NMM, c, sizeof c );

  fprintf ( out, "  forces   NSd = %s, MxSd = %s, MySd = %s\n", a, b, c );

  format_quantity_ ( m->VySd, STEELCHECK_UNIT_N, a, sizeof a );
  format_quantity_ ( m->VxSd, STEELCHECK_UNIT_N, b, sizeof b );

  fprintf ( out, "           VySd = %s, VxSd = %s\n", a, b );
}

/*
 *  Writes line without its trailing spaces, then a newline: the columns of a table are padded,
 *  and the last ones are often empty.
 */

static void put_line_ ( FILE* out, const char* line )
{
  size_t length = strlen ( line );

  while ( length > 0 && line[length - 1] == ' ' )
  {
    length--;
  }

  fprintf ( out, "%.*s\n", (int) length, line );
}

/*
 *  One step of the trace as a line of text.
 */

static void step_text_ ( FILE* out, const SteelCheckTrace* trace, const SteelCheckTraceStep* step )
{
  const char* text   = steelcheck_trace_text ( trace, step->text );
  const char* clause = steelcheck_trace_text ( trace, step->clause );

  switch ( step->kind )
  {
    case STEELCHECK_STEP_HEADING:
      fprintf ( out, "\n%s\n", text );
      return;

    case STEELCHECK_STEP_NOTE:
      fprintf ( out, "  %s\n", text );
      return;

    case STEELCHECK_STEP_DECISION:
      fprintf ( out, "    -> %s\n", text );
      return;

    case STEELCHECK_STEP_WARNING:
      fprintf ( out, "    WARNING: %s\n", text );
      return;

    case STEELCHECK_STEP_GIVEN:
    case STEELCHECK_STEP_EQUATION:
      break;
  }

  SteelCheckDisplayUnit display = steelcheck_display_unit ( step->unit );
  char                  symbol[64];
  char                  number[steelcheck_number_text_size];

  tex_to_text_ ( steelcheck_trace_text( trace, step->symbol_tex ), symbol, sizeof symbol );
  steelcheck_format_number ( step->value * display.factor, number, sizeof number );

  char line[step_line_size_];
  int  used = snprintf ( line, sizeof line, "    %-14s = %12s %-6s %s", symbol, number,
                         display.text, text );

  if ( clause[0] != '\0' && used >= 0 && (size_t) used < sizeof line )
  {
    snprintf ( &line[used], sizeof line - (size_t) used, "%s[%s]", text[0] != '\0' ? " " : "",
               clause );
  }

  put_line_ ( out, line );
}

/*
 *  The design force the governing limit state was compared with (its own, or the check's).
 */

static double governing_force_ ( const SteelCheckCheck* check )
{
  return check->state_count > 0 ? check->states[check->governing].applied : check->demand;
}

/*
 *  True if the limit states of a check were compared with different design forces: the report
 *  then shows each one.
 */

static bool forces_differ_ ( const SteelCheckCheck* check )
{
  if ( !check->has_demand )
  {
    return false;
  }

  for ( size_t i = 1; i < check->state_count; i++ )
  {
    if ( check->states[i].applied != check->states[0].applied )
    {
      return true;
    }
  }

  return false;
}

/*
 *  The conclusion of a check, as a line of text (and one line per limit state, when they were
 *  compared with different design forces).
 */

static void check_conclusion_text_ ( FILE* out, const SteelCheckCheck* check )
{
  SteelCheckUnit unit = check_unit_ ( check->kind );
  char           resistance[quantity_size_];
  char           demand[quantity_size_];

  if ( check->state_count == 0 )
  {
    fprintf ( out, "  => %s\n", steelcheck_check_status_name( check->status ) );
    return;
  }

  format_quantity_ ( check->resistance, unit, resistance, sizeof resistance );

  fprintf ( out, "  => resistance %s (%s)", resistance,
            steelcheck_limit_state_name( check->states[check->governing].state ) );

  if ( check->has_demand )
  {
    format_quantity_ ( governing_force_( check ), unit, demand, sizeof demand );
    fprintf ( out, ", design force %s, utilization %.2f: %s", demand, check->utilization,
              steelcheck_check_status_name( check->status ) );
  }

  fprintf ( out, "\n" );

  if ( !forces_differ_( check ) )
  {
    return;
  }

  for ( size_t i = 0; i < check->state_count; i++ )
  {
    const SteelCheckLimitResult* state = &check->states[i];

    format_quantity_ ( state->design, unit, resistance, sizeof resistance );
    format_quantity_ ( state->applied, unit, demand, sizeof demand );

    fprintf ( out, "     %s: resistance %s, design force %s, utilization %.2f\n",
              steelcheck_limit_state_name( state->state ), resistance, demand,
              state->utilization );
  }
}

/*
 *  The trace steps of one check, in the order they were written.
 */

static const SteelCheckTraceStep* steps_of_ ( const SteelCheckResult* result,
                                              const SteelCheckCheck* check )
{
  return check->trace_count > 0 ? &result->trace.steps[check->trace_first] : nullptr;
}

/*
 *  Number of steps before the first one of any check: the notes of the code. The first check with
 *  steps is not always checks[0] (tension is often not applicable).
 */

static size_t leading_steps_ ( const SteelCheckResult* result )
{
  size_t first = result->trace.count;

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck* check = &result->checks[k];

    if ( check->trace_count > 0 && check->trace_first < first )
    {
      first = check->trace_first;
    }
  }

  return first;
}

//==================================================================================================
//  steelcheck_display_unit
//==================================================================================================

SteelCheckDisplayUnit steelcheck_display_unit ( SteelCheckUnit unit )
{
  switch ( unit )
  {
    case STEELCHECK_UNIT_NONE:
      return (SteelCheckDisplayUnit) { "", "", 1.0 };

    case STEELCHECK_UNIT_MM:
      return (SteelCheckDisplayUnit) { "mm", "\\mathrm{mm}", 1.0 };

    case STEELCHECK_UNIT_MM2:
      return (SteelCheckDisplayUnit) { "cm^2", "\\mathrm{cm}^2", 1e-2 };

    case STEELCHECK_UNIT_MM3:
      return (SteelCheckDisplayUnit) { "cm^3", "\\mathrm{cm}^3", 1e-3 };

    case STEELCHECK_UNIT_MM4:
      return (SteelCheckDisplayUnit) { "cm^4", "\\mathrm{cm}^4", 1e-4 };

    case STEELCHECK_UNIT_MM6:
      return (SteelCheckDisplayUnit) { "cm^6", "\\mathrm{cm}^6", 1e-6 };

    case STEELCHECK_UNIT_MPA:
      return (SteelCheckDisplayUnit) { "MPa", "\\mathrm{MPa}", 1.0 };

    case STEELCHECK_UNIT_N:
      return (SteelCheckDisplayUnit) { "kN", "\\mathrm{kN}", 1e-3 };

    case STEELCHECK_UNIT_NMM:
      return (SteelCheckDisplayUnit) { "kN m", "\\mathrm{kN\\,m}", 1e-6 };
  }

  return (SteelCheckDisplayUnit) { "", "", 1.0 };
}

//==================================================================================================
//  steelcheck_format_number
//==================================================================================================

void steelcheck_format_number ( double value, char* text, size_t size )
{
  if ( fabs( value ) < 1000.0 )
  {
    snprintf ( text, size, "%.4g", value );
  }
  else if ( fabs( value - round( value ) ) < 1e-6 * fabs( value ) )
  {
    snprintf ( text, size, "%.0f", value );
  }
  else
  {
    snprintf ( text, size, "%.1f", value );
  }
}

//==================================================================================================
//  steelcheck_report_text
//==================================================================================================

void steelcheck_report_text ( FILE* out, const SteelCheckMember* member,
                              const SteelCheckResult* result )
{
  fprintf ( out, "======================================================================"
                 "==========\n" );
  fprintf ( out, "%s: %s, %s, %s\n", member->label, member->section->designation,
            member->material->name, result->code );

  member_data_text_ ( out, member );

  // The steps before the first check: the code's note.

  size_t leading = leading_steps_ ( result );

  for ( size_t i = 0; i < leading; i++ )
  {
    fprintf ( out, "  note: %s\n", steelcheck_trace_text( &result->trace,
                                                           result->trace.steps[i].text ) );
  }

  // Each check that was done: its steps, then its conclusion.

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck*     check = &result->checks[k];
    const SteelCheckTraceStep* steps = steps_of_ ( result, check );

    for ( size_t i = 0; i < check->trace_count; i++ )
    {
      step_text_ ( out, &result->trace, &steps[i] );
    }

    if ( check->trace_count > 0 )
    {
      check_conclusion_text_ ( out, check );
    }
  }

  // Summary table.

  fprintf ( out, "\nSummary\n" );
  fprintf ( out, "  %-25s %-16s %16s %16s %11s\n", "check", "status", "resistance",
            "design force", "utilization" );

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck* check = &result->checks[k];
    SteelCheckUnit         unit  = check_unit_ ( check->kind );
    char                   resistance[quantity_size_]         = "";
    char                   demand[quantity_size_]             = "";
    char                   ratio[steelcheck_number_text_size] = "";

    if ( check->state_count > 0 )
    {
      format_quantity_ ( check->resistance, unit, resistance, sizeof resistance );
    }

    if ( check->state_count > 0 && check->has_demand )
    {
      format_quantity_ ( governing_force_( check ), unit, demand, sizeof demand );
      snprintf ( ratio, sizeof ratio, "%.2f", check->utilization );
    }

    char line[step_line_size_];

    snprintf ( line, sizeof line, "  %-25s %-16s %16s %16s %11s",
               steelcheck_check_kind_name( check->kind ),
               steelcheck_check_status_name( check->status ), resistance, demand, ratio );
    put_line_ ( out, line );
  }

  if ( result->has_governing )
  {
    fprintf ( out, "\n  Governing: %s, utilization %.2f: %s\n",
              steelcheck_check_kind_name( result->governing ), result->utilization,
              result->utilization <= 1.0 ? "OK" : "FAIL" );
  }
  else
  {
    fprintf ( out, "\n  No design forces: resistances only.\n" );
  }
}

//==================================================================================================
//  steelcheck_report_html_begin
//==================================================================================================

void steelcheck_report_html_begin ( FILE* out, const char* title )
{
  fputs ( "<!DOCTYPE html>\n"
          "<html lang=\"en\">\n"
          "<head>\n"
          "<meta charset=\"utf-8\">\n"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n", out );

  fputs ( "<title>", out );
  html_escape_ ( out, title );
  fputs ( "</title>\n", out );

  fputs ( "<link rel=\"stylesheet\" href=\"" KATEX_ "katex.min.css\">\n"
          "<script defer src=\"" KATEX_ "katex.min.js\"></script>\n"
          "<script defer src=\"" KATEX_ "contrib/auto-render.min.js\"\n"
          "        onload=\"renderMathInElement(document.body)\"></script>\n"
          "<style>\n"
          "  body { font-family: system-ui, sans-serif; max-width: 62rem; margin: auto;\n"
          "         padding: 1rem; line-height: 1.45; color: #1d1d1d; background: #fff; }\n"
          "  h2 { border-bottom: 2px solid #ccc; padding-bottom: .2rem; margin-top: 2.5rem; }\n"
          "  table { border-collapse: collapse; margin: .5rem 0 1rem; }\n"
          "  th, td { border: 1px solid #ccc; padding: .25rem .6rem; text-align: right; }\n"
          "  th:first-child, td:first-child { text-align: left; }\n"
          "  ul.steps { list-style: none; padding-left: 1rem; }\n"
          "  .desc { color: #555; margin-left: 1rem; }\n"
          "  .clause { color: #777; font-size: .85em; margin-left: .5rem; }\n"
          "  .note { font-weight: 600; margin-top: .6rem; }\n"
          "  .decision { color: #335; font-style: italic; }\n"
          "  .warning { color: #a40; font-weight: 600; }\n"
          "  .OK { color: #176317; font-weight: 600; }\n"
          "  .FAIL { color: #b00; font-weight: 700; }\n"
          "</style>\n"
          "</head>\n"
          "<body>\n", out );

  fputs ( "<h1>", out );
  html_escape_ ( out, title );
  fputs ( "</h1>\n", out );
}

//==================================================================================================
//  steelcheck_report_html_member
//==================================================================================================

void steelcheck_report_html_member ( FILE* out, const SteelCheckMember* member,
                                     const SteelCheckResult* result )
{
  char text[quantity_size_];

  fputs ( "<section>\n<h2>", out );
  html_escape_ ( out, member->label );
  fputs ( ": ", out );
  html_escape_ ( out, member->section->designation );
  fputs ( ", ", out );
  html_escape_ ( out, member->material->name );
  fputs ( "</h2>\n<p>", out );
  html_escape_ ( out, result->code );

  size_t leading = leading_steps_ ( result );

  for ( size_t i = 0; i < leading; i++ )
  {
    fputs ( ". ", out );
    html_escape_ ( out, steelcheck_trace_text( &result->trace, result->trace.steps[i].text ) );
  }

  fputs ( "</p>\n", out );

  // Summary table first: the result at a glance.

  fputs ( "<table>\n<tr><th>check</th><th>status</th><th>resistance</th>"
          "<th>design force</th><th>utilization</th></tr>\n", out );

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck* check = &result->checks[k];
    SteelCheckUnit         unit  = check_unit_ ( check->kind );
    const char*            name  = steelcheck_check_status_name ( check->status );

    fprintf ( out, "<tr><td>%s</td><td class=\"%s\">%s</td><td>",
              steelcheck_check_kind_name( check->kind ), name, name );

    if ( check->state_count > 0 )
    {
      format_quantity_ ( check->resistance, unit, text, sizeof text );
      fputs ( text, out );
    }

    fputs ( "</td><td>", out );

    if ( check->state_count > 0 && check->has_demand )
    {
      format_quantity_ ( governing_force_( check ), unit, text, sizeof text );
      fprintf ( out, "%s</td><td>%.2f", text, check->utilization );
    }
    else
    {
      fputs ( "</td><td>", out );
    }

    fputs ( "</td></tr>\n", out );
  }

  fputs ( "</table>\n", out );

  // The calculation of each check.

  for ( int k = 0; k < STEELCHECK_CHECK_COUNT; k++ )
  {
    const SteelCheckCheck*     check = &result->checks[k];
    const SteelCheckTraceStep* steps = steps_of_ ( result, check );

    if ( check->trace_count == 0 )
    {
      continue;
    }

    for ( size_t i = 0; i < check->trace_count; i++ )
    {
      const SteelCheckTraceStep* step   = &steps[i];
      const char*                stext  = steelcheck_trace_text ( &result->trace, step->text );
      const char*                clause = steelcheck_trace_text ( &result->trace, step->clause );

      switch ( step->kind )
      {
        case STEELCHECK_STEP_HEADING:
          fputs ( "<h3>", out );
          html_escape_ ( out, stext );
          fputs ( "</h3>\n<ul class=\"steps\">\n", out );
          continue;

        case STEELCHECK_STEP_NOTE:
        case STEELCHECK_STEP_DECISION:
        case STEELCHECK_STEP_WARNING:
          fprintf ( out, "<li class=\"%s\">", step->kind == STEELCHECK_STEP_NOTE ? "note"
                    : step->kind == STEELCHECK_STEP_DECISION ? "decision" : "warning" );
          html_escape_ ( out, stext );
          fputs ( "</li>\n", out );
          continue;

        case STEELCHECK_STEP_GIVEN:
        case STEELCHECK_STEP_EQUATION:
          break;
      }

      // An equation: \( symbol = expression = value unit \), then its description and clause.

      SteelCheckDisplayUnit display    = steelcheck_display_unit ( step->unit );
      const char*           expression = steelcheck_trace_text ( &result->trace,
                                                                 step->expression );

      steelcheck_format_number ( step->value * display.factor, text, sizeof text );

      fputs ( "<li>\\( ", out );
      html_escape_ ( out, steelcheck_trace_text( &result->trace, step->symbol_tex ) );

      if ( expression[0] != '\0' )
      {
        fputs ( " = ", out );
        html_escape_ ( out, expression );
      }

      fprintf ( out, " = %s", text );

      if ( display.tex[0] != '\0' )
      {
        fprintf ( out, "\\, %s", display.tex );
      }

      fputs ( " \\)", out );

      if ( stext[0] != '\0' )
      {
        fputs ( "<span class=\"desc\">", out );
        html_escape_ ( out, stext );
        fputs ( "</span>", out );
      }

      if ( clause[0] != '\0' )
      {
        fputs ( "<span class=\"clause\">[", out );
        html_escape_ ( out, clause );
        fputs ( "]</span>", out );
      }

      fputs ( "</li>\n", out );
    }

    // Limit states compared with different design forces: each one.

    if ( forces_differ_( check ) )
    {
      SteelCheckUnit unit = check_unit_ ( check->kind );
      char           force[quantity_size_];

      for ( size_t i = 0; i < check->state_count; i++ )
      {
        const SteelCheckLimitResult* state = &check->states[i];

        format_quantity_ ( state->design, unit, text, sizeof text );
        format_quantity_ ( state->applied, unit, force, sizeof force );

        fprintf ( out, "<li class=\"decision\">%s: resistance %s, design force %s, "
                  "utilization %.2f</li>\n", steelcheck_limit_state_name( state->state ), text,
                  force, state->utilization );
      }
    }

    fputs ( "</ul>\n", out );
  }

  if ( result->has_governing )
  {
    const char* verdict = result->utilization <= 1.0 ? "OK" : "FAIL";

    fprintf ( out, "<p>Governing: %s, utilization %.2f: <span class=\"%s\">%s</span></p>\n",
              steelcheck_check_kind_name( result->governing ), result->utilization, verdict,
              verdict );
  }

  fputs ( "</section>\n", out );
}

//==================================================================================================
//  steelcheck_report_html_end
//==================================================================================================

void steelcheck_report_html_end ( FILE* out )
{
  fputs ( "</body>\n</html>\n", out );
}
