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
	char buffer[20 + 1];
	int iter;
	cw_test_executor_t * cte;
} callback_data_t;




static void callback(void * cdata, cw_easy_rec_data_t * erd)
{
	callback_data_t * data = (callback_data_t *) cdata;
	if (erd->is_iws) {
		data->cte->cte_log(data->cte, LOG_DEBUG, "received character ' '\n");
		data->buffer[data->iter] = ' ';
	} else {
		data->cte->cte_log(data->cte, LOG_DEBUG, "received character '%c'\n", erd->character);
		data->buffer[data->iter] = erd->character;
	}
	data->iter++;
}




/**
   @brief Test removing a character from end of enqueued characters

   @reviewed on 2023-07-17
*/
cwt_retv test_cw_gen_remove_last_character(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	cw_gen_t * gen = NULL;
	if (0 != helper_gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}

	cw_easy_rec_t * easy_rec = cw_easy_rec_new();
	cw_easy_rec_set_speed(easy_rec, cw_gen_get_speed(gen));
	cw_gen_register_value_tracking_callback_internal(gen, cw_easy_rec_handle_libcw_keying_event, easy_rec);

	callback_data_t data = { .buffer = { 0 }, .iter = 0, .cte = cte };
	cw_easy_rec_register_receive_callback(easy_rec, callback, &data);
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

			cte->expect_strcasecmp(cte, expected_results[chars_to_remove], data.buffer, "Removal of last %d character(s)", chars_to_remove);
		}
	}

	cw_easy_rec_stop(easy_rec);

	helper_gen_destroy(&gen);
	cw_easy_rec_delete(&easy_rec);

	cte->expect_op_int(cte, false, "==", failure, "remove last character");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

