#include "steelcheck/input.h"

#include "steelcheck/buffer.h"
#include "steelcheck/lines.h"
#include "steelcheck/number.h"

#include <stdarg.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  Number of members allocated by the first growth; every further growth doubles it.
 */

constexpr size_t initial_capacity_ = 8;

/*
 *  Size of a string value, including its '\0': enough for any designation or material name.
 */

constexpr size_t value_size_ = 128;

//==================================================================================================
//  The keys
//==================================================================================================

typedef enum KeyKind_ : int
{
  KIND_LABEL_    = 0,   // string, copied into the member's label
  KIND_SECTION_  = 1,   // string, looked up in the database
  KIND_MATERIAL_ = 2,   // string, looked up in the built-in grades
  KIND_CODE_     = 3,   // string, looked up in the known codes (top level only)
  KIND_LENGTH_   = 4,   // number, the default of the unbraced lengths
  KIND_NUMBER_   = 5,   // number, stored in a double member of SteelCheckMember
} KeyKind_;

typedef struct Key_
{
  const char* name;
  KeyKind_    kind;
  size_t      offset;   // KIND_NUMBER_ only: offsetof the double in SteelCheckMember
  double      scale;    // file unit -> internal unit, e.g. 1e3 for m -> mm
  bool        action;   // a design force: giving it makes the member one with forces
} Key_;

/*
 *  A double member of SteelCheckMember, with the factor from the file's unit. #name turns the
 *  macro argument into a string literal, so the key and the member always have the same name.
 */

