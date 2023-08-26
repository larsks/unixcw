#ifndef CW_REC_TESTER_H
#define CW_REC_TESTER_H




#include <stdint.h>
#include "../libcw/libcw_key.h"
#include <test_framework/basic_utils/param_ranger.h>
#include <test_framework/basic_utils/test_result.h>

#include "cw_easy_legacy_receiver.h"




#if defined(__cplusplus)
extern "C"
{
#endif




/**
   Used to determine size of input data and of buffer for received
   (polled from receiver) characters.
*/
#define REC_TEST_BUFFER_SIZE 4096




typedef struct cw_rec_tester_t {

	/* Whether generating timed events for receiver by test code
	   is in progress. */
	bool generating_in_progress;

	pthread_t receiver_test_code_thread_id;

	char input_string[REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t input_string_i;

	/* Array large enough to contain characters received (polled)
	   correctly and possible additional characters received
	   incorrectly. */
	char received_string[10 * REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t received_string_i;

	/* Generator used only to generate keying events at specific time
	   intervals. The events are received by tested receiver. */
	cw_gen_t * helper_gen;

	cw_key_t key;

	cwtest_param_ranger_t speed_ranger;

	/* Parameters used in "compare" function that verifies if
	   input and received strings are similar enough to pass the
	   test. */
	float acceptable_error_rate_percent; /* [percents] */
	size_t acceptable_last_mismatch_index;

	/*
	  How many characters to enqueue at once in helper generator each time
	  the helper generator's queue runs low?

	  Input variable for the test. Decreasing or increasing decides how many
	  characters are enqueued with the same speed S1. Next batch of
	  characters will be enqueued with another speed S2. Depending on how
	  long it will take to dequeue this batch, the difference between S2 and
	  S1 may be significant and this will throw receiver off.
	*/
	int characters_to_enqueue;

} cw_rec_tester_t;




/**
   @brief Initialize @p tester variable

   @reviewedon 2023.08.15

   @param[in/out] tester Pre-allocated tester variable to be initialized
*/
void cw_rec_tester_init(cw_rec_tester_t * tester);




/**
   @brief Configure a receiver's tester before using the tester

   @reviewedon 2023.08.15

   @param[in/out] tester Tester to configure
   @param[in] easy_rec Easy receiver that should be tested
   @param[in] use_ranger Whether to vary speed of test generator when generating Morse Code
*/
void cw_rec_tester_configure(cw_rec_tester_t * tester, cw_easy_legacy_receiver_t * easy_rec, bool use_ranger);




/**
   @brief Start tester of legacy receiver

   @reviewedon 2023.08.21

   @param[in/out] tester Tester of legacy receiver to start
 */
void cw_rec_tester_start_test_code(cw_rec_tester_t * tester);




/**
   @brief Stop tester of legacy receiver

   @reviewedon 2023.08.21

   @param[in/out] tester Tester of legacy receiver to stop
 */
void cw_rec_tester_stop_test_code(cw_rec_tester_t * tester);




/**
   @brief See how well the receiver has received the data

   Call this function at the end of receiver tests. Function compares buffers
   with text that was sent to test/helper generator and text that was
   received from tested receiver.

   Result of comparison is returned through @p test_passes: true if buffers
   are the same (or the same to a satisfying degree), false otherwise.

   @reviewedon 2023.08.26

   @param[in/out] tester Tester that was used during tests
   @param[out] test_passes Information whether test passes
*/
void cw_rec_tester_evaluate_receive_correctness(cw_rec_tester_t * tester, bool * test_passes);




/**
   @brief Pass to the tester a received character

   Let tester know that a tested receiver has received a non-inter-word-space
   character (i.e. character other than ' ' space).

   The tester also does an additional poll and a cross-check from the tested
   receiver to double-check the received @p erd.

   @reviewedon 2023.08.21

   @param[in/out] tester Tester of easy legacy receiver
   @param[in] erd Data received by tester easy legacy receiver
   @param[in] timeval Timestamp passed earlier to cw_receive_character()

   @return CW_SUCCESS if no error occurred during the additional poll or cross-check
   @return CW_FAILURE otherwise
*/
int cw_rec_tester_on_character(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer);




/**
   @brief Pass to the tester a received inter-word-space (' ' character)

   Let tester know that a tested receiver has received an inter-word-space
   character (i.e. ' ' character).

   The tester also does an additional poll and a cross-check from the tested
   receiver to double-check the received @p erd.

   @reviewedon 2023.08.21

   @param[in/out] tester Tester of easy legacy receiver
   @param[in] erd Data received by tester easy legacy receiver
   @param[in] timeval Timestamp passed earlier to cw_receive_character()

   @return CW_SUCCESS if no error occurred during the additional poll or cross-check
   @return CW_FAILURE otherwise
*/
int cw_rec_tester_on_space(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer);




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef CW_REC_TESTER_H */
