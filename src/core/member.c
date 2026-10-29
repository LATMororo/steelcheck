#include "steelcheck/member.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 *  The value of Cb for a uniform moment, the smallest that 5.4.2.3 a) gives for doubly symmetric
 *  sections. NBR 8800:2024 sets no upper limit; a smaller value can only come from an elastic
 *  stability analysis (destabilizing loads, some cantilevers: 5.4.2.3, 5.4.2.5).
 */

constexpr double cb_uniform_ = 1.0;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  The state of one validation: where to report, the member, whether an error was found.
 */

typedef struct Context_
{
  const SteelCheckReporter* reporter;
  const SteelCheckMember*   member;
  bool                      failed;
} Context_;

/*
 *  Reports an error about the member, prefixed with its label.
 */

STEELCHECK_PRINTF ( 2, 3 )
static void error_ ( Context_* ctx, const char* format, ... )
{
  char detail[steelcheck_diagnostic_message_size];

  va_list args;
  va_start ( args, format );
  vsnprintf ( detail, sizeof detail, format, args );
  va_end ( args );

  steelcheck_report ( ctx->reporter, STEELCHECK_SEVERITY_ERROR, STEELCHECK_ERROR_VALUE, 0, 0,
                      "member '%s': %s", ctx->member->label, detail );

  ctx->failed = true;
}

/*
 *  Reports a warning about the member: the data is used anyway.
 */

STEELCHECK_PRINTF ( 2, 3 )
static void warning_ ( Context_* ctx, const char* format, ... )
{
  char detail[steelcheck_diagnostic_message_size];

  va_list args;
  va_start ( args, format );
  vsnprintf ( detail, sizeof detail, format, args );
  va_end ( args );

  steelcheck_report ( ctx->reporter, STEELCHECK_SEVERITY_WARNING, STEELCHECK_ERROR_VALUE, 0, 0,
                      "member '%s': %s", ctx->member->label, detail );
}

/*
 *  Reports an error if value is not positive (!( value > 0 ) also catches NaN).
 */

static void positive_ ( Context_* ctx, const char* name, double value )
{
  if ( !( value > 0.0 ) )
  {
    error_ ( ctx, "%s must be positive, got %g", name, value );
  }
}

//==================================================================================================
//  steelcheck_member_init
//==================================================================================================

SteelCheckStatus steelcheck_member_init ( SteelCheckMember* member, const char* label,
                                          const SteelCheckSection* section,
                                          const SteelCheckMaterial* material, double length )
{
  if ( member == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  *member = (SteelCheckMember) {};

  size_t label_length = label != nullptr ? strlen( label ) : 0;

  if ( label_length >= steelcheck_member_label_size )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  if ( label_length > 0 )
  {
    memcpy ( member->label, label, label_length + 1 );
  }

  member->section  = section;
  member->material = material;

  member->Lx = length;
  member->Ly = length;
  member->Lz = length;
  member->Lb = length;
  member->Kx = 1.0;
  member->Ky = 1.0;
  member->Kz = 1.0;
  member->Cb = 1.0;

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_member_validate
//==================================================================================================

SteelCheckStatus steelcheck_member_validate ( const SteelCheckMember* member,
                                              const SteelCheckReporter* reporter )
{
  if ( member == nullptr )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  Context_ ctx = { .reporter = reporter, .member = member };

  if ( member->section == nullptr )
  {
    error_ ( &ctx, "no section" );
  }

  if ( member->material == nullptr )
  {
    error_ ( &ctx, "no material" );
  }

  positive_ ( &ctx, "Lx", member->Lx );
  positive_ ( &ctx, "Ly", member->Ly );
  positive_ ( &ctx, "Lz", member->Lz );
  positive_ ( &ctx, "Kx", member->Kx );
  positive_ ( &ctx, "Ky", member->Ky );
  positive_ ( &ctx, "Kz", member->Kz );

  // Lb = 0 means a continuously braced compression flange: no lateral-torsional buckling.

  if ( !( member->Lb >= 0.0 ) )
  {
    error_ ( &ctx, "Lb must not be negative, got %g", member->Lb );
  }

  positive_ ( &ctx, "Cb", member->Cb );

  if ( member->Cb > 0.0 && member->Cb < cb_uniform_ && !member->has_moments
       && !member->has_free_flange )
  {
    warning_ ( &ctx, "Cb %g is below 1.0: only valid from an elastic stability analysis "
               "(5.4.2.3, 5.4.2.5)", member->Cb );
  }

  // The moment diagram for Cb: Mmax (when given) is the largest moment, so it is not smaller
  // than the others, and the diagram is not all zeros.

  if ( member->has_moments )
  {
    double MA   = fabs ( member->MA );
    double MB   = fabs ( member->MB );
    double MC   = fabs ( member->MC );
    double Mmax = fabs ( member->Mmax );

    if ( !isfinite( MA ) || !isfinite( MB ) || !isfinite( MC ) || !isfinite( Mmax ) )
    {
      error_ ( &ctx, "MA, MB, MC and Mmax must be finite numbers" );
    }
    else if ( Mmax > 0.0 && Mmax < fmax( MA, fmax( MB, MC ) ) )
    {
      error_ ( &ctx, "Mmax %g N mm is smaller than one of MA, MB, MC", Mmax );
    }
    else if ( fmax( Mmax, fmax( MA, fmax( MB, MC ) ) ) == 0.0 && member->MxSd == 0.0 )
    {
      error_ ( &ctx, "MA, MB, MC and Mmax are all zero: no moment diagram for Cb" );
    }
  }

  // The moments of a member with one flange continuously restrained (5.4.2.4 a)): M1 compresses
  // the free flange (negative), and is the largest end moment that does.

  if ( member->has_free_flange )
  {
    if ( !isfinite( member->M1 ) || !isfinite( member->M2 ) || !isfinite( member->MCL ) )
    {
      error_ ( &ctx, "M1, M2 and MCL must be finite numbers" );
    }
    else if ( !( member->M1 < 0.0 ) )
    {
      error_ ( &ctx, "M1 must be negative (it compresses the free flange), got %g N mm",
               member->M1 );
    }
    else if ( member->M2 < member->M1 )
    {
      error_ ( &ctx, "M2 %g N mm compresses the free flange more than M1 %g N mm: swap them",
               member->M2, member->M1 );
    }
  }

  if ( member->has_moments && member->has_free_flange )
  {
    error_ ( &ctx, "give either MA, MB, MC or M1, M2, MCL for Cb, not both" );
  }

  // The net section needs both An and Ct, or neither.

  bool has_an = member->An != 0.0;
  bool has_ct = member->Ct != 0.0;

  if ( has_an != has_ct )
  {
    error_ ( &ctx, "An and Ct must be given together (An = %g, Ct = %g)", member->An, member->Ct );
  }

  if ( has_an && member->section != nullptr
       && !( member->An > 0.0 && member->An <= member->section->A ) )
  {
    error_ ( &ctx, "An must be positive and at most A = %g mm^2, got %g", member->section->A,
             member->An );
  }

  if ( has_ct && !( member->Ct > 0.0 && member->Ct <= 1.0 ) )
  {
    error_ ( &ctx, "Ct must be greater than 0 and at most 1, got %g", member->Ct );
  }

  return ctx.failed ? STEELCHECK_ERROR_VALUE : STEELCHECK_OK;
}