#define NUMBER_(name, scale, action) \
  { #name, KIND_NUMBER_, offsetof ( SteelCheckMember, name ), (scale), (action) }

static const Key_ top_keys_[] =
{
  { "code",     KIND_CODE_,     0, 1.0, false },
  { "material", KIND_MATERIAL_, 0, 1.0, false },
};

static const Key_ member_keys_[] =
{
  { "label",    KIND_LABEL_,    0, 1.0, false },
  { "section",  KIND_SECTION_,  0, 1.0, false },
  { "material", KIND_MATERIAL_, 0, 1.0, false },
  { "length",   KIND_LENGTH_,   0, 1e3, false },   // m -> mm

  NUMBER_ ( Lx,   1e3, false ),                    // m -> mm
  NUMBER_ ( Ly,   1e3, false ),
  NUMBER_ ( Lz,   1e3, false ),
  NUMBER_ ( Lb,   1e3, false ),
  NUMBER_ ( Kx,   1.0, false ),
  NUMBER_ ( Ky,   1.0, false ),
  NUMBER_ ( Kz,   1.0, false ),
  NUMBER_ ( Cb,   1.0, false ),
  NUMBER_ ( An,   1e2, false ),                    // cm^2 -> mm^2
  NUMBER_ ( Ct,   1.0, false ),
  NUMBER_ ( NSd,  1e3, true ),                     // kN -> N
  NUMBER_ ( MxSd, 1e6, true ),                     // kN m -> N mm
  NUMBER_ ( MySd, 1e6, true ),
  NUMBER_ ( VySd, 1e3, true ),
  NUMBER_ ( VxSd, 1e3, true ),
  NUMBER_ ( Mmax, 1e6, false ),                    // kN m -> N mm; the moment diagram for Cb
  NUMBER_ ( MA,   1e6, false ),
  NUMBER_ ( MB,   1e6, false ),
  NUMBER_ ( MC,   1e6, false ),
  NUMBER_ ( M1,   1e6, false ),                    // kN m -> N mm; Cb by 5.4.2.4 a)
  NUMBER_ ( M2,   1e6, false ),
  NUMBER_ ( MCL,  1e6, false ),
};

#undef NUMBER_

/*
 *  Positions of the keys that close_member_ needs by name: in top_keys_, then in member_keys_
 *  (keep them in step with the tables).
 */

enum : size_t
{
  TOP_MATERIAL_KEY_ = 1,

  SECTION_KEY_      = 1,
  MATERIAL_KEY_     = 2,
  LENGTH_KEY_       = 3,
  CB_KEY_           = 11,
  MMAX_KEY_         = 19,
  MA_KEY_           = 20,
  MB_KEY_           = 21,
  MC_KEY_           = 22,
  M1_KEY_           = 23,
  M2_KEY_           = 24,
  MCL_KEY_          = 25,
};

constexpr size_t member_key_count_ = sizeof member_keys_ / sizeof member_keys_[0];

// Repeated keys are found with one bit per key in a uint64_t.

static_assert ( member_key_count_ <= 64, "one bit per key in a uint64_t" );

//==================================================================================================
//  The parser's state
//==================================================================================================

/*
 *  The member being read: what its [[member]] table gave so far. It becomes a SteelCheckMember
 *  when the table ends (at the next [[member]] or at the end of the text), when every key is
 *  known: the defaults depend on the length, which may come after the lengths it is a default of.
 */

typedef struct Pending_
{
  bool                      open;     // inside a [[member]] table
  bool                      failed;   // one of its keys had an error: it is not built
  size_t                    line;     // line of its [[member]] header
  uint64_t                  seen;     // bit k: member_keys_[k] was given
  char                      label[steelcheck_member_label_size];
  const SteelCheckSection*  section;
  const SteelCheckMaterial* material;
  double                    length;
  double                    values[member_key_count_];   // converted to internal units
} Pending_;

typedef struct Parser_
{
  SteelCheckInput*          input;
  const SteelCheckDatabase* db;
  const SteelCheckReporter* reporter;
  const SteelCheckMaterial* material;   // the top-level material, nullptr if not given
  uint64_t                  top_seen;   // bit k: top_keys_[k] was given
  size_t                    tables;     // [[member]] tables so far, the current one included
  Pending_                  member;
  size_t                    line;       // number of the line being read
  SteelCheckStatus          first;      // status of the first error
} Parser_;

/*
 *  A value as written after '=': a quoted string (with its escapes resolved), or a bare token,
 *  which should be a number.
 */

typedef struct Value_
{
  bool           is_string;
  char           text[value_size_];   // is_string: the string
  SteelCheckView token;               // otherwise: the bare token
  size_t         column;              // 1-based column of its first character
} Value_;

//==================================================================================================
//  Private helpers
//==================================================================================================

/*
 *  Reports an error at the current line and remembers the first one. An error inside a
 *  [[member]] table also marks the member, so that it is not built from incomplete data.
 */

STEELCHECK_PRINTF ( 4, 5 )
static void error_ ( Parser_* p, SteelCheckStatus status, size_t column, const char* format, ... )
{
  char message[steelcheck_diagnostic_message_size];

  va_list args;
  va_start ( args, format );
  vsnprintf ( message, sizeof message, format, args );
  va_end ( args );

  steelcheck_report ( p->reporter, STEELCHECK_SEVERITY_ERROR, status, p->line, column, "%s",
                      message );

  if ( p->first == STEELCHECK_OK )
  {
    p->first = status;
  }

  if ( p->member.open )
  {
    p->member.failed = true;
  }
}

/*
 *  The byte at pos, or '\0' past the end of the line: the line is a view, not a string.
 */

static char at_ ( SteelCheckView line, size_t pos )
{
  return pos < line.length ? line.data[pos] : '\0';
}

static size_t skip_blanks_ ( SteelCheckView line, size_t pos )
{
  while ( at_( line, pos ) == ' ' || at_( line, pos ) == '\t' )
  {
    pos++;
  }

  return pos;
}

/*
 *  True if nothing but blanks and a comment follows pos.
 */

static bool rest_is_empty_ ( SteelCheckView line, size_t pos )
{
  pos = skip_blanks_ ( line, pos );

  return pos >= line.length || line.data[pos] == '#';
}

static bool is_key_char_ ( char c )
{
  return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' )
         || c == '_' || c == '-';
}

/*
 *  Index of the key named name in keys, or -1.
 */

static int find_key_ ( const Key_* keys, size_t count, SteelCheckView name )
{
  for ( size_t k = 0; k < count; k++ )
  {
    if ( steelcheck_view_equals_cstr( name, keys[k].name ) )
    {
      return (int) k;
    }
  }

  return -1;
}

/*
 *  True if the member's table gave member_keys_[k].
 */

static bool given_ ( const Pending_* m, size_t k )
{
  return ( m->seen >> k ) & 1u;
}

/*
 *  Copies view into the '\0'-terminated text, if it fits.
 */

static bool copy_view_ ( char* text, size_t size, SteelCheckView view )
{
  if ( view.length >= size )
  {
    return false;
  }

  if ( view.length > 0 )
  {
    memcpy ( text, view.data, view.length );
  }

  text[view.length] = '\0';

  return true;
}

//==================================================================================================
//  Values
//==================================================================================================

/*
 *  Reads the quoted string that starts at *pos (at its quote) into value->text, and moves *pos
 *  past the closing quote. Double quotes resolve escapes; single quotes (TOML's literal strings)
 *  take every character as it is.
 */

static bool parse_string_ ( Parser_* p, SteelCheckView line, size_t* pos, Value_* value )
{
  char   quote  = at_ ( line, *pos );
  size_t i      = *pos + 1;
  size_t length = 0;

  if ( at_( line, i ) == quote && at_( line, i + 1 ) == quote )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, *pos + 1, "multi-line strings are not supported" );
    return false;
  }

  for ( ;; )
  {
    if ( i >= line.length )
    {
      error_ ( p, STEELCHECK_ERROR_SYNTAX, *pos + 1, "string without its closing %c", quote );
      return false;
    }

    char c = line.data[i++];

    if ( c == quote )
    {
      break;
    }

    if ( c == '\\' && quote == '"' )
    {
      char escape = at_ ( line, i );

      switch ( escape )
      {
        case '"':  c = '"';  break;
        case '\\': c = '\\'; break;
        case 't':  c = '\t'; break;
        case 'n':  c = '\n'; break;
        case 'r':  c = '\r'; break;

        default:
          error_ ( p, STEELCHECK_ERROR_SYNTAX, i, "unsupported escape sequence '\\%c'",
                   escape != '\0' ? escape : ' ' );
          return false;
      }

      i++;
    }

    if ( length + 1 >= sizeof value->text )
    {
      error_ ( p, STEELCHECK_ERROR_VALUE, *pos + 1, "string too long (at most %zu bytes)",
               sizeof value->text - 1 );
      return false;
    }

    value->text[length++] = c;
  }

  value->text[length] = '\0';
  value->is_string    = true;
  *pos                = i;

  return true;
}

