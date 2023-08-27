/*
   Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
   Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/




/**
   @file libcw_legacy_api_tests_rec_poll.c

   Test code for 'poll' method for legacy receiver used in xcwcp.

   xcwcp is implementing a receiver of Morse code keyed with Space or
   Enter keyboard key. Recently I have added a test code to xcwcp
   (from unixcw 3.5.1) that verifies how receive process is working
   and whether it is working correctly.

   Then I have extracted the core part of the xcwcp receiver code and
   the test code, and I have put it here.

   Now we have the test code embedded in xcwcp (so we can always have
   it as 'in vivo' test), and we have the test code here in libcw
   tests set.

   In a regular production code the received data would be accumulated thanks
   to a callback function registered in receiver using a dedicated
   function, but this test doesn't require such accumulation.
*/




#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#if defined(HAVE_STRING_H)
# include <string.h> /* FreeBSD 12.1 */
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#include <cwutils/cw_common.h>
#include <cwutils/cw_rec_tester.h>
#include <cwutils/cw_easy_legacy_receiver.h>
#include <cwutils/cw_easy_legacy_receiver_internal.h>
#include <cwutils/sleep.h>




#include "libcw.h"
#include "libcw2.h"




#include "libcw_gen.h"
#include "libcw_key.h"
#include "libcw_tq.h"
#include "libcw_utils.h"

#include "test_framework.h"
#include <test_framework/basic_utils/param_ranger.h>
#include "libcw_legacy_api_tests_rec_poll.h"




static cw_test_executor_t * g_cte;




static cwt_retv legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool poll_representation);


/* Main poll function and its helpers. */
static void receiver_poll_data(cw_easy_legacy_receiver_t * easy_rec);
static void receiver_poll_character(cw_easy_legacy_receiver_t * easy_rec, bool poll_representation);
static void receiver_poll_space(cw_easy_legacy_receiver_t * easy_rec, bool poll_representation);

static void receiver_poll_report_error(cw_easy_legacy_receiver_t * easy_rec);

static int expect_correct_receive_on_character_c_r(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer);
static int expect_correct_receive_on_character_r_c(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer);
static int expect_correct_receive_on_space_r_c(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer);
static int expect_correct_receive_on_space_c_r(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer);




