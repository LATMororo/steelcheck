#ifndef STEELCHECK_INPUT_H
#define STEELCHECK_INPUT_H

#include "steelcheck/compat.h"
#include "steelcheck/code.h"
#include "steelcheck/database.h"
#include "steelcheck/diagnostic.h"
#include "steelcheck/member.h"
#include "steelcheck/status.h"
#include "steelcheck/view.h"

#include <stddef.h>

/*
 *  Input files: the members to check, written in a small subset of TOML (https://toml.io).
 *
 *      # Comments start with '#' and go to the end of the line.
 *
 *      code     = "NBR 8800"          # optional; the default is NBR 8800
 *      material = "A572 Gr.50"        # optional; the material of members that give none
 *
 *      [[member]]                     # one table per member
 *      label    = "C1"
 *      section  = "W 310 x 52.0"      # required; looked up in the section database
 *      length   = 3.5                 # required [m]; the default of Lx, Ly, Lz and Lb
 *      Ky       = 0.8
 *      NSd      = -850                # [kN], compression negative
 *      MxSd     = 120                 # [kN m]
 *
 *  Member keys and their units in the file (converted to the internal mm, N, N mm on reading):
 *
 *      label                 string, at most 31 bytes (default: "M1", "M2", ... by position)
 *      section, material     strings
 *      length, Lx, Ly, Lz    m
 *      Lb                    m (0: the compression flange is braced along the whole length)
 *      Kx, Ky, Kz, Cb, Ct    no unit
 *      An                    cm^2
 *      NSd, VySd, VxSd       kN
 *      MxSd, MySd            kN m
 *      MA, MB, MC, Mmax      kN m: the moment diagram over Lb, for Cb (see below)
 *      M1, M2, MCL           kN m: moments for Cb with one flange restrained (see below)
 *
 *  Giving any of NSd, MxSd, MySd, VySd or VxSd makes the member one with design forces (the others
 *  are then 0); without them, only resistances are computed (see steelcheck_check_member).
 *
 *  Cb is either given, or computed by the code from the moments at a quarter, the middle and
 *  three quarters of Lb (MA, MB, MC, given together) and the largest one in Lb (Mmax, optional:
 *  the largest of MA, MB, MC and MxSd). When one flange has continuous lateral restraint and the
 *  other is free, Cb comes instead from M1, M2 and MCL (5.4.2.4 a), given together; negative
 *  compresses the free flange). Give only one of Cb, MA-MB-MC and M1-M2-MCL; without any,
 *  Cb = 1.0.
 *
 *  The subset of TOML: bare keys (letters, digits, '_' and '-'; case matters), strings in double
 *  quotes (escapes \" \\ \t \n and \r) or single quotes (no escapes), decimal numbers as
 *  steelcheck_parse_double accepts them, and [[member]] tables. Not supported: other tables,
 *  dotted or quoted keys, multi-line strings, arrays, inline tables, booleans, dates, and the
 *  '_' digit separator.
 */

typedef struct SteelCheckInput
{
  const SteelCheckCode* code;       // the "code" key, or steelcheck_code_default()
  SteelCheckMember*     members;    // owned array; each member borrows its section from the
                                    // database and its material from the built-in grades
  size_t                count;      // members in the array
  size_t                capacity;   // members allocated
} SteelCheckInput;

/*
 *  Reads the members in text; their sections are looked up in db, which must outlive input.
 *
 *  input must be empty (zero-initialized or freed). Every problem is reported with its line and
 *  column: a syntax error; an unknown or repeated key; a value of the wrong type; an invalid
 *  number; an unknown code, section or material; a member without section, length or material;
 *  member data rejected by steelcheck_member_validate; no member at all. One error rejects the
 *  whole text (input is then left empty).
 *
 *  Returns STEELCHECK_OK or the status of the first error (STEELCHECK_ERROR_SYNTAX,
 *  STEELCHECK_ERROR_DUPLICATE, STEELCHECK_ERROR_VALUE, STEELCHECK_ERROR_NUMBER,
 *  STEELCHECK_ERROR_RANGE, STEELCHECK_ERROR_MISSING_VALUE), STEELCHECK_ERROR_NO_MEMORY,
 *  STEELCHECK_ERROR_OVERFLOW, or STEELCHECK_ERROR_INVALID_ARGUMENT.
 */

[[nodiscard]] SteelCheckStatus steelcheck_input_load ( SteelCheckInput* input, SteelCheckView text,
                                                       const SteelCheckDatabase* db,
                                                       const SteelCheckReporter* reporter );

/*
 *  Reads the file at path and loads it like steelcheck_input_load. A file that cannot be read is
 *  reported (without the path: the caller knows it) and its status returned.
 */

[[nodiscard]] SteelCheckStatus steelcheck_input_load_file ( SteelCheckInput* input,
                                                            const char* path,
                                                            const SteelCheckDatabase* db,
                                                            const SteelCheckReporter* reporter );

/*
 *  Frees the members and leaves input empty. Safe on an empty input and on nullptr.
 */

void steelcheck_input_free ( SteelCheckInput* input );

#endif // STEELCHECK_INPUT_H