/*
 *  Reads the value that starts at *pos: a quoted string, or a bare token up to a blank, a '#' or
 *  the end of the line.
 */

static bool parse_value_ ( Parser_* p, SteelCheckView line, size_t* pos, Value_* value )
{
  *value = (Value_) { .column = *pos + 1 };

  char c = at_ ( line, *pos );

  if ( c == '"' || c == '\'' )
  {
    return parse_string_ ( p, line, pos, value );
  }

  size_t start = *pos;

  while ( *pos < line.length && line.data[*pos] != ' ' && line.data[*pos] != '\t'
          && line.data[*pos] != '#' )
  {
    ( *pos )++;
  }

  if ( *pos == start )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, start + 1, "missing value after '='" );
    return false;
  }

  if ( c == '[' || c == '{' )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, start + 1, "arrays and inline tables are not supported" );
    return false;
  }

  value->token = (SteelCheckView) { .length = *pos - start, .data = &line.data[start] };

  return true;
}

/*
 *  The value as a string, or an error if it is a bare token.
 */

static const char* string_ ( Parser_* p, const char* key, const Value_* value )
{
  if ( !value->is_string )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, value->column,
             "'%s' needs a string in quotes, e.g. %s = \"%.*s\"", key, key,
             steelcheck_view_print_length( value->token ), value->token.data );
    return nullptr;
  }

  return value->text;
}

/*
 *  The value as a number, or an error if it is a string or not a valid number.
 */

static bool number_ ( Parser_* p, const char* key, const Value_* value, double* number )
{
  if ( value->is_string )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, value->column, "'%s' needs a number, not a string", key );
    return false;
  }

  SteelCheckStatus status = steelcheck_parse_double ( value->token, number );

  if ( status != STEELCHECK_OK )
  {
    error_ ( p, status, value->column, "%s for '%s': '%.*s'", steelcheck_status_message( status ),
             key, steelcheck_view_print_length( value->token ), value->token.data );
    return false;
  }

  return true;
}

