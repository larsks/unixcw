/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */




#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>




#include <cwutils/lib/random.h>

#include "libcw.h"
#include "libcw2.h"

#include "common.h"
#include "libcw_utils.h"
#include "libcw_tq.h"
#include "libcw_tq_internal.h"
#include "libcw_tq_tests.h"
#include "libcw_debug.h"
#include "test_framework.h"




#define MSG_PREFIX "libcw/tq: "




static int test_cw_tq_enqueue_internal(cw_test_executor_t * cte, cw_tone_queue_t * tq);
static int test_cw_tq_dequeue_internal(cw_test_executor_t * cte, cw_tone_queue_t * tq);


static void enqueue_tone_low_level(cw_test_executor_t * cte, cw_tone_queue_t * tq, const cw_tone_t * tone);
static cw_tone_queue_t * test_cw_tq_capacity_test_init(cw_test_executor_t * cte, size_t capacity, size_t high_water_mark, int head_shift);
static void test_helper_tq_callback(void * data);
static cwt_retv test_helper_fill_queue(cw_test_executor_t * cte, cw_tone_queue_t * tq, size_t count);




/**
   @reviewed on 2019-10-03
*/
int test_cw_tq_new_delete_internal(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	bool failure = false;
	cw_tone_queue_t * tq = NULL;

	for (int i = 0; i < loops; i++) {
		tq = LIBCW_TEST_FUT(cw_tq_new_internal)();
		if (!cte->expect_valid_pointer_errors_only(cte, tq, "creating new tone queue")) {
			failure = true;
			break;
		}


		/*
		  Try to access some fields in cw_tone_queue_t just to
		  be sure that the tq has been allocated properly.

		  Trying to read and write tq->head and tq->tail may
		  seem silly, but I just want to dereference tq
		  pointer and be sure that nothing crashes.
		*/
		{
			if (!cte->expect_op_int_errors_only(cte, 0, "==", tq->head, "trying to dereference tq (read ::head)")) {
				failure = true;
				break;
			}

			tq->tail = tq->head + 10;
			if (!cte->expect_op_int_errors_only(cte, 10, "==", tq->tail, "trying to dereference tq (read ::tail)")) {
				failure = true;
				break;
			}
		}


		LIBCW_TEST_FUT(cw_tq_delete_internal)(&tq);
		if (!cte->expect_null_pointer_errors_only(cte, tq, "deleting tone queue")) {
			failure = true;
			break;
		}
	}

	cte->expect_op_int(cte, false, "==", failure, "using tone queue's new/delete methods");

	/* Cleanup after (possibly) failed tests. */
	if (tq) {
		cw_tq_delete_internal(&tq);
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-03
*/
int test_cw_tq_capacity_internal(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	bool failure = false;
	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tone queue");

	for (int i = 0; i < loops; i++) {
		/* This is a silly test, but let's have any test of
		   the getter. TODO: come up with better test. */

		uint32_t intended_capacity = 0;
		cw_random_get_uint32(10, 4000, &intended_capacity);
		tq->capacity = (size_t) intended_capacity;

		const size_t readback_capacity = LIBCW_TEST_FUT(cw_tq_capacity_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte, intended_capacity, "==", readback_capacity, "getting tone queue capacity")) {
			failure = true;
			break;
		}
	}

	cw_tq_delete_internal(&tq);

	cte->expect_op_int(cte, false, "==", failure, "getting tone queue capacity");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-03
*/
int test_cw_tq_prev_index_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	if (NULL == tq) {
		kite_log(cte, LOG_ERR, "failed to create new tone queue");
		return -1;
	}

	struct {
		size_t current_index;
		size_t expected_prev_index;
		bool guard;
	} input[] = {
		{ tq->capacity - 4, tq->capacity - 5, false },
		{ tq->capacity - 3, tq->capacity - 4, false },
		{ tq->capacity - 2, tq->capacity - 3, false },
		{ tq->capacity - 1, tq->capacity - 2, false },

		/* This one should never happen. We can't pass index
		   equal "capacity" because it's out of range. */
		/*
		{ tq->capacity - 0, tq->capacity - 1, false },
		*/

		{                0, tq->capacity - 1, false },
		{                1,                0, false },
		{                2,                1, false },
		{                3,                2, false },
		{                4,                3, false },

		{                0,                0, true  } /* guard */
	};

	int i = 0;
	bool failure = false;
	while (!input[i].guard) {
		const size_t readback_prev_index = LIBCW_TEST_FUT(cw_tq_prev_index_internal)(tq, input[i].current_index);
		if (!cte->expect_op_int_errors_only(cte, input[i].expected_prev_index, "==", readback_prev_index, "calculating 'prev' index, test %d", i)) {
			failure = true;
			break;
		}
		i++;
	}

	cw_tq_delete_internal(&tq);

	cte->expect_op_int(cte, false, "==", failure, "calculating 'prev' index");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-03
*/
int test_cw_tq_next_index_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	if (NULL == tq) {
		kite_log(cte, LOG_ERR, "failed to create new tone queue");
		return -1;
	}

	struct {
		size_t current_index;
		size_t expected_next_index;
		bool guard;
	} input[] = {
		{ tq->capacity - 5, tq->capacity - 4, false },
		{ tq->capacity - 4, tq->capacity - 3, false },
		{ tq->capacity - 3, tq->capacity - 2, false },
		{ tq->capacity - 2, tq->capacity - 1, false },
		{ tq->capacity - 1,                0, false },
		{                0,                1, false },
		{                1,                2, false },
		{                2,                3, false },
		{                3,                4, false },

		{                0,                0, true  } /* guard */
	};

	int i = 0;
	bool failure = false;
	while (!input[i].guard) {
		const size_t readback_next_index = LIBCW_TEST_FUT(cw_tq_next_index_internal)(tq, input[i].current_index);
		if (!cte->expect_op_int_errors_only(cte, input[i].expected_next_index, "==", readback_next_index, "calculating 'next' index, test %d", i)) {
			failure = true;
			break;
		}
		i++;
	}

	cw_tq_delete_internal(&tq);

	cte->expect_op_int(cte, false, "==", failure, "calculating 'next' index");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Helper function, wrapper for some low-level operations.

   @reviewed on 2019-10-04
*/
void enqueue_tone_low_level(cw_test_executor_t * cte, cw_tone_queue_t * tq, const cw_tone_t * tone)
{
	/* This is just some code copied from implementation of
	   'enqueue' function. I don't use 'enqueue' function itself
	   because it's not tested yet. I get rid of all the other
	   code from the 'enqueue' function and use only the essential
	   part to manually add elements to list, and then to check
	   length of the list. */

	/* This block of code pretends to be enqueue function.  The
	   most important functionality of enqueue function is done
	   here manually. We don't do any checks of boundaries of tq,
	   we trust that this is enforced by for loop's conditions. */

	/* Notice that this is *before* enqueueing the tone. */
	cte->assert2(cte, tq->len < tq->capacity,
		     "length before enqueue reached capacity: %zu / %zu",
		     tq->len, tq->capacity);

	/* Enqueue the new tone and set the new tail index. */
	tq->queue[tq->tail] = *tone;
	tq->tail = cw_tq_next_index_internal(tq, tq->tail);
	tq->len++;

	cte->assert2(cte, tq->len <= tq->capacity,
		     "length after enqueue exceeded capacity: %zu / %zu",
		     tq->len, tq->capacity);
}




/**
   The second function is just a wrapper for the first one, so this
   test case tests both functions at once.

   @reviewed on 2019-10-04
*/
int test_cw_tq_length_internal_1(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tone queue");

	cw_tone_t tone;
	CW_TONE_INIT(&tone, 1, 1, CW_SLOPE_MODE_NO_SLOPES);

	bool failure = false;

	for (size_t i = 0; i < tq->capacity; i++) {

		enqueue_tone_low_level(cte, tq, &tone);

		/* OK, added a tone, ready to measure length of the queue. */
		const size_t expected_len = i + 1;
		const size_t readback_len = LIBCW_TEST_FUT(cw_tq_length_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte, expected_len, "==", readback_len, "tone queue length A, readback #1")) {
			failure = true;
			break;
		}
		if (!cte->expect_op_int_errors_only(cte, tq->len, "==", readback_len, "tone queue length A, readback #2")) {
			failure = true;
			break;
		}
	}

	cw_tq_delete_internal(&tq);

	cte->expect_op_int(cte, false, "==", failure, "tone queue length A");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
  \brief Wrapper for tests of enqueue() and dequeue() function

  First we fill a tone queue when testing enqueue(), and then use the
  filled tone queue to test dequeue().

  @reviewed 2020-10-03
*/
int test_cw_tq_enqueue_dequeue_internal(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);
	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tone queue");

	for (int i = 0; i < loops; i++) {

		/* Fill the tone queue with tones. */
		test_cw_tq_enqueue_internal(cte, tq);

		/* Use the same (now filled) tone queue to test dequeue()
		   function. */
		test_cw_tq_dequeue_internal(cte, tq);
	}

	cw_tq_delete_internal(&tq);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test enqueueing operation

   @reviewed 2020-10-02
*/
cwt_retv test_cw_tq_enqueue_internal(cw_test_executor_t * cte, cw_tone_queue_t * tq)
{
	/* At this point cw_tq_length_internal() should be
	   tested, so we can use it to verify correctness of 'enqueue'
	   function. */

	cw_tone_t tone;
	CW_TONE_INIT(&tone, 1, 1, CW_SLOPE_MODE_NO_SLOPES);
	bool enqueue_failure = false;
	bool length_failure = false;

	for (size_t i = 0; i < tq->capacity; i++) {

		/* This tests for potential problems with function call. */
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
		if (!cte->expect_op_int_errors_only(cte,
						    CW_SUCCESS, "==", cwret,
						    "%s:%d: enqueueing tone",
						    __func__, __LINE__)) {
			enqueue_failure = true;
			break;
		}

		/* This tests for correctness of working of the
		   'enqueue' function and of keeping track of tone
		   queue length. */
		const size_t expected_len = i + 1;
		const size_t readback_len = cw_tq_length_internal(tq);
		if (!cte->expect_op_int_errors_only(cte,
						    expected_len, "==", readback_len,
						    "%s:%d:readback #1",
						    __func__, __LINE__)) {
			length_failure = true;
			break;
		}
		if (!cte->expect_op_int_errors_only(cte,
						    tq->len, "==", readback_len,
						    "%s:%d: readback #2",
						    __func__, __LINE__)) {
			length_failure = true;
			break;
		}
	}

	cte->expect_op_int(cte, false, "==", enqueue_failure, "%s:%d: enqueueing", __func__, __LINE__);
	cte->expect_op_int(cte, false, "==", length_failure, "%s:%d: tone queue length", __func__, __LINE__);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @reviewed on 2019-10-04
*/
int test_cw_tq_dequeue_internal(cw_test_executor_t * cte, cw_tone_queue_t * tq)
{
	/* tq should be completely filled after tests of enqueue()
	   function. */

	/* Test some assertions about full tq, just to be sure. */
	cte->assert2(cte, tq->capacity == tq->len,
		     "dequeue: capacity != len of full queue: %zu != %zu",
		     tq->capacity, tq->len);

	cw_tone_t tone;
	CW_TONE_INIT(&tone, 1, 1, CW_SLOPE_MODE_NO_SLOPES);

	bool dequeue_failure = false;
	bool length_failure = false;

	for (size_t i = tq->capacity; i > 0; i--) {
		size_t expected_len;
		size_t readback_len;

		expected_len = i;
		readback_len = tq->len;
		/* Length of tone queue before dequeue. */
		if (!cte->expect_op_int_errors_only(cte, expected_len, "==", readback_len, "dequeue: length before dequeueing tone #%zu", i)) {
			length_failure = true;
			break;
		}

		const cw_queue_state_t queue_state = LIBCW_TEST_FUT(cw_tq_dequeue_internal)(tq, &tone);
		if (1 == i) {
			/* Dequeueing last tone from the queue. */
			if (!cte->expect_op_int_errors_only(cte, CW_TQ_JUST_EMPTIED, "==", queue_state, "dequeue: dequeueing tone #%zu", i)) {
				dequeue_failure = true;
				break;
			}
		} else {
			/* Dequeueing last tone from the queue. */
			if (!cte->expect_op_int_errors_only(cte, CW_TQ_NONEMPTY, "==", queue_state, "dequeue: dequeueing tone #%zu", i)) {
				dequeue_failure = true;
				break;
			}
		}

		/* Length of tone queue after dequeue. */
		expected_len = i - 1;
		readback_len = tq->len;
		if (!cte->expect_op_int_errors_only(cte, expected_len, "==", readback_len, "dequeue: length after dequeueing tone #%zu",  i)) {
			length_failure = true;
			break;
		}
	}

	cte->expect_op_int(cte, false, "==", dequeue_failure, "dequeue: dequeueing tones");
	cte->expect_op_int(cte, false, "==", length_failure, "dequeue: length of tq");


	return 0;
}




/**
   @brief Test 'is full' function while enqueueing tones to it

   Remember that the function checks whether tq is full, not whether
   it is non-empty.

   @reviewed 2020-10-03
*/
cwt_retv test_cw_tq_is_full_internal_while_enqueueing(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tq");
	bool failure = false;

	cw_tone_t tone;
	CW_TONE_INIT(&tone, 1, 1, CW_SLOPE_MODE_NO_SLOPES);

	for (size_t i = 0; i < tq->capacity; i++) {
		/* The 'enqueue' function is tested elsewhere, but it won't
		   hurt to check this simple condition here as well. */
		const cw_ret_t cwret = cw_tq_enqueue_internal(tq, &tone);
		if (!cte->expect_op_int_errors_only(cte,
						    CW_SUCCESS, "==", cwret,
						    "%s:%d: enqueuing tone #%zu",
						    __func__, __LINE__, i)) {
			failure = true;
			break;
		}

		/* The last tone will make the queue full. */
		const bool expected_value = i == (tq->capacity - 1);
		const bool is_full = LIBCW_TEST_FUT(cw_tq_is_full_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte,
						    expected_value, "==", is_full,
						    "%s:%d: is tone queue full after enqueueing tone #%zu",
						    __func__, __LINE__, i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte,
			   false, "==", failure,
			   "%s:%d: 'full' state during enqueueing",
			   __func__, __LINE__);

	cw_tq_delete_internal(&tq);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Test 'is full' function while dequeueing tones from it

   Remember that the function checks whether tq is full, not whether
   it is non-empty.

   @reviewed 2020-10-03
*/
cwt_retv test_cw_tq_is_full_internal_while_dequeueing(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tq");
	bool failure = false;

	/* Fill tone queue completely. */
	if (cwt_retv_ok != test_helper_fill_queue(cte, tq, tq->capacity)) {
		cte->log_error(cte, "%s:%d: failed to fill tone queue\n", __func__, __LINE__);
		return cwt_retv_err;
	}

	/* First test the function on filled and not-dequeued-from queue. */
	bool is_full = LIBCW_TEST_FUT(cw_tq_is_full_internal)(tq);
	if (!cte->expect_op_int(cte,
				true, "==", is_full,
				"%s:%d: queue should be full after completely filling it",
				__func__, __LINE__)) {
	}

	/* Now test the 'is full' function as we dequeue ALL tones. */
	for (size_t i = tq->capacity; i > 0; i--) {
		/* The 'dequeue' function has been already tested, but it
		   won't hurt to check this simple condition here as
		   well. The dequeue operation may return CW_TQ_JUST_EMPTIED
		   for last tone, but it shouldn't return CW_TQ_EMPTY. */
		cw_tone_t tone;
		const cw_queue_state_t queue_state = LIBCW_TEST_FUT(cw_tq_dequeue_internal)(tq, &tone);
		if (!cte->expect_op_int_errors_only(cte,
						    CW_TQ_EMPTY, "!=", queue_state,
						    "%s:%d: dequeueing tone #%zd",
						    __func__, __LINE__, i)) {
			failure = true;
			break;
		}

		/* Here is the proper test of tested function. Since
		   we have called "dequeue" above, the queue becomes
		   non-full during first iteration. */
		is_full = LIBCW_TEST_FUT(cw_tq_is_full_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte,
						    false, "==", is_full,
						    "%s:%d: queue should not be full after dequeueing tone %zd",
						    __func__, __LINE__, i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte,
			   false, "==", failure,
			   "%s:%d: 'full' state during dequeueing",
			   __func__, __LINE__);


	cw_tq_delete_internal(&tq);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test "capacity" property of tone queue

   Function tests "capacity" property of tone queue, and also tests
   related properties: head and tail.

   Just like in test_cw_tq_test_capacity_B(), enqueueing is done with
   cw_tq_enqueue_internal().

   Unlike test_cw_tq_test_capacity_B(), this function dequeues tones
   using "manual" method.

   After every dequeue we check that dequeued tone is the one that we
   were expecting to get.

   @reviewed on 2019-10-04
*/
int test_cw_tq_test_capacity_A(cw_test_executor_t * cte)
{
	/* We don't need to check tq with capacity ==
	   CW_TONE_QUEUE_CAPACITY_MAX (yet). Let's test a smaller
	   queue capacity. */
	uint32_t x_random = 0;
	cw_random_get_uint32(30, 70, &x_random);
	const size_t capacity = (size_t) x_random;
	const size_t watermark = capacity - (capacity * 0.2);

	cte->print_test_header(cte, "%s (%zu)", __func__, capacity);

	/* We will do tests of queue with constant capacity, but with
	   different initial position at which we insert first element
	   (tone), i.e. different position of queue's head.

	   Elements of the array should be no larger than capacity. -1
	   is a guard.

	   TODO: allow negative head shifts in the test. */
	const int head_shifts[] = { 0, 5, 10, 29, -1, 30, -1 };
	int shift_idx = 0;

	while (head_shifts[shift_idx] != -1) {

		bool enqueue_failure = false;
		bool dequeue_failure = false;

		const int current_head_shift = head_shifts[shift_idx];

		cte->log_info_cont(cte, "\n");
		cte->log_info(cte, "Testing with head shift = %d\n", current_head_shift);

		/* For every new test with new head shift we need a
		   "clean" queue. */
		cw_tone_queue_t * tq = test_cw_tq_capacity_test_init(cte, capacity, watermark, current_head_shift);

		/* Fill all positions in queue with tones of known
		   frequency.  If shift_head != 0, the enqueue
		   function should make sure that the enqueued tones
		   are nicely wrapped after end of queue. */
		for (size_t i = 0; i < tq->capacity; i++) {
			cw_tone_t tone;
			CW_TONE_INIT(&tone, (int) i, 1000, CW_SLOPE_MODE_NO_SLOPES);

			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "capacity A: enqueueing ton #%zu, queue size %zu, head shift %d", i, capacity, current_head_shift)) {
				enqueue_failure = true;
				break;
			}
		}

		/*
		  With the queue filled with valid and known data,
		  it's time to read back the data and verify that the
		  tones were placed in correct positions, as
		  expected. Let's do the readback N times, just for
		  fun. Every time the results should be the same.

		  We don't remove/dequeue tones from tq, we just
		  iterate over tq and check that tone at shifted_i has
		  correct, expected properties.
		*/
		for (int loop = 0; loop < 3; loop++) {
			for (size_t i = 0; i < tq->capacity; i++) {

				/* When shift of head == 0, tone with
				   frequency 'i' is at index 'i'. But with
				   non-zero shift of head, tone with frequency
				   'i' is at index 'shifted_i'. */
				const size_t shifted_i = (i + current_head_shift) % (tq->capacity);

				const size_t expected_freq = i;
				const size_t readback_freq = tq->queue[shifted_i].frequency;

				if (!cte->expect_op_int_errors_only(cte, expected_freq, "==", readback_freq, "capacity A: readback loop #%d: queue position %zu, head shift %d",
							loop, i, current_head_shift)) {
					dequeue_failure = true;
					break;
				}
			}
		}


		/* Matches tone queue creation made in
		   test_cw_tq_capacity_test_init(). */
		cw_tq_delete_internal(&tq);

		cte->expect_op_int(cte, false, "==", enqueue_failure, "capacity A: enqueue @ head shift = %d:", current_head_shift);
		cte->expect_op_int(cte, false, "==", dequeue_failure, "capacity A: dequeue @ head shift = %d:", current_head_shift);

		shift_idx++;
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Test "capacity" property of tone queue

   Function tests "capacity" property of tone queue, and also tests
   related properties: head and tail.

   Just like in test_cw_tq_test_capacity_A(), enqueueing is done with
   cw_tq_enqueue_internal().

   Unlike test_cw_tq_test_capacity_A(), this function dequeues tones
   using cw_tq_dequeue_internal().

   After every dequeue we check that dequeued tone is the one that we
   were expecting to get.

   @reviewed on 2019-10-04
*/
int test_cw_tq_test_capacity_B(cw_test_executor_t * cte)
{
	/* We don't need to check tq with capacity ==
	   CW_TONE_QUEUE_CAPACITY_MAX (yet). Let's test a smaller
	   queue. */
	uint32_t x_random = 0;
	cw_random_get_uint32(30, 70, &x_random);
	const size_t capacity = (size_t) x_random;
	const size_t watermark = capacity - (capacity * 0.2);

	cte->print_test_header(cte, "%s", __func__);

	/* We will do tests of queue with constant capacity, but with
	   different initial position at which we insert first element
	   (tone), i.e. different position of queue's head.

	   Elements of the array should be no larger than capacity. -1
	   is a guard.

	   TODO: allow negative head shifts in the test. */
	const int head_shifts[] = { 0, 5, 10, 29, -1, 30, -1 };
	int shift_idx = 0;

	while (head_shifts[shift_idx] != -1) {

		bool enqueue_failure = false;
		bool dequeue_failure = false;
		bool capacity_failure = false;
		const int current_head_shift = head_shifts[shift_idx];

		cte->log_info_cont(cte, "\n");
		cte->log_info(cte, "Testing with head shift = %d, capacity = %zd\n", current_head_shift, capacity);

		/* For every new test with new head shift we need a
		   "clean" queue. */
		cw_tone_queue_t * tq = test_cw_tq_capacity_test_init(cte, capacity, watermark, current_head_shift);

		/* Fill all positions in queue with tones of known
		   frequency.  If shift_head != 0, the enqueue
		   function should make sure that the enqueued tones
		   are nicely wrapped after end of queue. */
		for (size_t i = 0; i < tq->capacity; i++) {
			cw_tone_t tone;
			CW_TONE_INIT(&tone, (int) i, 1000, CW_SLOPE_MODE_NO_SLOPES);

			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
			if (!cte->expect_op_int_errors_only(cte,
							    CW_SUCCESS, "==", cwret,
							    "%s:%d: enqueueing tone #%zu/%zu",
							    __func__, __LINE__,
							    i + 1, tq->capacity)) {
				enqueue_failure = true;
				break;
			}
		}

		/* With the queue filled with valid and known data,
		   it's time to read back the data and verify that the
		   tones were placed in correct positions, as
		   expected.

		   In test_cw_tq_test_capacity_A() we did the readback
		   "manually" (or rather we just iterated over a tone
		   queue, without actually taking anything out of the
		   tq), this time let's use "dequeue" function to do
		   the job.

		   Since the "dequeue" function moves queue pointers,
		   we can do this test only once (we can't repeat the
		   readback N times with calls to dequeue() expecting
		   the same results). */

		size_t i = 0;
		cw_tone_t deq_tone; /* For output only, so no need to initialize. */
		cw_queue_state_t state;
		while (CW_TQ_EMPTY != (state = LIBCW_TEST_FUT(cw_tq_dequeue_internal)(tq, &deq_tone))) {

			/* When shift of head == 0, tone with
			   frequency 'i' is at index 'i'. But with
			   non-zero shift of head, tone with frequency
			   'i' is at index 'shifted_i'. */

			const size_t expected_freq = i;
			const size_t readback_freq = deq_tone.frequency;

			if (!cte->expect_op_int_errors_only(cte,
							    expected_freq, "==", readback_freq,
							    "%s:%d: readback: state = %d, queue position %zu/%zu, head shift %d",
							    __func__, __LINE__,
							    state,
							    i + 1, tq->capacity, current_head_shift)) {
				dequeue_failure = true;
				break;
			}

			i++;
		}
		const size_t n_dequeues = i;

		if (!cte->expect_op_int_errors_only(cte, tq->capacity, "==", n_dequeues, "capacity B: number of dequeues vs tone queue capacity")) {
			capacity_failure = true;
		}


		/* Matches tone queue creation made in
		   test_cw_tq_capacity_test_init(). */
		cw_tq_delete_internal(&tq);


		cte->expect_op_int(cte, false, "==", enqueue_failure, "capacity B: enqueue  @ shift = %d:", current_head_shift);
		cte->expect_op_int(cte, false, "==", dequeue_failure, "capacity B: dequeue  @ shift = %d:", current_head_shift);
		cte->expect_op_int(cte, false, "==", capacity_failure, "capacity B: capacity @ shift = %d:", current_head_shift);


		shift_idx++;
	}

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   \brief Create and initialize tone queue for tests of capacity

   Create new tone queue for tests using three given parameters: \p
   capacity, \p high_water_mark, \p head_shift. The function is used
   to create a new tone queue in tests of "capacity" parameter of a
   tone queue.

   First two function parameters are rather boring. What is
   interesting is the third parameter: \p head_shift.

   In general the behaviour of tone queue (a circular list) should be
   independent of initial position of queue's head (i.e. from which
   position in the queue we start adding new elements to the queue).

   By initializing the queue with different initial positions of head
   pointer, we can test this assertion about irrelevance of initial
   head position.

   The "initialize" word may be misleading. The function does not
   enqueue any tones, it just initializes (resets) every slot in queue
   to non-random value.

   Returned pointer is owned by caller.

   @reviewed on 2019-10-04

   \param capacity - intended capacity of tone queue
   \param high_water_mark - high water mark to be set for tone queue
   \param head_shift - position of first element that will be inserted in empty queue

   \return newly allocated and initialized tone queue
*/
cw_tone_queue_t * test_cw_tq_capacity_test_init(cw_test_executor_t * cte, size_t capacity, size_t high_water_mark, int head_shift)
{
	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tone queue");
	// tq->state = CW_TQ_NONEMPTY; TODO: what does it do here?

	/*
	  TODO: we can come up with better test for "set_capacity_internal":
	  1. have empty queue,
	  2. set capacity of queue to N,
	  3. try to add N tones - should succeed,
	  4. try to add N+1 tones - should fail.
	*/
	cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_set_capacity_internal)(tq, capacity, high_water_mark);
	cte->assert2(cte, cwret == CW_SUCCESS, "failed to set capacity/high water mark");
	cte->assert2(cte, tq->capacity == capacity, "incorrect capacity: %zu != %zu", tq->capacity, capacity);
	cte->assert2(cte, tq->high_water_mark == high_water_mark, "incorrect high water mark: %zu != %zu", tq->high_water_mark, high_water_mark);

	/* Initialize *all* tones with known value. Do this manually,
	   to be 100% sure that all tones in queue table have been
	   initialized. */
	for (int i = 0; i < CW_TONE_QUEUE_CAPACITY_MAX; i++) {
		CW_TONE_INIT(&tq->queue[i], 10000 + i, 1, CW_SLOPE_MODE_STANDARD_SLOPES);
	}

	/* Move head and tail of empty queue to initial position. The
	   queue is empty because the initialization of fields done
	   above is not considered as real enqueueing of valid
	   tones. */
	tq->tail = head_shift;
	tq->head = tq->tail;
	tq->len = 0;

	/* TODO: why do this here? */
	//tq->state = CW_TQ_NONEMPTY;

	return tq;
}




/**
   \brief Test the limits of the parameters to the tone queue routine

   @reviewed on 2019-10-04
*/
cwt_retv test_cw_tq_enqueue_internal_tone_validity(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create a tone queue\n");
	cw_tone_t tone;
	cw_ret_t cwret;


	int freq_min;
	int freq_max;
	cw_get_frequency_limits(&freq_min, &freq_max);


	/* Test 1: invalid duration of tone. */
	errno = 0;
	tone.duration = -1;          /* Invalid duration. */
	tone.frequency = freq_min;   /* Valid frequency. */
	cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
	cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "enqueued tone with invalid duration (cwret)");
	cte->expect_op_int(cte, EINVAL, "==", errno, "enqueued tone with invalid duration (cwret)");


	/* Test 2: tone's frequency too low. */
	errno = 0;
	tone.duration = 100;              /* Valid duration. */
	tone.frequency = freq_min - 1;    /* Invalid frequency. */
	cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
	cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "enqueued tone with too low frequency (cwret)");
	cte->expect_op_int(cte, EINVAL, "==", errno, "enqueued tone with too low frequency (errno)");


	/* Test 3: tone's frequency too high. */
	errno = 0;
	tone.duration = 100;              /* Valid duration. */
	tone.frequency = freq_max + 1;    /* Invalid frequency. */
	cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
	cte->expect_op_int(cte, CW_FAILURE, "==", cwret, "enqueued tone with too high frequency (cwret)");
	cte->expect_op_int(cte, EINVAL, "==", errno, "enqueued tone with too high frequency (errno)");


	cw_tq_delete_internal(&tq);
	cte->expect_null_pointer(cte, tq, "tone queue not deleted properly");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   @brief Add @param count tones to tone queue

   This is a test helper function.

   @reviewed 2020-10-03

   @param[in] cte test executor
   @param[in/out] tq tone queue to which to add tones
   @param[in] count how many tones to add?

   @return cwt_retv_ok on success
   @return cwt_retv_err otherwise
*/
cwt_retv test_helper_fill_queue(cw_test_executor_t * cte, cw_tone_queue_t * tq, size_t count)
{
	/* This is a test helper function, so I don't use
	   cte->expect_op_int...(). Using this function would increment test
	   status counters. */

	cw_tone_t tone;
	CW_TONE_INIT(&tone, 20, 10000, CW_SLOPE_MODE_STANDARD_SLOPES);

	for (size_t i = 0; i < count; i++) {
		const cw_ret_t cwret = cw_tq_enqueue_internal(tq, &tone);
		if (CW_SUCCESS != cwret) {
			cte->log_error(cte, "%s:%d: enqueue tone #%zd failed\n", __func__, __LINE__, i);
			return cwt_retv_err;
		}
	}

	const size_t len = tq->len;
	if (len == count) {
		return cwt_retv_ok;
	} else {
		cte->log_error(cte, "%s:%d: queue len invalid: %zd != %zd\n",
			       __func__, __LINE__, len, count);
		return cwt_retv_err;
	}
}




/**
   This function creates a generator that internally uses a tone
   queue. The generator is needed to perform automatic dequeueing
   operations, so that cw_tq_wait_for_level_internal() can detect
   expected level.

   @reviewed on
*/
cwt_retv test_cw_tq_wait_for_level_internal(cw_test_executor_t * cte)
{
	/* +1 for cases where repetition count is 1.
	   In such cases modulo operation in the loop would fail. */
	const int loops = cte->get_loops_count(cte) + 1;
	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	bool wait_failure = false;
	bool diff_failure = false;

	cw_gen_t * gen = NULL;

	for (int i = 0; i < loops; i++) {
		gen = cw_gen_new(&cte->current_gen_conf);
		cte->assert2(cte, gen, "failed to create a tone queue\n");
		cw_gen_start(gen);

		if (cwt_retv_ok != test_helper_fill_queue(cte, gen->tq, loops)) {
			cte->log_error(cte, "%s:%d: failed to fill tone queue\n", __func__, __LINE__);
			return cwt_retv_err;
		}

		/* Notice that level is always smaller than number of
		   items added to queue. TODO: reconsider if we want
		   to randomize this value. */
		int level = 0;
		cw_random_get_int(0, (int) (floorf(0.7F * loops)), &level);
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_level_internal)(gen->tq, level);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "wait for level = %d, tone queue items count = %d", level, loops)) {
			wait_failure = true;
			break;
		}

		const size_t readback_len = cw_tq_length_internal(gen->tq);

		/* cw_tq_length_internal() is called after return of
		   tested function, so 'len' can be smaller by one,
		   but never larger, than 'level'.

		   During initial tests, for function implemented with
		   signals and with alternative (newer) inter-thread
		   comm method, len was always equal to level. */
		const size_t expected_len_lower = level == 0 ? level : level - 1;
		const size_t expected_len_higher = level;
		if (!cte->expect_between_int_errors_only(cte, expected_len_lower, readback_len, expected_len_higher, "wait for level: length of queue after end of waiting")) {
			diff_failure = true;
			break;
		}

		cw_gen_stop(gen);
		cw_gen_delete(&gen);
	}

	cte->expect_op_int(cte, false, "==", wait_failure, "wait for level (wait function)");
	cte->expect_op_int(cte, false, "==", diff_failure, "wait for level (queue length)");

	/* For those tests that fail. */
	if (gen) {
		cw_gen_stop(gen);
		cw_gen_delete(&gen);
	}

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




/**
   \brief Simple tests of queueing and dequeueing of tones

   This is not an entirely stand-alone queue, but a queue that is a part of generator.

   Ensure we can generate a few simple tones, and wait for them to end.

   @reviewed on
*/
int test_cw_tq_gen_operations_A(cw_test_executor_t * cte)
{
	/* +1 for cases where repetition count is 1.  In such cases
	   division operation in the loop would fail. */
	const int loops = cte->get_loops_count(cte) + 1;
	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_gen_t * gen = NULL;
	if (0 != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return -1;
	}
	/* Notice that we start the generator later. */

	int freq_min;
	int freq_max;
	cw_get_frequency_limits(&freq_min, &freq_max);

	const int duration = 100000;  /* Duration of tone. */
	const int delta_freq = ((freq_max - freq_min) / (loops - 1));


	/* Test 1: enqueue max tones, and wait for each of them
	   separately. Control length of tone queue in the process. */
	{
		bool length_failure = false;
		bool enqueue_failure = false;


		/* Enqueue first tone. Don't check queue length yet.

		   The first tone is being dequeued right after enqueueing, so
		   checking the queue length would yield incorrect result.
		   Instead, enqueue the first tone, and during the process of
		   dequeueing it, enqueue rest of the tones in the loop,
		   together with checking length of the tone queue. */
		int freq = freq_min;
		cw_tone_t tone;
		CW_TONE_INIT(&tone, freq, duration, CW_SLOPE_MODE_NO_SLOPES);


		/* Enqueue rest of tones. Generator is not started
		   yet, so tones won't be dequeued parallel to being
		   enqueued, so we always have certainty how many
		   tones must there be in queue. */
		for (int i = 0; i < loops; i++) {
			/* Monitor length of a queue as it is filled - before
			   adding a new tone. */
			size_t expected_length = (size_t) i; /* Expected length of tone queue. */

			/* Measured length of tone queue. */
			size_t readback_length = LIBCW_TEST_FUT(cw_tq_length_internal)(gen->tq);
			if (!cte->expect_op_int_errors_only(cte, expected_length, "==", readback_length, "tq gen operations A: length pre-enqueue (#%02d):", i)) {
				length_failure = true;
				break;
			}


			/* Add a tone to queue. All frequencies should be
			   within allowed range, so there should be no
			   error. */
			freq = freq_min + i * delta_freq;
			CW_TONE_INIT(&tone, freq, duration, CW_SLOPE_MODE_NO_SLOPES);
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(gen->tq, &tone);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "tq gen operations A: enqueue (#%02d)", i)) {
				enqueue_failure = true;
				break;
			}


			/* Monitor length of a queue as it is filled - after
			   adding a new tone. */
			readback_length = LIBCW_TEST_FUT(cw_tq_length_internal)(gen->tq);
			expected_length = (size_t) (i + 1);
			if (!cte->expect_op_int_errors_only(cte, expected_length, "==", readback_length, "tq gen operations A: length: post-enqueue (#%02d):", i)) {
				length_failure = true;
				break;
			}
		}
		cte->expect_op_int(cte, false, "==", length_failure, "tq gen operations A: length during enqueue");
		cte->expect_op_int(cte, false, "==", enqueue_failure, "tq gen operations A: enqueue");
	}




	/* And this is the proper test - waiting for dequeueing tones.
	   The dequeueing must happen automatically, so we have to
	   start the generator. Starting the generator will dequeue
	   first tone, so we will expect the measured length to be in
	   a range of values. */
	cw_gen_start(gen);

	/* TODO: I understand that when generator is started, one tone
	   is taken from tq: this is reflected in using "-1" in
	   initialization of for() loop. But then testing the tone
	   queue with ranges is not really necessary: we should be
	   able to tell exactly what the length of queue in each
	   iteration will be. */
	bool length_failure = false;
	bool wait_failure = false;
	for (int i = loops - 1; i > 0; i--) { /* -1 because after starting the generator one tone is already dequeued. */

		size_t readback_length = 0;  /* Measured length of tone queue. */
		size_t expected_length_min = 0;
		size_t expected_length_max = 0;

		/* Monitor length of a queue as it is emptied - before dequeueing. */
		readback_length = LIBCW_TEST_FUT(cw_tq_length_internal)(gen->tq);
		expected_length_min = (size_t) i - 1;
		expected_length_max = (size_t) i;
		if (!cte->expect_between_int_errors_only(cte, expected_length_min, readback_length, expected_length_max, "tq gen operations A: length pre-dequeue (#%02d)", i)) {
			length_failure = true;
			break;
		}

		/* Wait for each of N tones to be dequeued. */
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_end_of_current_tone_internal)(gen->tq);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "tq gen operations A: wait for tone (#%02d)", i)) {
			wait_failure = true;
			break;
		}

		/* Monitor length of a queue as it is emptied - after dequeueing. */
		readback_length = LIBCW_TEST_FUT(cw_tq_length_internal)(gen->tq);
		expected_length_min = (size_t) i - 1 - 1;
		expected_length_max = (size_t) i - 1;
		if (!cte->expect_between_int_errors_only(cte, expected_length_min, readback_length, expected_length_max, "tq gen operations A: length post-dequeue (#%02d)", i)) {
			length_failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", length_failure, "tq gen operations A: length during dequeue");
	cte->expect_op_int(cte, false, "==", wait_failure, "tq gen operations A: wait for tone");


	/* Test 2: fill a queue, but this time don't wait for each
	   tone separately, but wait for a whole queue to become
	   empty. */

	bool failure = false;
	for (int i = 0; i < loops; i++) {
		int freq = freq_min + i * delta_freq;
		cw_tone_t tone;
		CW_TONE_INIT(&tone, freq, duration, CW_SLOPE_MODE_NO_SLOPES);
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(gen->tq, &tone);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "tq gen operations A: enqueue all, tone %04d)", i)) {
			failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", failure, "tq gen operations A: enqueue all");


	const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_level_internal)(gen->tq, 0);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "tq gen operations A: final wait for level 0");


	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   Run the complete range of tone generation, at 1Hz intervals, first
   up the octaves, and then down.  If the queue fills (which is
   expected with frequency step so small) then pause until it isn't so
   full.

   @reviewed on 2019-10-06
