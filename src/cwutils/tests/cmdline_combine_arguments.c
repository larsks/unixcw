/*
  Copyright (C) 2022-2023  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program. If not, see <https://www.gnu.org/licenses/>.
*/




#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <cwutils/cw_cmdline.h>

#include "cmdline_combine_arguments.h"




/*
  Sizes of argv[] arrays. Output is larger because in addition to copy of
  elements from input argv[], it has to also store options taken from
  environment variable.
*/
#define INPUT_ARGV_SIZE  12
#define OUTPUT_ARGV_SIZE 24




// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding) // disable clang-tidy's specific test
static const struct {
	int in_argc;                                   /**< argc passed to program's main() function */
	const char * in_argv[INPUT_ARGV_SIZE];         /**< argv[] passed to program's main() function, to be combined into final result. */
	const char * env_name;                         /**< Name of environment variable from which to get options to be combined into final result. */
	const char * env_value;                        /**< Value of environment variable with value of options to be combined into final result. */
	int expected_argc;                             /**< Expected count of elements in combined argv (in expected_argv[]). */
	const char * expected_argv[OUTPUT_ARGV_SIZE];  /**< Expected combined argv[] (array of options), a result of combining input argv[] and env variable's value. */
} test_data[] = {
	/* Just program name. */
	{ .in_argc = 1,
	  .in_argv = { "progname" },
	  .env_name = "",  /* TODO acerion 2023.09.02: tested function can't handle NULL env name. Fix this in future, and add test code for it. */
	  .env_value = "",
	  .expected_argc = 1,
	  .expected_argv = { "progname" }
	},



	/* Program name + one arg in command line. */
	{ .in_argc = 2,
	  .in_argv = { "progname", "-t=500"},
	  .env_name = "",
	  .env_value = "",
	  .expected_argc = 2,
	  .expected_argv = { "progname", "-t=500" }
	},
	/* Program name + multiple args in command line. */
	{ .in_argc = 5,
	  .in_argv = { "progname", "-t=500", "--weighting", "40", "--wpm=30" },
	  .env_name = "",
	  .env_value = "",
	  .expected_argc = 5,
	  .expected_argv = { "progname", "-t=500", "--weighting", "40", "--wpm=30" }
	},



	/* Program name + one arg in env variable. */
	{ .in_argc = 1,
	  .in_argv = { "progname" },
	  .env_name = "TEST_CW_OPTIONS_A",
	  .env_value = "--tone=1000",
	  .expected_argc = 2,
	  .expected_argv = { "progname", "--tone=1000" }
	},
	/* Program name + multiple args in env variable. */
	{ .in_argc = 1,
	  .in_argv = { "progname", },
	  .env_name = "TEST_CW_OPTIONS_B",
	  .env_value = "--tone=1000 -w 20 --gap=2",
	  .expected_argc = 5,
	  .expected_argv = { "progname", "--tone=1000", "-w", "20", "--gap=2" }
	},



	/* Program name + one arg in env variable + one arg in command line. */
	{ .in_argc = 2,
	  .in_argv = { "progname", "-t=500" },
	  .env_name = "TEST_CW_OPTIONS_C",
	  .env_value = "--tone=1000",
	  .expected_argc = 3,
	  .expected_argv = { "progname", "--tone=1000", "-t=500" }
	},
	/* Program name + multiple args in env variable + multiple args in command line. */
	{ .in_argc = 5,
	  .in_argv = { "progname", "-t=500", "--weighting", "40", "--wpm=30" },
	  .env_name = "TEST_CW_OPTIONS_D",
	  .env_value = "--tone=1000 -w 20",
	  .expected_argc = 8,
	  .expected_argv = { "progname", "--tone=1000", "-w", "20", "-t=500", "--weighting", "40", "--wpm=30" }
	}
};