/**
   @brief Poll the receiver and handle anything found in the data returned by
   the poll function

   This function should be periodically called to try and poll a character
   from given @p easy_rec.

   @reviewedon 2023.08.26

   @param easy_rec Easy receiver to poll
*/
static void receiver_poll_data(cw_easy_legacy_receiver_t * easy_rec)
{
	/*
	  The flow of code in this function is a standard flow of receiver's poll
	  function.
	*/
	if (0 != cw_easy_legacy_receiver_get_libcw_errno(easy_rec)) {
		receiver_poll_report_error(easy_rec);
	}

	if (cw_easy_legacy_receiver_is_pending_iws(easy_rec)) {
		/* Check if receiver received the pending inter-word-space. */
		receiver_poll_space(easy_rec, easy_rec->get_representation);

		if (!cw_easy_legacy_receiver_is_pending_iws(easy_rec)) {
			/* We received the pending space. After it the
			   receiver may have received another
			   character.  Try to get it too. */
			receiver_poll_character(easy_rec, easy_rec->get_representation);
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		receiver_poll_character(easy_rec, easy_rec->get_representation);
	}
}




/**
   \brief Handle any error that occurred when handling a libcw keying event

   @reviewedon 2023.08.15

   @param[in/out] easy_rec Easy receiver for which to do an error handling
*/
static void receiver_poll_report_error(cw_easy_legacy_receiver_t * easy_rec)
{
	/* TODO (acerion) 2023.08.15: do a real reporting of error. */

	cw_easy_legacy_receiver_clear_libcw_errno(easy_rec);
}




/**
   \brief Try polling non-inter-word-space character from legacy receiver

   The function is called r_c because primary function in production code
   polls representation, and only then in test code a character is polled.

   If @p poll_representation is true, this function will try polling a
   representation (string of dots/dashes) of received data.

   If @p poll_representation is false, this function will try polling a
   character (represented by integer-typed variable).

   This test code does a verification of data returned by the first poll
   described above. The verification is done during a second poll that does
   the polling using the "opposite" function. Results of both polls are
   compared with each other.

   @reviewedon 2023.08.26

   @param[in/out] easy_rec Receiver from which to poll character
   @param[in] poll_representation Flag indicating which method to use for polling
*/
static void receiver_poll_character(cw_easy_legacy_receiver_t * easy_rec, bool poll_representation)
{
	/* Don't use easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   local_timer.

	   Additionally using easy_rec->main_timer here would mess up
	   time intervals measured by easy_rec->main_timer, and that
	   would interfere with recognizing dots and dashes. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL); /* TODO acerion 2023.08.26: use monotonic clock instead of wall clock. */
	//fprintf(stderr, "poll_receive_char:  %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	const bool debug_errnos = false;
	static int prev_errno = 0;

	cw_rec_data_t erd = { 0 };
	cw_ret_t cwret = CW_FAILURE;
	const char * function_name = NULL;
	if (poll_representation) {
		cwret = LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, erd.representation, &erd.is_iws, &erd.is_error);
		function_name = "cw_receive_representation()";
	} else {
		cwret = LIBCW_TEST_FUT(cw_receive_character)(&local_timer, &erd.character, &erd.is_iws, NULL);
		function_name = "cw_receive_character()";
	}
	if (CW_SUCCESS == cwret) {

		prev_errno = 0;

		if (poll_representation) {
			/* Polling the receiver gives us a representation in erd. Display it. */
			fprintf(stderr, "[II] Polled representation '%s'\n", erd.representation);

			/* Test code. */
			{
				/* Test currently polled data. */
				expect_correct_receive_on_character_r_c(g_cte, &erd, &local_timer);

				/* Append current data to accumulator for later tests. */
				cw_rec_tester_t * tester = (cw_rec_tester_t *) easy_rec->rec_tester;
				tester->received_string[tester->received_string_i++] = erd.character;
			}

		} else {
			/* Polling the receiver gives us a character in erd. Display it. */
			fprintf(stderr, "[II] Polled character '%c'\n", erd.character);

			/* Test code. */
			{
				/* Test currently polled data. */
				expect_correct_receive_on_character_c_r(g_cte, &erd, &local_timer);

				/* Append current data to accumulator for later tests. */
				cw_rec_tester_t * tester = (cw_rec_tester_t *) easy_rec->rec_tester;
				tester->received_string[tester->received_string_i++] = erd.character;
			}
		}


		/* A full character has been received. Directly after it comes some
		   space. Either a short inter-character-space followed by another
		   character (in this case we won't display the
		   inter-character-space), or longer inter-word-space - this space we
		   would like to catch and display.

		   Set a flag indicating that next poll *may* result in
		   inter-word-space. */
		easy_rec->is_pending_iws = true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (errno) {
		case EAGAIN:
			/* Call made too early, receiver hasn't received a full character
			   yet. Try next time. */
			if (debug_errnos && prev_errno != EAGAIN) {
				fprintf(stderr, "[NN] %s: %d -> EAGAIN\n", function_name, prev_errno);
				prev_errno = EAGAIN;
			}
			break;

		case ERANGE:
			/* Call made not in time, or not in proper sequence. Receiver
			   hasn't received any character (yet). Try harder. */
			if (debug_errnos && prev_errno != ERANGE) {
				fprintf(stderr, "[NN] %s: %d -> RANGE\n", function_name, prev_errno);
				prev_errno = ERANGE;
			}
			break;

		case ENOENT:
			/* Invalid character in receiver's buffer. */
			if (debug_errnos && prev_errno != ENOENT) {
				fprintf(stderr, "[NN] %s: %d -> ENONENT\n", function_name, prev_errno);
				prev_errno = ENOENT;
			}
			cw_clear_receive_buffer();
			break;

		case EINVAL:
			/* Invalid timestamp. */
			if (debug_errnos && prev_errno != EINVAL) {
				fprintf(stderr, "[NN] %s: %d -> EINVAL\n", function_name, prev_errno);
				prev_errno = EINVAL;
			}
			cw_clear_receive_buffer();
			break;

		default:
			perror(function_name);
			// TODO: Perhaps this should be counted as test error
			return;
		}
	}
}