/*
 *  The built-in grade named by the value, or an error.
 */

static const SteelCheckMaterial* material_ ( Parser_* p, const Value_* value )
{
  const char* name = string_ ( p, "material", value );

  if ( name == nullptr )
  {
    return nullptr;
  }

  const SteelCheckMaterial* material = steelcheck_material_find ( name );

  if ( material == nullptr )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, value->column, "unknown material '%s'", name );
  }

  return material;
}

//==================================================================================================
//  Keys
//==================================================================================================

/*
 *  A key before the first [[member]] table.
 */

static void top_key_ ( Parser_* p, SteelCheckView name, size_t column, const Value_* value )
{
  int k = find_key_ ( top_keys_, sizeof top_keys_ / sizeof top_keys_[0], name );

  if ( k < 0 )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, column, "unknown key '%.*s' (expected code, material "
             "or a [[member]] table)", steelcheck_view_print_length( name ), name.data );
    return;
  }

  uint64_t bit = UINT64_C( 1 ) << k;

  if ( p->top_seen & bit )
  {
    error_ ( p, STEELCHECK_ERROR_DUPLICATE, column, "key '%s' given twice", top_keys_[k].name );
    return;
  }

  p->top_seen |= bit;

  if ( top_keys_[k].kind == KIND_MATERIAL_ )
  {
    p->material = material_ ( p, value );
    return;
  }

  // KIND_CODE_

  const char* id = string_ ( p, "code", value );

  if ( id == nullptr )
  {
    return;
  }

  p->input->code = steelcheck_code_find ( id );

  if ( p->input->code == nullptr )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, value->column, "unknown design code '%s'", id );
  }
}

/*
 *  A key inside a [[member]] table.
 */

static void member_key_ ( Parser_* p, SteelCheckView name, size_t column, const Value_* value )
{
  Pending_* m = &p->member;
  int       k = find_key_ ( member_keys_, member_key_count_, name );

  if ( k < 0 )
  {
    // A top-level key here belongs to the table, by TOML's rules: say where it should go.

    bool top = find_key_( top_keys_, sizeof top_keys_ / sizeof top_keys_[0], name ) >= 0;

    error_ ( p, STEELCHECK_ERROR_VALUE, column, "unknown key '%.*s' in [[member]]%s",
             steelcheck_view_print_length( name ), name.data,
             top ? " (top-level keys must come before the first [[member]])" : "" );
    return;
  }

  const Key_* key = &member_keys_[k];
  uint64_t    bit = UINT64_C( 1 ) << k;

  if ( m->seen & bit )
  {
    error_ ( p, STEELCHECK_ERROR_DUPLICATE, column, "key '%s' given twice in this member",
             key->name );
    return;
  }

  m->seen |= bit;

  switch ( key->kind )
  {
    case KIND_LABEL_:
    {
      const char* label = string_ ( p, key->name, value );

      if ( label != nullptr && !copy_view_( m->label, sizeof m->label,
                                            steelcheck_view_from_cstr( label ) ) )
      {
        error_ ( p, STEELCHECK_ERROR_VALUE, value->column, "label too long (at most %zu bytes)",
                 sizeof m->label - 1 );
      }
      break;
    }

    case KIND_SECTION_:
    {
      const char* designation = string_ ( p, key->name, value );

      if ( designation == nullptr )
      {
        break;
      }

      m->section = steelcheck_database_find ( p->db, designation );

      if ( m->section == nullptr )
      {
        error_ ( p, STEELCHECK_ERROR_VALUE, value->column, "no section '%s' in the database",
                 designation );
      }
      break;
    }

    case KIND_MATERIAL_:
      m->material = material_ ( p, value );
      break;

    case KIND_LENGTH_:
    case KIND_NUMBER_:
    {
      double number = 0.0;

      if ( number_( p, key->name, value, &number ) )
      {
        m->values[k] = number * key->scale;
      }
      break;
    }

    case KIND_CODE_:
      break;
  }
}

//==================================================================================================
//  Members
//==================================================================================================

