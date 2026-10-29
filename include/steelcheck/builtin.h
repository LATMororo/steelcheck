#ifndef STEELCHECK_BUILTIN_H
#define STEELCHECK_BUILTIN_H

#include "steelcheck/compat.h"
#include "steelcheck/view.h"

/*
 *  The built-in section database: data/gerdau_w_hp.csv, embedded in the program at compile time
 *  with the C23 #embed directive, so steelcheck works without any database file.
 *
 *  Compilers without #embed (e.g. GCC 14; it came with GCC 15 and Clang 19) build the program
 *  without it: steelcheck_builtin_available() then returns false, and a database file is needed.
 */

/*
 *  Name used as the source of the built-in database in messages, like a file path, e.g.
 *  "<built-in>:113: warning: ...".
 */

constexpr char steelcheck_builtin_source[] = "<built-in>";

/*
 *  True if this build contains the built-in database.
 */

[[nodiscard]] bool steelcheck_builtin_available ();

/*
 *  The text of the built-in database (the exact bytes of the CSV file), or an empty view if this
 *  build has none. The view stays valid for the whole run of the program.
 */

[[nodiscard]] SteelCheckView steelcheck_builtin_text ();

#endif // STEELCHECK_BUILTIN_H
