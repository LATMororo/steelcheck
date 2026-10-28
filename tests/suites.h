#ifndef STEELCHECK_SUITES_H
#define STEELCHECK_SUITES_H

/*
 *  One public function per test file; each runs the (static) tests of that file. Called by
 *  test_main.c. Tests that read files use paths relative to the project root, where make test
 *  runs the test runner.
 */

void test_framework_suite ();
void test_version_suite   ();
void test_status_suite    ();
void test_buffer_suite    ();
void test_lines_suite     ();
void test_view_suite      ();

#endif // STEELCHECK_SUITES_H