/**
   @brief Try polling inter-word-space character from legacy receiver

   If we received a character on an earlier poll, check again to see if we
   need to revise the decision about whether it is the end of a word too.

   The function is called r_c because primary function in production code
   polls representation, and only then in test code a character is polled.

   If @p poll_representation is true, this function will try polling with a
   function used for getting from the receiver a representation (string of
   dots/dashes) of received data. In case of polling a space having a
   representation (dots/dashes) doesn't make sense, but the polling function
   also returns "is-inter-word-space" flag.

   If @p poll_representation is false, this function will try polling a
   character (represented by integer-typed variable). That polling function
   also returns "is-inter-word-space" flag.

   This test code does a verification of data returned by the first poll
   described above. The verification is done during a second poll that does
   the polling using the "opposite" function. Results of both polls are
   compared with each other.

   @reviewedon 2023.08.26

   @param[in/out] easy_rec Receiver from which to poll character
   @param[in] poll_representation Flag indicating which method to use for polling
*/
static void receiver_poll_space(cw_easy_legacy_receiver_t * easy_rec, bool poll_representation)
{
	/* Recheck the receive buffer for end of word. */

	/* We expect the receiver to contain a character, but we don't ask for it
	   this time. The receiver should also store information about an
	   inter-character-space. If it is longer than a regular
	   inter-character-space, then the receiver will treat it as
	   inter-word-space, and communicate it over is_iws.

	   Don't use easy_rec->main_timer - it is used eclusively for marking
	   initial "key down" events. Use local throw-away local_timer. */
	struct timeval local_timer;
	gettimeofday(&local_timer, NULL); /* TODO acerion 2023.08.26: use monotonic clock instead of wall clock. */
	//fprintf(stderr, "receiver_poll_space(): %10ld : %10ld\n", local_timer.tv_sec, local_timer.tv_usec);


	cw_rec_data_t erd = { 0 };
	/* TODO acerion 2023.08.26: investigate why we don't check return value
	   of cw_receive_X() here. */
	if (poll_representation) {
		LIBCW_TEST_FUT(cw_receive_representation)(&local_timer, erd.representation, &erd.is_iws, NULL);
	} else {
		LIBCW_TEST_FUT(cw_receive_character)(&local_timer, NULL, &erd.is_iws, NULL);
	}
	if (erd.is_iws) {
		fprintf(stderr, "[II] Polled inter-word-space\n");

		/* Test code. */
		{
			/* Test currently polled data. */
			if (poll_representation) {
				expect_correct_receive_on_space_r_c(g_cte, &erd, &local_timer);
			} else {
				expect_correct_receive_on_space_c_r(g_cte, &erd, &local_timer);
			}

			/* Append current data to accumulator for later tests. */
			cw_rec_tester_t * tester = (cw_rec_tester_t *) easy_rec->rec_tester;
			tester->received_string[tester->received_string_i++] = ' ';
		}

		cw_clear_receive_buffer();
		easy_rec->is_pending_iws = false;
	} else {
		/* We don't reset is_pending_iws. The space that currently lasts, and
		   isn't long enough to be considered inter-word-space, may grow to
		   become the inter-word-space. Or not.

		   This growing of inter-character-space into inter-word-space may be
		   terminated by incoming next tone (key down event) - the tone will
		   mark beginning of new character within the same word. And since a
		   new character begins, the flag will be reset (elsewhere). */
	}
}




/**
   @brief Top-level test function

   Function hiding two modes in which the tested functionality can be tested.

   @reviewed 2023.08.26

   @param cte Test executor
*/
cwt_retv legacy_api_test_rec_poll(cw_test_executor_t * cte)
{
	if (0 != legacy_api_test_rec_poll_inner(cte, false)) {
		return cwt_retv_err;
	}
	if (0 != legacy_api_test_rec_poll_inner(cte, true)) {
		return cwt_retv_err;
	}
	return cwt_retv_ok;
}




/**
   If @p poll_representation is true, then the test code will first poll
   representation from the receiver, then during verification it will poll
   character, then it will compare results of the two polls.

   If on the other hand @p poll_representation is false, the opposite will
   happen: first a poll of character from the receiver, then a second
   (verifying) poll of representation, and then - again - a comparison of
   results of the two polls.

   @reviewedon 2023.08.26

   @param cte Test executor
   @param[in] poll_representation Mode of testing
 */