/*
 *  A reporter that forwards to another one, adding a line number to diagnostics without one:
 *  steelcheck_member_validate knows nothing about files, but its errors are best reported at the
 *  member's [[member]] line.
 */

typedef struct AtLine_
{
  const SteelCheckReporter* target;
  size_t                    line;
} AtLine_;

static void at_line_ ( void* context, const SteelCheckDiagnostic* diagnostic )
{
  const AtLine_*       at   = context;
  SteelCheckDiagnostic copy = *diagnostic;

  if ( copy.line == 0 )
  {
    copy.line = at->line;
  }

  if ( at->target != nullptr && at->target->report != nullptr )
  {
    at->target->report ( at->target->context, &copy );
  }
}

/*
 *  Makes room for at least one more member (see grow_ in database.c).
 */

static SteelCheckStatus grow_ ( SteelCheckInput* input )
{
  size_t capacity = initial_capacity_;
  size_t bytes    = 0;

  if ( input->capacity > 0 && ckd_mul( &capacity, input->capacity, 2 ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  if ( ckd_mul( &bytes, capacity, sizeof *input->members ) )
  {
    return STEELCHECK_ERROR_OVERFLOW;
  }

  SteelCheckMember* members = realloc ( input->members, bytes );

  if ( members == nullptr )
  {
    return STEELCHECK_ERROR_NO_MEMORY;
  }

  input->members  = members;
  input->capacity = capacity;

  return STEELCHECK_OK;
}

/*
 *  Ends the [[member]] table being read: builds the member from its keys, validates it and adds
 *  it to the input. Returns STEELCHECK_OK also for a member with errors (they are reported and
 *  remembered in p->first); only out of memory and overflow stop the parser.
 */

static SteelCheckStatus close_member_ ( Parser_* p )
{
  Pending_* m = &p->member;

  if ( !m->open )
  {
    return STEELCHECK_OK;
  }

  // The errors are reported at the [[member]] line from here on.

  p->line = m->line;

  size_t index = p->tables;
  bool   ready = !m->failed;

  if ( !given_( m, SECTION_KEY_ ) )
  {
    error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0, "member %zu has no 'section'", index );
    ready = false;
  }

  if ( !given_( m, LENGTH_KEY_ ) )
  {
    error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0, "member %zu has no 'length' [m]", index );
    ready = false;
  }

  const SteelCheckMaterial* material = m->material != nullptr ? m->material : p->material;

  // A material given but unknown has been reported already, at its own line.

  bool top_material = ( p->top_seen >> TOP_MATERIAL_KEY_ ) & 1u;

  if ( material == nullptr )
  {
    if ( !given_( m, MATERIAL_KEY_ ) && !top_material )
    {
      error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0,
               "member %zu has no 'material', and there is no top-level one", index );
    }

    ready = false;
  }

  // The moment diagram for Cb: MA, MB and MC together, Mmax only with them, and not with Cb.

  int diagram = given_( m, MA_KEY_ ) + given_( m, MB_KEY_ ) + given_( m, MC_KEY_ );

  if ( diagram > 0 && diagram < 3 )
  {
    error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0,
             "member %zu: give MA, MB and MC together (moments for Cb, 5.4.2.3)", index );
    ready = false;
  }
  else if ( diagram == 0 && given_( m, MMAX_KEY_ ) )
  {
    error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0, "member %zu: Mmax needs MA, MB and MC",
             index );
    ready = false;
  }

  if ( diagram > 0 && given_( m, CB_KEY_ ) )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, 0,
             "member %zu: give either Cb or MA, MB and MC, not both", index );
    ready = false;
  }

  // The moments of a member with one flange continuously restrained (5.4.2.4 a)): M1, M2 and MCL
  // together, and not with Cb or MA, MB, MC.

  int free_flange = given_( m, M1_KEY_ ) + given_( m, M2_KEY_ ) + given_( m, MCL_KEY_ );

  if ( free_flange > 0 && free_flange < 3 )
  {
    error_ ( p, STEELCHECK_ERROR_MISSING_VALUE, 0,
             "member %zu: give M1, M2 and MCL together (moments for Cb, 5.4.2.4 a))", index );
    ready = false;
  }

  if ( free_flange > 0 && ( diagram > 0 || given_( m, CB_KEY_ ) ) )
  {
    error_ ( p, STEELCHECK_ERROR_VALUE, 0,
             "member %zu: give only one of Cb, MA-MB-MC and M1-M2-MCL", index );
    ready = false;
  }

  if ( !ready )
  {
    *m = (Pending_) {};
    return STEELCHECK_OK;
  }

  // Defaults first (from the length), then every number the table gave.

  SteelCheckMember member = {};

  if ( m->label[0] == '\0' )
  {
    snprintf ( m->label, sizeof m->label, "M%zu", index );
  }

  SteelCheckStatus status = steelcheck_member_init ( &member, m->label, m->section, material,
                                                     m->values[LENGTH_KEY_] );

  if ( status != STEELCHECK_OK )
  {
    *m = (Pending_) {};
    return status;
  }

  for ( size_t k = 0; k < member_key_count_; k++ )
  {
    const Key_* key = &member_keys_[k];

    if ( key->kind != KIND_NUMBER_ || !given_( m, k ) )
    {
      continue;
    }

    // The double member at its offset (see number_slot_ in section.c).

    *(double*) ( (unsigned char*) &member + key->offset ) = m->values[k];

    if ( key->action )
    {
      member.has_actions = true;
    }
  }

  member.has_moments     = given_ ( m, MA_KEY_ );
  member.has_free_flange = given_ ( m, M1_KEY_ );

  AtLine_            at       = { .target = p->reporter, .line = m->line };
  SteelCheckReporter reporter = { .report = at_line_, .context = &at };

  *m = (Pending_) {};

  if ( steelcheck_member_validate( &member, &reporter ) != STEELCHECK_OK )
  {
    if ( p->first == STEELCHECK_OK )
    {
      p->first = STEELCHECK_ERROR_VALUE;
    }

    return STEELCHECK_OK;
  }

  if ( p->input->count == p->input->capacity )
  {
    status = grow_ ( p->input );

    if ( status != STEELCHECK_OK )
    {
      return status;
    }
  }

  p->input->members[p->input->count++] = member;

  return STEELCHECK_OK;
}

