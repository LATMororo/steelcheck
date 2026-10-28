#include "steelcheck/compat.h"
#include "suites.h"
#include "test.h"

//==================================================================================================
//  main
//==================================================================================================

int main ()
{
  test_framework_suite ();
  test_version_suite   ();
  test_status_suite    ();
  test_buffer_suite    ();
  test_lines_suite     ();
  test_view_suite      ();

  return test_summary ();
}