static int legacy_api_test_rec_poll_inner(cw_test_executor_t * cte, bool poll_representation)
{
	cte->print_test_header(cte, __func__);
	g_cte = cte;

	if (poll_representation) {
		cte->log_info(cte, "Test mode: poll representation, verify by polling character\n");
	} else {
		cte->log_info(cte, "Test mode: poll character, verify by polling representation\n");
	}

	if (CW_SUCCESS != cw_generator_new(cte->current_gen_conf.sound_system, cte->current_gen_conf.sound_device)) {
		kite_log(cte, LOG_ERR, "failed to create generator\n");
		return -1;
	}


	cw_easy_legacy_receiver_t * easy_rec = cw_easy_legacy_receiver_new();
	cw_clear_receive_buffer();
	cw_set_frequency(cte->config->frequency);
	cw_generator_start();
	cw_enable_adaptive_receive();

	/* Register handler as the CW library keying event callback.

	   The handler called back by libcw is important because it's
	   used to send to libcw information about timings of events
	   (key down and key up events) through easy_rec.main_timer.

	   Without the callback the library can play sounds as key or
	   paddles are pressed, but (since it doesn't receive timing
	   parameters) it won't be able to identify entered Morse
	   code. */
	cw_register_keying_callback(cw_easy_legacy_receiver_handle_libcw_keying_event, easy_rec);
	gettimeofday(&easy_rec->main_timer, NULL);
	//fprintf(stderr, "time on aux config: %10ld : %10ld\n", easy_rec->main_timer.tv_sec, easy_rec->main_timer.tv_usec);

	/* TODO acerion 2023.08.26: there should be some way to specify test
	   string in a place that is using the tester. */
	cw_rec_tester_t tester = { 0 };
	cw_rec_tester_init(&tester);
	cw_rec_tester_configure(&tester, easy_rec, true);
	cw_rec_tester_start_test_code(&tester);

	/* Prepare easy_rec object. */
	easy_rec->rec_tester = &tester;
	easy_rec->get_representation = poll_representation;

	while (tester.generating_in_progress) {
		/* This program is polling a receiver for data. Polling
		   happens at given interval. */
		cw_millisleep_internal(CW_REC_MINIMAL_POLL_PERIOD_MSECS);

		receiver_poll_data(easy_rec);
		int new_speed = 0;
		if (cwtest_param_ranger_get_next(&tester.speed_ranger, &new_speed)) {
			cw_gen_set_speed(tester.helper_gen, new_speed);
		}
	}

	/*
	  Stop thread with test code.

	  TODO: Is this really needed? The thread should already be stopped
	  if we get here. Calling this function leads to this problem in valgrind:

	  ==2402==    by 0x596DEDA: pthread_cancel_init (unwind-forcedunwind.c:52)
	  ==2402==    by 0x596A4EF: pthread_cancel (pthread_cancel.c:38)
	  ==2402==    by 0x1118BE: tester_stop_test_code (libcw_legacy_api_tests_rec_poll.c:958)
	  ==2402==    by 0x1118BE: legacy_api_test_rec_poll_inner (libcw_legacy_api_tests_rec_poll.c:1181)
	  ==2402==    by 0x111C6D: legacy_api_test_rec_poll (libcw_legacy_api_tests_rec_poll.c:1098)
	  ==2402==    by 0x11E1D5: iterate_over_test_objects (test_framework.c:1372)
	  ==2402==    by 0x11E1D5: iterate_over_sound_systems (test_framework.c:1329)
	  ==2402==    by 0x11E1D5: iterate_over_topics (test_framework.c:1306)
	  ==2402==    by 0x11E1D5: cw_test_main_test_loop (test_framework.c:1282)
	  ==2402==    by 0x10E703: main (test_main.c:130)
	*/
	/* TODO: remove this function altogether. */
	//cw_rec_tester_stop_test_code(&tester);

	bool test_passes = false;
	cw_rec_tester_evaluate_receive_correctness(&tester, &test_passes);
	if (cte->expect_op_int(cte, true, "==", test_passes, "Final comparison of receive correctness")) {
		fprintf(stderr, "[II] Test result: success\n");
	} else {
		fprintf(stderr, "[EE] Test result: failure\n");
	}

	/* Tell legacy objects of libcw (those in production code) to stop working. */
	cw_complete_reset();
	cw_generator_stop();
	cw_generator_delete();

	cw_easy_legacy_receiver_delete(&easy_rec);

	cte->print_test_footer(cte, __func__);

	return 0;
}




