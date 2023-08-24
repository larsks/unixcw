#ifndef _TEST_FRAMEWORK_BASIC_UTILS_TEST_RESULT_H_
#define _TEST_FRAMEWORK_BASIC_UTILS_TEST_RESULT_H_




/**
   @brief Status of a test (unit test or other developer tests)
*/
typedef enum {
	/* Test has succeeded. */
	test_result_pass,

	/*
	  Test has failed because some expectation about behaviour of production
	  code was not met.
	*/
	test_result_fail
} test_result_t;





/**
   @brief Get string representing status of a test

   The string may contain escape codes that result in string being displayed
   in color.

   @reviewedon 2023.08.21

   @param[in] result Status/result of a test, for which to get a string representation

   @return String representing given @p result
*/
const char * get_test_result_string(test_result_t result);




#endif /* #ifndef _TEST_FRAMEWORK_BASIC_UTILS_TEST_RESULT_H_ */