*/
int test_cw_tq_gen_operations_B(cw_test_executor_t * cte)
{
	cw_gen_t * gen = NULL;
	if (0 != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return -1;
	}
	const size_t capacity = cw_tq_capacity_internal(gen->tq);
	int level = 0;
	cw_random_get_int(0, (capacity / 2), &level);


	cte->print_test_header(cte, "%s (%d)", __func__, level);


	const int duration = 4000;

	int freq_min;
	int freq_max;
	cw_get_frequency_limits(&freq_min, &freq_max);
	const int freq_delta = 1;

	/* Because in the loops below we want to saturate (completely
	   fill) tone queue with tones and then wait (with
	   cw_tq_is_full_internal() + cw_tq_wait_for_level_internal())
	   for some free space in the queue, we have to be able to
	   enqueue more tones than the capacity of tq. Because we want
	   to enqueue (freq_max - freq_min) tones, we better check
	   that capacity is smaller than that. */
	cte->assert2(cte, capacity < (size_t) (freq_max - freq_min), "range of frequencies is too small");


	bool wait_failure = false;
	bool queue_failure = false;

	/* The test loops below use cw_tq_wait_for_level_internal(),
	   so generator must be running. */
	cw_gen_start(gen);

	for (int freq = freq_min; freq < freq_max; freq += freq_delta) {
		while (cw_tq_is_full_internal(gen->tq)) {
			/* TODO: currently there is no guarantee that
			   the tone queue will become full and the
			   tested function will be called. It would be
			   better to make capacity rather small
			   (e.g. 200) compared to range of
			   frequencies. */
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_level_internal)(gen->tq, level);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "wait for level %d (up)", level)) {
				wait_failure = true;
				break;
			}
		}

		cw_tone_t tone;
		CW_TONE_INIT(&tone, freq, duration, CW_SLOPE_MODE_NO_SLOPES);
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(gen->tq, &tone);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue tone %d (up)", freq)) {
			queue_failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", queue_failure, "enqueueing tone (up)");
	cte->expect_op_int(cte, false, "==", wait_failure, "waiting for level %d (up)", level);


	wait_failure = false;
	queue_failure = false;
	for (int freq = freq_max; freq > freq_min; freq -= 1) {
		while (cw_tq_is_full_internal(gen->tq)) {
			const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_level_internal)(gen->tq, level);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "wait for level %d (down)", level)) {
				wait_failure = true;
				break;
			}
		}

		cw_tone_t tone;
		CW_TONE_INIT(&tone, freq, duration, CW_SLOPE_MODE_NO_SLOPES);
		const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(gen->tq, &tone);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueue tone %d (down)", freq)) {
			queue_failure = true;
			break;
		}
	}
	cte->expect_op_int(cte, false, "==", queue_failure, "enqueueing tone (down)");
	cte->expect_op_int(cte, false, "==", wait_failure, "waiting for level %d (down)", level);



	const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_wait_for_level_internal)(gen->tq, 0);
	cte->expect_op_int(cte, CW_SUCCESS, "==", cwret, "waiting for level 0 (final)");


	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, 100, CW_SLOPE_MODE_NO_SLOPES);
	cw_tq_enqueue_internal(gen->tq, &tone);
	cw_tq_wait_for_level_internal(gen->tq, 0);

	gen_destroy(&gen);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test properties (capacity, length and other) of empty tone queue

   @reviewed 2020-10-03