//==================================================================================================
//  Lines
//==================================================================================================

/*
 *  A line that starts with '[': only "[[member]]" is accepted.
 */

static SteelCheckStatus table_ ( Parser_* p, SteelCheckView line, size_t pos )
{
  size_t start = pos;

  if ( at_( line, pos + 1 ) != '[' )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, start + 1,
             "unsupported table (only [[member]] tables are supported)" );
    return STEELCHECK_OK;
  }

  pos = skip_blanks_ ( line, pos + 2 );

  size_t name_start = pos;

  while ( is_key_char_( at_( line, pos ) ) )
  {
    pos++;
  }

  SteelCheckView name = { .length = pos - name_start, .data = &line.data[name_start] };

  pos = skip_blanks_ ( line, pos );

  if ( at_( line, pos ) != ']' || at_( line, pos + 1 ) != ']' || !rest_is_empty_( line, pos + 2 ) )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, start + 1, "malformed table header (expected "
             "[[member]])" );
    return STEELCHECK_OK;
  }

  if ( !steelcheck_view_equals_cstr( name, "member" ) )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, name_start + 1, "unsupported table [[%.*s]] (only "
             "[[member]] tables are supported)", steelcheck_view_print_length( name ), name.data );
    return STEELCHECK_OK;
  }

  // The previous member ends here; this one starts.

  size_t line_number = p->line;

  SteelCheckStatus status = close_member_ ( p );

  p->line   = line_number;
  p->member = (Pending_) { .open = true, .line = line_number };
  p->tables++;

  return status;
}

/*
 *  A line of the form key = value [# comment].
 */

