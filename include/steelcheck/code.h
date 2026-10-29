#ifndef STEELCHECK_CODE_H
#define STEELCHECK_CODE_H

#include "steelcheck/compat.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/member.h"
#include "steelcheck/result.h"
#include "steelcheck/status.h"

/*
 *  The interface of a design code (NBR 8800 now; AISC 360 and Eurocode 3 later).
 *
 *  A code is a table of functions, one per kind of check, indexed by SteelCheckCheckKind. This is
 *  C's way of writing an interface: the driver, steelcheck_check_member, calls whatever function a
 *  code puts in the table, without knowing which code it is. A code that does not support a check
 *  leaves its entry nullptr.
 *
 *  A check function fills result->checks[kind]: its limit states (or another status, such as
 *  NOT_IMPLEMENTED), its design force (has_demand, demand), and the steps of its calculation in
 *  result->trace. It may read the checks of lower kinds, which are already finished (the
 *  interaction check needs the resistances of the others). It returns STEELCHECK_OK, or the
 *  status of a failure to add to the trace (out of memory).
 */

typedef SteelCheckStatus ( *SteelCheckCheckFunction ) ( const SteelCheckMember* member,
                                                        SteelCheckResult* result );

typedef struct SteelCheckCode
{
  const char*             id;     // short name for input files, e.g. "NBR 8800"
  const char*             name;   // full name for reports, e.g. "ABNT NBR 8800"
  const char*             note;   // shown at the top of each calculation (may be nullptr)
  SteelCheckCheckFunction checks[STEELCHECK_CHECK_COUNT];
} SteelCheckCode;

/*
 *  The code whose id matches (ignoring case, spaces, '.', '-' and '_': "nbr8800" finds
 *  "NBR 8800"), or nullptr.
 */

[[nodiscard]] const SteelCheckCode* steelcheck_code_find ( const char* id );

/*
 *  The default code: ABNT NBR 8800.
 */

[[nodiscard]] const SteelCheckCode* steelcheck_code_default ();

/*
 *  Checks member against code and fills *result (which must not hold a previous result; free
 *  that first).
 *
 *  The member is validated first (problems are reported; STEELCHECK_ERROR_VALUE is returned and
 *  result is left empty). Then each check runs in the order of SteelCheckCheckKind:
 *    - without design forces, every check except the interaction computes its resistance;
 *    - with design forces, only the checks the forces call for: tension if NSd > 0, compression
 *      if NSd < 0, bending about x if MxSd != 0, ..., and the interaction if at least two of NSd,
 *      MxSd and MySd are not zero; the others are NOT_APPLICABLE.
 *  Each check is finished (governing state, utilization), then the result (governing check).
 *
 *  Returns STEELCHECK_OK, STEELCHECK_ERROR_VALUE, STEELCHECK_ERROR_NO_MEMORY (result is then
 *  freed), or STEELCHECK_ERROR_INVALID_ARGUMENT.
 */

[[nodiscard]] SteelCheckStatus steelcheck_check_member ( const SteelCheckCode* code,
                                                         const SteelCheckMember* member,
                                                         SteelCheckResult* result,
                                                         const SteelCheckReporter* reporter );

#endif // STEELCHECK_CODE_H