*/
cwt_retv test_cw_tq_properties_empty(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	bool failure_capacity = false;
	bool failure_length = false;
	bool failure_dequeue = false;

	/*
	  Do the tests few times in a loop. In the loop simulate using the
	  queue (by filling it) and then emptying it and in next iteration
	  re-test properties of emptied queue (i.e. queue that is empty not
	  because it's never been used, but because it has been emptied).
	*/
	for (size_t i = 0; i < 5; i++) {

		/* Capacity of queue should always be the same. */
		const int capacity = LIBCW_TEST_FUT(cw_tq_capacity_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte,
						    CW_TONE_QUEUE_CAPACITY_MAX, "==", capacity,
						    "%s:%d: empty queue's capacity",
						    __func__, __LINE__)) {
			failure_capacity = true;
			break;
		}

		/* Length of empty tone queue should be zero (of course). */
		const size_t readback_len = LIBCW_TEST_FUT(cw_tq_length_internal)(tq);
		if (!cte->expect_op_int_errors_only(cte,
						    0, "==", readback_len,
						    "%s:%d: empty queue's length",
						    __func__, __LINE__)) {
			failure_length = true;
			break;
		}
		if (!cte->expect_op_int_errors_only(cte,
						    0, "==", tq->len,
						    "%s:%d: empty queue's length",
						    __func__, __LINE__)) {
			failure_length = true;
			break;
		}

		/* Dequeueing from empty tone queue should return CW_TQ_EMPTY. */
		cw_tone_t tone;
		const cw_queue_state_t queue_state = LIBCW_TEST_FUT(cw_tq_dequeue_internal)(tq, &tone);
		if (!cte->expect_op_int_errors_only(cte,
						    CW_TQ_EMPTY, "==", queue_state,
						    "%s:%d: dequeueing from empty queue",
						    __func__, __LINE__)) {
			failure_dequeue = true;
			break;
		}


		/* Prepare tone queue for next iteration of the loop: fill
		   and empty the tone queue. */
		if (cwt_retv_ok != test_helper_fill_queue(cte, tq, tq->capacity)) {
			cte->log_error(cte, "%s:%d: failed to fill tone queue\n", __func__, __LINE__);
			return cwt_retv_err;
		}
		cw_tq_flush_internal(tq);
		cw_tq_wait_for_level_internal(tq, 0);


		/* Go to checking properties of emptied tq in next iteration. */
	}


	cte->expect_op_int(cte,
			   false, "==", failure_capacity,
			   "%s:%d: empty queue's capacity",
			   __func__, __LINE__);
	cte->expect_op_int(cte,
			   false, "==", failure_length,
			   "%s:%d: empty queue's length",
			   __func__, __LINE__);
	cte->expect_op_int(cte,
			   false, "==", failure_dequeue,
			   "%s:%d: dequeueing from empty queue",
			   __func__, __LINE__);


	cw_tq_delete_internal(&tq);
	cte->expect_null_pointer_errors_only(cte, tq, "deleting tone queue");

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Test properties (capacity, length and other) of full tq

   Since the queue is not a member of a generator that is started, there will
   be no automatic dequeueing. TODO: add a test that verifies that
   non-started-generator doesn't dequeue tones.

   @reviewed 2020-10-02