static void key_value_ ( Parser_* p, SteelCheckView line, size_t pos )
{
  size_t key_start = pos;

  while ( is_key_char_( at_( line, pos ) ) )
  {
    pos++;
  }

  if ( pos == key_start )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, pos + 1, "expected a key (letters, digits, '_', '-')" );
    return;
  }

  SteelCheckView name = { .length = pos - key_start, .data = &line.data[key_start] };

  pos = skip_blanks_ ( line, pos );

  if ( at_( line, pos ) != '=' )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, pos + 1, "expected '=' after the key '%.*s'",
             steelcheck_view_print_length( name ), name.data );
    return;
  }

  pos = skip_blanks_ ( line, pos + 1 );

  Value_ value = {};

  if ( !parse_value_( p, line, &pos, &value ) )
  {
    return;
  }

  if ( !rest_is_empty_( line, pos ) )
  {
    error_ ( p, STEELCHECK_ERROR_SYNTAX, skip_blanks_( line, pos ) + 1,
             "unexpected text after the value" );
    return;
  }

  if ( p->member.open )
  {
    member_key_ ( p, name, key_start + 1, &value );
  }
  else
  {
    top_key_ ( p, name, key_start + 1, &value );
  }
}

//==================================================================================================
//  steelcheck_input_load
//==================================================================================================

SteelCheckStatus steelcheck_input_load ( SteelCheckInput* input, SteelCheckView text,
                                         const SteelCheckDatabase* db,
                                         const SteelCheckReporter* reporter )
{
  if ( input == nullptr || db == nullptr || input->members != nullptr || input->count != 0 )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  *input = (SteelCheckInput) { .code = steelcheck_code_default() };

  Parser_ parser =
  {
    .input    = input,
    .db       = db,
    .reporter = reporter,
    .first    = STEELCHECK_OK,
  };

  SteelCheckLines  lines  = {};
  SteelCheckView   line   = {};
  SteelCheckStatus status = STEELCHECK_OK;

  steelcheck_lines_init ( &lines, text );

  while ( status == STEELCHECK_OK && steelcheck_lines_next( &lines, &line, &parser.line ) )
  {
    size_t pos = skip_blanks_ ( line, 0 );

    if ( rest_is_empty_( line, pos ) )
    {
      continue;
    }

    if ( line.data[pos] == '[' )
    {
      status = table_ ( &parser, line, pos );
    }
    else
    {
      key_value_ ( &parser, line, pos );
    }
  }

  if ( status == STEELCHECK_OK )
  {
    status = close_member_ ( &parser );
  }

  // Out of memory or overflow: nothing more can be done.

  if ( status != STEELCHECK_OK )
  {
    steelcheck_input_free ( input );
    return status;
  }

  if ( parser.first == STEELCHECK_OK && input->count == 0 )
  {
    parser.line = 0;
    error_ ( &parser, STEELCHECK_ERROR_MISSING_VALUE, 0, "no [[member]] tables" );
  }

  // Every line has been read and every problem reported; one error rejects the whole text.

  if ( parser.first != STEELCHECK_OK )
  {
    steelcheck_input_free ( input );
    return parser.first;
  }

  return STEELCHECK_OK;
}

//==================================================================================================
//  steelcheck_input_load_file
//==================================================================================================

SteelCheckStatus steelcheck_input_load_file ( SteelCheckInput* input, const char* path,
                                              const SteelCheckDatabase* db,
                                              const SteelCheckReporter* reporter )
{
  if ( input == nullptr || path == nullptr || db == nullptr || input->members != nullptr
       || input->count != 0 )
  {
    return STEELCHECK_ERROR_INVALID_ARGUMENT;
  }

  SteelCheckBuffer buff   = {};
  SteelCheckStatus status = steelcheck_buffer_load ( &buff, path );

  if ( status != STEELCHECK_OK )
  {
    steelcheck_report ( reporter, STEELCHECK_SEVERITY_ERROR, status, 0, 0, "%s",
                        steelcheck_status_message( status ) );
    return status;
  }

  // The members copy their labels and point into the database, not into the text, so the buffer
  // can be freed right after loading.

  status = steelcheck_input_load ( input, steelcheck_buffer_view( &buff ), db, reporter );

  steelcheck_buffer_free ( &buff );

  return status;
}

//==================================================================================================
//  steelcheck_input_free
//==================================================================================================

void steelcheck_input_free ( SteelCheckInput* input )
{
  if ( input == nullptr )
  {
    return;
  }

  free ( input->members );

  *input = (SteelCheckInput) {};
}
