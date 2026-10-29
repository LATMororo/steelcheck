#include "steelcheck/builtin.h"

/*
 *  #embed "file" turns the bytes of a file into a comma-separated list of integer constants at
 *  compile time, so the file becomes the initializer of an ordinary array. The path is relative
 *  to this source file, like for #include "...".
 *
 *  __has_embed ( "file" ) tells whether the compiler can embed the file: __STDC_EMBED_FOUND__,
 *  __STDC_EMBED_NOT_FOUND__ or __STDC_EMBED_EMPTY__. The two #if are nested on purpose: a compiler
 *  without __has_embed would still parse a combined "defined(__has_embed) && __has_embed(...)",
 *  and fail on it, although && would never evaluate the second part.
 */

#if defined(__has_embed)
#  if __has_embed ( "../../data/gerdau_w_hp.csv" ) == __STDC_EMBED_FOUND__
#    define STEELCHECK_HAS_BUILTIN 1
#  else
#    error "data/gerdau_w_hp.csv cannot be embedded: missing or empty"
#  endif
#else
#  define STEELCHECK_HAS_BUILTIN 0
#endif

#if STEELCHECK_HAS_BUILTIN

/*
 *  unsigned char, not char: the embedded values are bytes from 0 to 255, which a (signed) char
 *  could not all hold. The data is not '\0'-terminated; its length is sizeof builtin_text_.
 */

static const unsigned char builtin_text_[] =
{
#  embed "../../data/gerdau_w_hp.csv"
};

#endif

//==================================================================================================
//  steelcheck_builtin_available
//==================================================================================================

bool steelcheck_builtin_available ()
{
  return STEELCHECK_HAS_BUILTIN;
}

//==================================================================================================
//  steelcheck_builtin_text
//==================================================================================================

SteelCheckView steelcheck_builtin_text ()
{
#if STEELCHECK_HAS_BUILTIN
  return (SteelCheckView) { .length = sizeof builtin_text_, .data = (const char*) builtin_text_ };
#else
  return (SteelCheckView) {};
#endif
}