*/
cwt_retv test_cw_tq_properties_full(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	cw_tone_queue_t * tq = cw_tq_new_internal();
	if (cwt_retv_ok != test_helper_fill_queue(cte, tq, tq->capacity)) {
		cte->log_error(cte, "%s:%d: failed to fill tone queue\n", __func__, __LINE__);
		return cwt_retv_err;
	}


	const size_t capacity = LIBCW_TEST_FUT(cw_tq_capacity_internal)(tq);
	cte->expect_op_int(cte,
			   CW_TONE_QUEUE_CAPACITY_MAX, "==", capacity,
			   "%s:%d: full queue's capacity",
			   __func__, __LINE__);


	const size_t len_full = LIBCW_TEST_FUT(cw_tq_length_internal)(tq);
	cte->expect_op_int(cte,
			   CW_TONE_QUEUE_CAPACITY_MAX, "==", len_full,
			   "%s:%d: full queue's length",
			   __func__, __LINE__);


	/* This tests for correctness of working of the 'enqueue' function.
	   Full tq should not grow beyond its capacity. */
	cte->expect_op_int(cte,
			   tq->capacity, "==", tq->len,
			   "%s:%d: length of full queue vs. capacity (first variant)",
			   __func__, __LINE__);
	cte->expect_op_int(cte,
			   capacity, "==", len_full,
			   "%s:%d: length of full queue vs. capacity (second variant)",
			   __func__, __LINE__);


	/* Try adding a tone to full tq. This tests for potential problems
	   with 'enqueue' function.  Enqueueing should fail when the queue is
	   full. */
	cte->log_info(cte, "*** you may now see \"EE: can't enqueue tone, tq is full\" message ***\n");
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, 100, CW_SLOPE_MODE_NO_SLOPES);
	const cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_enqueue_internal)(tq, &tone);
	cte->expect_op_int(cte,
			   CW_FAILURE, "==", cwret,
			   "%s:%d: attempting to enqueue tone to full queue",
			   __func__, __LINE__);


	cw_tq_delete_internal(&tq);

	cte->print_test_footer(cte, __func__);

	return 0;
}




