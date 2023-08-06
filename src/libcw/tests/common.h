#ifndef LIBCW_TESTS_COMMON_H
#define LIBCW_TESTS_COMMON_H




#include "libcw_gen.h"
#include "libcw_rec.h"
#include "test_framework.h"




/**
   @brief Test relations between duration parameters of a receiver

   Function looks at different members of @p params and validates that the
   relations between the values of different members are valid.

   Result of the test (pass/fail) is recorded in @p cte.

   @param[in/out] cte CW Test Executor variable
   @param[in] params Parameters to test
   @param[in] tolerance Receiver's tolerance (see libcw.h/cw_get_tolerance())

   @return 0 if test was executed to the end
   @return -1 if test was interrupted
*/
int test_rec_params_relations(cw_test_executor_t * cte, const cw_rec_parameters_t * params, int tolerance);




/**
   @brief Test relations between duration parameters of a generator

   Function looks at different members of @p params and validates that the
   relations between the values of different members are valid.

   Result of the test (pass/fail) is recorded in @p cte.

   @param[in/out] cte CW Test Executor variable
   @param[in] params Parameters to test
   @param[in] speed Generator's speed ([wpm])

   @return cwt_retv_ok if test was executed to the end
   @return cwt_retv_err if test was interrupted
*/
cwt_retv test_gen_params_relations(cw_test_executor_t * cte, const cw_gen_durations_t * params, int speed);




/**
   @brief Setup test environment for a test of legacy function

   @param start_gen whether a prepared generator should be started

   @reviewed on 2020-10-04

   @return 0 on success
   @return -1 on failure
*/
int legacy_api_standalone_test_setup(cw_test_executor_t * cte, bool start_gen);




/**
   @brief Deconfigure test environment after running a test of legacy function

   @reviewed on 2020-10-04
*/
int legacy_api_standalone_test_teardown(__attribute__((unused)) cw_test_executor_t * cte);




/**
   @brief Prepare new generator, possibly with parameter values passed through command line

   Test helper function.

   @reviewed on 2023-08-06

   @return 0 on success
   @return -1 on failure
*/
int helper_gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen);




/**
   @brief Delete @param gen, set the pointer to NULL

   Test helper function.

   @reviewed on 2023-08-06
*/
void helper_gen_destroy(cw_gen_t ** gen);




#endif /* #ifndef LIBCW_TESTS_COMMON_H */

