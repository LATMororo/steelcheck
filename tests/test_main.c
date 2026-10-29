#include "steelcheck/compat.h"
#include "suites.h"
#include "test.h"

//==================================================================================================
//  main
//==================================================================================================

int main ()
{
  test_framework_suite ();
  test_version_suite ();
  test_status_suite ();
  test_buffer_suite ();
  test_lines_suite ();
  test_view_suite ();
  test_csv_suite ();
  test_number_suite ();
  test_section_suite ();
  test_database_suite ();
  test_cli_suite ();
  test_member_suite ();
  test_result_suite ();
  test_nbr8800_suite ();
  test_report_suite ();
  test_input_suite ();

  return test_summary ();
}