/**
   @brief Verify correctness of receive event upon receiving a non-inter-word-space character

   This function is called when the character is received with
   cw_receive_character(). What the function does is it polls from the
   receiver the same character, but with cw_receive_representation(). Then it
   compares data received using these two methods - the data should match
   because it represents the same receive event, just obtained by two
   different (but very closely related) functions.

   Result of verification is recorded in @p cte.

   @reviewedon 2023.08.26

   @param[in/out] cte Test executor
   @param[in] erd Data obtained from receiver
   @param[in] timer Timer at which receive event occurred

   @return 0 if verification was executed without interruptions
   @return -1 if verification was aborted
*/
static int expect_correct_receive_on_character_c_r(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer)
{
	/* We need this variable only because most of tests in the function are
	   _errors_only(), so if there are no errors, a success won't be
	   recorded. The final call to expect() will check this flag and will
	   record (hopefully) a success. */
	bool test_ok = true;

	/* Helper 'data' variable for the additional readback from receiver, done
	   with the other function. */
	cw_rec_data_t test = { 0 };

	cw_ret_t cwret = cw_receive_representation(timer, test.representation, &test.is_iws, &test.is_error);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS, "Test poll representation (c_r)");

	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", erd->is_iws,
	                                                    "Comparing 'is inter-word-space' flags: %d, %d", test.is_iws, erd->is_iws);

	/* We are polling a character here, so we expect that receiver will set
	   'is inter-word-space' flag to false. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", false,
	                                                    "Evaluating 'is inter-word-space' flag");

	test.character = cw_representation_to_character(test.representation);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, 0, "!=", test.character, "Lookup character for representation");

	test_ok = test_ok && cte->expect_op_int_errors_only(cte,  erd->character, "==", test.character,
	                                                    "Compare polled and looked up character: %c, %c",
	                                                    erd->character, test.character);

	fprintf(stderr, "[II] Poll character: %c -> '%s' -> %c\n",
	        erd->character, test.representation, test.character);

	cte->expect_op_int(cte, test_ok, "==", true, "Polling character");

	return 0;
}




/**
   @brief Verify correctness of receive event upon receiving a non-inter-word-space character

   This function is called when the character is received with
   cw_receive_representation(). What the function does is it polls from the
   receiver the same character, but with cw_receive_character(). Then it
   compares data received using these two methods - the data should match
   because it represents the same receive event, just obtained by two
   different (but very closely related) functions.

   Result of verification is recorded in @p cte.

   @reviewedon 2023.08.26

   @param[in/out] cte Test executor
   @param[in] erd Data obtained from receiver
   @param[in] timer Timer at which receive event occurred

   @return 0 if verification was executed without interruptions
   @return -1 if verification was aborted
 */
static int expect_correct_receive_on_character_r_c(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer)
{
	/* We need this variable only because most of tests in the function are
	   _errors_only(), so if there are no errors, a success won't be
	   recorded. The final call to expect() will check this flag and will
	   record (hopefully) a success. */
	bool test_ok = true;

	/* Helper 'data' variable for the additional readback from receiver, done
	   with the other function. */
	cw_rec_data_t test = { 0 };

	cw_ret_t cwret = cw_receive_character(timer, &test.character, &test.is_iws, &test.is_error);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
	                                                    "Test poll character (in r_c)");

	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", erd->is_iws,
	                                                    "Comparing 'is inter-word-space' flags: %d, %d",
	                                                    test.is_iws, erd->is_iws);

	/* We are polling a character here, so we expect that receiver will set
	   'is inter-word-space' flag to false. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", false,
	                                                    "Evaluating 'is inter-word-space' flag");

	char * looked_up_representation = cw_character_to_representation(test.character);
	test_ok = test_ok && cte->expect_valid_pointer_errors_only(cte, looked_up_representation,
	                                                           "Lookup representation of character");

	const int cmp = strcmp(erd->representation, looked_up_representation);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, cmp, "==", 0,
	                                                    "Compare polled and looked up representation: '%s', '%s'",
	                                                    erd->representation, looked_up_representation);

	fprintf(stderr, "[II] Poll representation: '%s' -> %c -> '%s'\n", erd->representation, test.character, looked_up_representation);

	/* This is needed to allow caller to accumulate received data. Smells
	   like a workaround. */
	erd->character = test.character;

	cte->expect_op_int(cte, test_ok, "==", true, "Poll representation");

	free(looked_up_representation);

	return 0;
}