struct cw_callback_struct {
	cw_gen_t * gen;
	cw_test_executor_t * cte;
	size_t captured_level;
	bool already_captured;
};


/**
   @reviewed on 2019-10-07
*/
int test_cw_tq_callback(cw_test_executor_t * cte)
{
	const int loops = cte->get_loops_count(cte);
	cte->print_test_header(cte, "%s (%d)", __func__, loops);

	cw_gen_t * gen = NULL;

	bool register_failure = false;
	bool enqueue_failure = false;
	bool capture_failure = false;

	for (int i = 1; i < loops; i++) { /* TODO: the test doesn't work for i == 0. This needs to be communicated in documentation. */

		if (0 != gen_setup(cte, &gen)) {
			cte->log_error(cte, "%s:%d: Failed to create generator in iteration %d\n", __func__, __LINE__, i);
			return -1;
		}

		struct cw_callback_struct test_data;
		memset(&test_data, 0, sizeof (test_data));
		test_data.gen = gen;
		test_data.cte = cte;
		test_data.captured_level = 999999;
		test_data.already_captured = false; /* Used to prevent multiple captures of level in one iteration of this loop. */

		/* Test the callback mechanism for very small values,
		   but for a bit larger as well. */
		int level = i <= 5 ? i : 3 * i;

		cw_ret_t cwret = LIBCW_TEST_FUT(cw_tq_register_low_level_callback_internal)(gen->tq, test_helper_tq_callback, (void *) &test_data, level);
		if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "registering low level callback with level %d", level)) {
			register_failure = true;
			break;
		}


		/* Add a lot of tones to tone queue. "a lot" means two
		   times more than a value of trigger level. */
		for (int j = 0; j < 2 * level; j++) {
			cwret = cw_gen_enqueue_character_no_ics(gen, 'e'); /* TODO: if we want to add 'a lot", then single-mark 'e' may not be the best choice. */
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret, "enqueueing tones, tone #%d", j)) {
				enqueue_failure = true;
				break;
			}
		}
		if (enqueue_failure) {
			break;
		}


		/* Start generator (and dequeueing) only after the tq
		   has been filled. */
		cw_gen_start(gen);

		/* Wait for the queue to be drained to zero. While the
		   tq is drained, and level of tq reaches trigger
		   level, a callback will be called. Its only task is
		   to copy the current level (tq level at time of
		   calling the callback) value into
		   test_data.captured_level.

		   Since the value of trigger level is different in
		   consecutive iterations of loop, we can test the
		   callback for different values of trigger level. */
		cw_tq_wait_for_level_internal(gen->tq, 0);

		/* Because of order of calling callback and decreasing
		   length of queue, I think that it's safe to assume
		   that level captured by registered callback may be
		   within a range of expected values. */
		const int expected_level_lower = level - 1;
		const int expected_level_higher = level;
		if (!cte->expect_between_int_errors_only(cte, expected_level_lower, test_data.captured_level, expected_level_higher, "tone queue callback:  capturing level")) {
			capture_failure = true;
			break;
		}

		cw_tq_flush_internal(gen->tq);
		gen_destroy(&gen);
	}

	cte->expect_op_int(cte, false, "==", register_failure, "registering low level callback");
	cte->expect_op_int(cte, false, "==", enqueue_failure, "enqueueing tones");
	cte->expect_op_int(cte, false, "==", capture_failure, "capturing tone queue level");


	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @reviewed on 2019-10-07