int test_combine_arguments(void)
{
	const size_t n_tests = sizeof (test_data) / sizeof (test_data[0]);
	int errors = 0;
	for (size_t test = 0; test < n_tests; test++) {

		/* Variables that will be storing tested function's result, and that
		   will be compared with expected result. */
		char ** result_argv = NULL;
		int result_argc = 0;

		cw_ret_t cwretv = CW_FAILURE;

		/* TODO acerion 2023.09.02: this re-packing of argv is needed due to
		   different types (with/without const qualifier) in different places
		   of code. Try to unify the types to avoid this repackaging. */
		char * alloced_argv[OUTPUT_ARGV_SIZE] = { 0 };
		for (int arg = 0; arg < test_data[test].in_argc; arg++) {
			alloced_argv[arg] = strdup(test_data[test].in_argv[arg]);
		}
		if (0 != strlen(test_data[test].env_name)) {
			if (0 != setenv(test_data[test].env_name, test_data[test].env_value, 0)) {
				fprintf(stderr, "[ERROR] setenv(%s, %s, 0) has failed: %s\n",
				        test_data[test].env_name, test_data[test].env_value,
				        strerror(errno));
				errors++;
				goto CLEANUP;
			}
		}

		/* Tested function. */
		cwretv = combine_arguments(test_data[test].env_name,
		                           test_data[test].in_argc,
		                           alloced_argv,
		                           &result_argc, &result_argv);

		if (CW_FAILURE == cwretv) {
			fprintf(stderr, "[ERROR] Test %2zu: tested function returns failure\n", test);
			errors++;
			goto CLEANUP;
		}
		if (result_argc != test_data[test].expected_argc) {
			fprintf(stderr, "[ERROR] Test %2zu: mismatched argc: result = %d, expected = %d\n",
			        test, result_argc, test_data[test].expected_argc);
			errors++;
			goto CLEANUP;
		}
		/* Compare each slot, element by element. Also check those elements
		   that should be empty! */
		for (int arg = 0; arg < OUTPUT_ARGV_SIZE; arg++) {
			if (arg < result_argc) {
				/* This slot in both arrays should be a valid pointer to
				   string. */
				if (0 != strcmp(result_argv[arg], test_data[test].expected_argv[arg])) {
					fprintf(stderr, "[ERROR] test %2zu, mismatched arg %2d: result = [%s], expected = [%s]\n",
					        test, arg, result_argv[arg], test_data[test].expected_argv[arg]);
					errors++;
					goto CLEANUP;
				}
			} else {
#if 0
				/* Don't check result_argv[arg] because at this point 'arg'
				   would be indexing the argv array beyond allocated area. */
				if (NULL != result_argv[arg]) {
					fprintf(stderr, "[ERROR] test %2zu, result argv[%d] is unexpectedly non-NULL: [%s]\n",
					        test, arg, result_argv[arg]);
					errors++;
					goto CLEANUP;
				}
#endif
				/* This test doesn't validate production code, but it
				   validates a test code. It may or may not catch errors in
				   test code. It's safe to leave it here as it is. */
				if (NULL != test_data[test].expected_argv[arg]) {
					fprintf(stderr, "[ERROR] test %2zu, expected argv[%d] is unexpectedly non-NULL: [%s]\n",
					        test, arg, test_data[test].expected_argv[arg]);
					errors++;
					goto CLEANUP;
				}
			}
			if (0) { /* For debugging only. */
				fprintf(stderr, "[DEBUG] Test %2zu, arg %2d: result argv = [%s], expected argv = [%s]\n",
				        test, arg, result_argv[arg], test_data[test].expected_argv[arg]);
			}
		}

	CLEANUP:
		for (int arg = 0; arg < test_data[test].in_argc; arg++) {
			free(alloced_argv[arg]);
			alloced_argv[arg] = NULL;
		}
		if (0 != strlen(test_data[test].env_name)) {
			/* Leave the env clean. */
			if (0 != unsetenv(test_data[test].env_name)) {
				fprintf(stderr, "[ERROR] unsetenv(%s) has failed: %s\n",
				        test_data[test].env_name,
				        strerror(errno));
			}
		}
	}

	if (errors) {
		return -1;
	} else {
		return 0;
	}
}