/**
   @brief Verify correctness of receive event upon receiving a inter-word-space character

   Result of verification is recorded in @p cte.

   @reviewedon 2023.08.26

   @param[in/out] cte Test executor
   @param[in] erd Data obtained from receiver
   @param[in] timer Timer at which receive event occurred

   @return 0 if verification was executed without interruptions
   @return -1 if verification was aborted
 */
static int expect_correct_receive_on_space_r_c(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer)
{
	/* We need this variable only because most of tests in the function are
	   _errors_only(), so if there are no errors, a success won't be
	   recorded. The final call to expect() will check this flag and will
	   record (hopefully) a success. */
	bool test_ok = true;

	/* Helper 'data' variable for the additional readback from receiver, done
	   with the other function. */
	cw_rec_data_t test = { 0 };
#if 0
	/* cw_receive_character() will return through 'c' variable the last
	   character that was polled before space.

	   Maybe this is good, maybe this is bad, but this is the legacy
	   behaviour that we will keep supporting. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space");
#endif

	cw_ret_t cwret = cw_receive_character(timer, &test.character, &test.is_iws, &test.is_error);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
	                                                    "Getting character during space");

	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", erd->is_iws,
	                                                    "Comparing 'is inter-word-space' flags: %d, %d",
	                                                    test.is_iws, erd->is_iws);

	/* We are polling ' ' space here, so we expect that receiver will set 'is
	   inter-word-space' flag to true. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, true, "==", test.is_iws,
	                                                    "Evaluating 'is inter-word-space' flag");

	cte->expect_op_int(cte, test_ok, "==", true, "Polling inter-word-space");

	return 0;
}




/**
   @brief Verify correctness of receive event upon receiving a inter-word-space character

   Result of verification is recorded in @p cte.

   @reviewedon 2023.08.26

   @param[in/out] cte Test executor
   @param[in] erd Data obtained from receiver
   @param[in] timer Timer at which receive event occurred

   @return 0 if verification was executed without interruptions
   @return -1 if verification was aborted
 */
static int expect_correct_receive_on_space_c_r(cw_test_executor_t * cte, cw_rec_data_t * erd, const struct timeval * timer)
{
	/* We need this variable only because most of tests in the function are
	   _errors_only(), so if there are no errors, a success won't be
	   recorded. The final call to expect() will check this flag and will
	   record (hopefully) a success. */
	bool test_ok = true;

	/* Helper 'data' variable for the additional readback from receiver, done
	   with the other function. */
	cw_rec_data_t test = { 0 };
#if 0
	/* cw_receive_character() will return through 'c' variable the last
	   character that was polled before space.

	   Maybe this is good, maybe this is bad, but this is the legacy
	   behaviour that we will keep supporting. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, c, "!=", ' ', "returned character should not be space");
#endif

	cw_ret_t cwret = cw_receive_representation(timer, test.representation, &test.is_iws, &test.is_error);
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, cwret, "==", CW_SUCCESS,
	                                                    "Getting representation during space");

	test_ok = test_ok && cte->expect_op_int_errors_only(cte, test.is_iws, "==", erd->is_iws,
	                                                    "Comparing 'is inter-word-space' flags: %d, %d",
	                                                    test.is_iws, erd->is_iws);

	/* We are polling ' ' space here, so we expect that receiver will set 'is
	   inter-word-space' flag to true. */
	test_ok = test_ok && cte->expect_op_int_errors_only(cte, true, "==", test.is_iws,
	                                                    "Evaluating 'is inter-word-space' flag");

	cte->expect_op_int(cte, test_ok, "==", true, "Polling inter-word-space");

	return 0;
}


