#ifndef STEELCHECK_REPORT_H
#define STEELCHECK_REPORT_H

#include "steelcheck/compat.h"
#include "steelcheck/member.h"
#include "steelcheck/result.h"
#include "steelcheck/trace.h"

#include <stddef.h>
#include <stdio.h>

/*
 *  Reports of design checks, written from the result and its calculation trace: plain text for the
 *  terminal, and HTML with the equations typeset by KaTeX (loaded from a CDN, so the page needs a
 *  network connection to show formatted equations; without one it shows their LaTeX source).
 *
 *  Values are converted from internal units to display units here, and only here: N to kN, N mm
 *  to kN m, mm^2 to cm^2, and so on.
 */

/*
 *  A display unit: its text, its LaTeX, and the factor from the internal unit
 *  (display value = internal value x factor).
 */

typedef struct SteelCheckDisplayUnit
{
  const char* text;     // e.g. "kN m"
  const char* tex;      // e.g. "\mathrm{kN\,m}"
  double      factor;   // e.g. 1e-6 for N mm -> kN m
} SteelCheckDisplayUnit;

[[nodiscard]] SteelCheckDisplayUnit steelcheck_display_unit ( SteelCheckUnit unit );

/*
 *  Size of a buffer that always holds a formatted number.
 */

constexpr size_t steelcheck_number_text_size = 32;

/*
 *  Formats a value the way catalogs and reports print it: up to 4 significant digits below 1000
 *  (6.18, 85.8, 0.6584), whole numbers above (8581, 14676643), with one decimal if it has one
 *  (1597.4).
 */

void steelcheck_format_number ( double value, char* text, size_t size );

/*
 *  Writes the text report of one member: its data, the calculation of every check that was done,
 *  and a summary table with the governing check.
 */

void steelcheck_report_text ( FILE* out, const SteelCheckMember* member,
                              const SteelCheckResult* result );

/*
 *  An HTML report: begin (the page head, the title), one section per member, end.
 */

void steelcheck_report_html_begin  ( FILE* out, const char* title );
void steelcheck_report_html_member ( FILE* out, const SteelCheckMember* member,
                                     const SteelCheckResult* result );
void steelcheck_report_html_end    ( FILE* out );

#endif // STEELCHECK_REPORT_H