*/
static void test_helper_tq_callback(void * data)
{
	struct cw_callback_struct * s = (struct cw_callback_struct *) data;
	if (!s->already_captured) {

		s->captured_level = cw_tq_length_internal(s->gen->tq);
		s->already_captured = true;

		s->cte->log_info(s->cte, "tq callback helper: captured level = %zd\n", s->captured_level);
	}

	return;
}




/**
   @brief Test return values of dequeue function

   @reviewed 2020-10-03
*/
cwt_retv test_cw_tq_dequeue_internal_returns(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);
	bool failure = false;

	cw_tone_queue_t * tq = cw_tq_new_internal();
	cte->assert2(cte, tq, "failed to create new tone queue");

	struct {
		int frequency;
		cw_queue_state_t expected_state;
	} test_data[] = {
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },
		{ 100, CW_TQ_NONEMPTY },

		/* This will be the last tone added to queue, so dequeuing it
		   should return 'just emptied'. */
		{ 100, CW_TQ_JUST_EMPTIED },

		/* This tone won't be enqueued by test code (frequency == 0),
		   and dequeue operation should return 'empty' when doing
		   this N-th dequeue attempt because the queue should
		   transition from 'just emptied' to 'empty' */
		{   0, CW_TQ_EMPTY },
	};
	const size_t n_items = sizeof (test_data) / sizeof (test_data[0]);

	/* Enqueue tones. */
	for (size_t i = 0; i < n_items; i++) {
		if (test_data[i].frequency) {
			cw_tone_t tone;
			const int duration = 100;
			CW_TONE_INIT(&tone, test_data[i].frequency, duration, CW_SLOPE_MODE_STANDARD_SLOPES);
			cw_tq_enqueue_internal(tq, &tone);
		}
	}

	/* Dequeue tones, do the test. */
	for (size_t i = 0; i < n_items; i++) {
		cw_tone_t tone;
		const cw_queue_state_t queue_state = LIBCW_TEST_FUT(cw_tq_dequeue_internal)(tq, &tone);
		if (!cte->expect_op_int_errors_only(cte,
						    test_data[i].expected_state, "==", queue_state,
						    "%s:%d: dequeue return value for test data #%zd",
						    __func__, __LINE__,
						    i)) {
			failure = true;
			break;
		}
	}

	cw_tq_delete_internal(&tq);

	cte->expect_op_int(cte, false, "==", failure, "dequeue's return values");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

