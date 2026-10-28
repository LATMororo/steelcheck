#include "steelcheck/compat.h"
#include "steelcheck/status.h"
#include "suites.h"
#include "test.h"

#include <string.h>

static void test_status_messages ()
{
  // Every known code has its own non-empty message; -Wswitch already checks this at compile
  // time, the test documents it.

  constexpr SteelCheckStatus codes[] =
  {
    STEELCHECK_OK,
    STEELCHECK_ERROR_OPEN,
    STEELCHECK_ERROR_READ,
    STEELCHECK_ERROR_TOO_LARGE,
    STEELCHECK_ERROR_NO_MEMORY,
    STEELCHECK_ERROR_OVERFLOW,
    STEELCHECK_ERROR_INVALID_ARGUMENT,
    STEELCHECK_ERROR_SYNTAX,
    STEELCHECK_ERROR_HEADER,
    STEELCHECK_ERROR_MISSING_VALUE,
    STEELCHECK_ERROR_NUMBER,
    STEELCHECK_ERROR_RANGE,
    STEELCHECK_ERROR_VALUE,
    STEELCHECK_ERROR_INCONSISTENT,
  };

  for ( size_t i = 0; i < sizeof codes / sizeof codes[0]; i++ )
  {
    const char* message = steelcheck_status_message ( codes[i] );

    CHECK ( message != nullptr && message[0] != '\0' );
    CHECK ( strcmp( message, "unknown status" ) != 0 );
  }

  CHECK_STR_EQ ( steelcheck_status_message( STEELCHECK_ERROR_OPEN ), "cannot open file" );
}

static void test_status_unknown ()
{
  // A fixed underlying type allows any int to be stored, not only the listed codes.

  CHECK_STR_EQ ( steelcheck_status_message( (SteelCheckStatus) 999 ), "unknown status" );
}

//==================================================================================================
//  test_status_suite
//==================================================================================================

void test_status_suite ()
{
  RUN_TEST ( test_status_messages );
  RUN_TEST ( test_status_unknown );
}
