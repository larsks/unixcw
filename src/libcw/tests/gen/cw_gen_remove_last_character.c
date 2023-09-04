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




/**
   @file cw_gen_remove_last_character.c

   Test of cw_gen_remove_last_character().
*/




#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cwutils/cw_easy_rec.h>

#include "common.h"
#include "libcw.h"
#include "libcw2.h"
#include "libcw_debug.h"
#include "libcw_gen.h"
#include "libcw_gen_tests.h"
#include "libcw_debug.h"
#include "libcw_utils.h"
#include "cw_gen_remove_last_character.h"




/* This is where received characters will be appended. Given that input
   string has up to 8 characters, I don't expect the receiver to require more
   than 20 slots for received characters. */
typedef struct callback_data {
	char accumulator[20 + 1]; /**< Accumulator of received characters. */
	int acc_iter;             /**< Iterator for the accumulator, pointing to first free slot in the accumulator. */
	cw_test_executor_t * cte; /**< Test executor variable. */
} callback_data_t;




/**
   @brief Callback called by easy receiver on each receive event

   @p cdata is an object in this problem that knows what to do when the
   callback is called by easy receiver, and knows how to handle the contents
   of @p erd.

   The object accumulates characters received by helper receiver from a
   tested generator

   @reviewedon 2023.08.26

   @param[in/out] cdata Pointer to an object in this program
   @param[in] erd Easy receiver data - variable storing result of the receive event
*/
static void receive_callback(void * cdata, cw_easy_rec_data_t * erd)
{
	callback_data_t * data = (callback_data_t *) cdata;
	if (erd->is_iws) {
		kite_log(data->cte, LOG_DEBUG, "received character ' '\n");
		data->accumulator[data->acc_iter] = ' ';
	} else {
		kite_log(data->cte, LOG_DEBUG, "received character '%c'\n", erd->character);
		data->accumulator[data->acc_iter] = erd->character;
	}
	data->acc_iter++;
}




/**
   @brief Test removing a character from end of enqueued characters

   @reviewed on 2023-08-26

   @param cte test executor

   @return cwt_retv_ok if execution of the test was carried out
   @return cwt_retv_err if execution of the test had to be aborted
*/
cwt_retv test_cw_gen_remove_last_character(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	/* Tested generator. */
	cw_gen_t * gen = NULL;
	if (0 != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create tested generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}

	/* Helper receiver will be used to detect what a tested generator has
	   played (whether removed characters are sent or not). */
	cw_easy_rec_t * easy_rec = cw_easy_rec_new();
	cw_easy_rec_set_speed(easy_rec, cw_gen_get_speed(gen));
	/* Helper receiver will be notified through callback about each change of
	   tested generator's state (mark/space). */
	cw_gen_register_value_tracking_callback_internal(gen, cw_easy_rec_handle_libcw_keying_event, easy_rec);

	callback_data_t data = { .accumulator = { 0 }, .acc_iter = 0, .cte = cte };
	/* Configure callback to be called each time a helper receiver makes a
	   successful receive of something sent by tested generator. */
	cw_easy_rec_register_receive_callback(easy_rec, receive_callback, &data);
	cw_easy_rec_start(easy_rec);

	const char * input_string = "oooo" "ssss";

	/* This test will attempt to remove zero to N characters from end of
	   enqueued string. */
#define REMOVED_CHARS_MAX 4

	bool failure = false;
	for (int chars_to_remove = 0; chars_to_remove <= REMOVED_CHARS_MAX; chars_to_remove++) {
		cw_gen_start(gen);
		memset(&data, 0, sizeof (data));
		data.cte = cte;

		cte->log_info(cte, "You will now hear 'oooo' followed by %d 's' characters\n", REMOVED_CHARS_MAX - chars_to_remove);
		cw_gen_enqueue_string(gen, input_string);

		/* cw_gen_enqueue_string() is non-blocking - it returns immediately.
		   Now we can try to remove some characters from end of generator's
		   queue, before generator gets to play them. */

		/* Remove n characters from end. */
		for (int i = 0; i < chars_to_remove; i++) {
			cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_remove_last_character(gen));
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret,
							    "remove last %d characters, removing %d-th character",
							    chars_to_remove, i)) {
				failure = true;
				break;
			}
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_usleep_internal(1 * CW_USECS_PER_SEC);
		cw_gen_stop(gen);
		// Clearing of receiver appears to be unnecessary.
		//cw_easy_rec_clear(easy_rec);

		if (failure) {
			break;
		}

		/* The main part of the test: comparing enqueued string with what has
		   been played and received. There are spaces at the end of strings
		   because the generator adds "inter-word-space" after playing a
		   string. */
		{
			const char * expected_results[REMOVED_CHARS_MAX + 1] = {
				"oooo" "ssss" " ",
				"oooo" "sss"  " ",
				"oooo" "ss"   " ",
				"oooo" "s"    " ",
				"oooo" ""     " "
			};

			cte->expect_strcasecmp(cte, expected_results[chars_to_remove], data.accumulator,
			                       "Removal of last %d character(s)", chars_to_remove);
		}
	}

	cw_easy_rec_stop(easy_rec);

	gen_destroy(&gen);
	cw_easy_rec_delete(&easy_rec);

	cte->expect_op_int(cte, false, "==", failure, "Remove last character");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

