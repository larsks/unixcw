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
   @file cw_gen_enqueue_character_no_ics.c

   Test of cw_gen_enqueue_character_no_ics().
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
#include "cw_gen_enqueue_character_no_ics.h"




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
		kite_log(data->cte, LOG_DEBUG, "Received character ' '\n");
		data->accumulator[data->acc_iter] = ' ';
	} else {
		kite_log(data->cte, LOG_DEBUG, "Received character '%c'\n", erd->character);
		data->accumulator[data->acc_iter] = erd->character;
	}
	data->acc_iter++;
}




/*
  The test data reflects this table from man page 'cw.7':

  An alternative view of punctuation and procedural signals is as combination Morse characters:
  Ch   Prosig      Ch   Prosig
  ─────────────────────────────
  "    [AF]        '    [WG]
  $    [SX]        (    [KN]
  )    [KK]        +    [AR]
  ,    [MIM]       -    [DU]
  .    [AAA]       /    [DN]
  :    [OS]        ;    [KR]
  =    [BT]        ?    [IMI]
  _    [IQ]        @    [AC]
  <    [VA],[SK]   >    [BK]
  !    [SN]        &    [AS]
  ^    [KA]        ~    [AL]
*/

/* There are spaces at the end of strings because the generator adds
   "inter-word-space" after playing a string. */
static const struct {
	/* String to be played: two (or three) characters that form a procedural
	   signal. */
	const char * input;

	/* Expected result of receiving: a punctuation character formed by
	   playing two input characters with just an inter-mark-space between
	   them. The expected result includes inter-word-space added by tested
	   generator. */
	const char * expected_result;
} g_test_data[] = {
	{ "AF",     "\""  " " },
	{ "SX",     "$"   " " },
	{ "KK",     ")"   " " },
	{ "MIM",    ","   " " },
	{ "AAA",    "."   " " },
	{ "OS",     ":"   " " },
	{ "BT",     "="   " " },
	{ "IQ",     "_"   " " },

	{ "VA",     "<"   " " },
	{ "SK",     "<"   " " },

	{ "SN",     "!"   " " },
	{ "KA",     "^"   " " },
	{ "WG",     "'"   " " },
	{ "KN",     "("   " " },
	{ "AR",     "+"   " " },
	{ "DU",     "-"   " " },
	{ "DN",     "/"   " " },
	{ "KR",     ";"   " " },
	{ "IMI",    "?"   " " },
	{ "AC",     "@"   " " },
	{ "BK",     ">"   " " },
	{ "AS",     "&"   " " },
	{ "AL",     "~"   " " },
	{ "",       NULL      } /* Guard. */
};





/**
   @brief Test enqueueing and playing of procedural characters

   See if enqueueing two specific characters that form procedural signals,
   without inter-character-space between them, will be played correctly and
   received as a single (punctuation) character.

   This test confirms that a duration of space between two characters
   enqueued in such a way is more or less just an inter-mark-space.

   @reviewed on 2023-08-26

   @param cte test executor

   @return cwt_retv_ok if execution of the test was carried out
   @return cwt_retv_err if execution of the test had to be aborted
*/
cwt_retv test_cw_gen_enqueue_character_no_ics(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	/* Tested generator. */
	cw_gen_t * gen = NULL;
	if (0 != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	/* Helper receiver. */
	cw_easy_rec_t * easy_rec = cw_easy_rec_new();
	cw_easy_rec_set_speed(easy_rec, cw_gen_get_speed(gen));

	/*
	  15%; few times lower than the default. With relatively low and constant
	  generator speed this should be enough. With 10% and PulseAudio I
	  already experienced problems.

	  TODO acerion 2023.08.26: the value of tolerance should be different for
	  different sound systems.
	*/
	cw_easy_rec_set_tolerance(easy_rec, 15);

	/* Helper receiver will be notified through callback about each change of
	   tested generator's state (mark/space). */
	cw_gen_register_value_tracking_callback_internal(gen, cw_easy_rec_handle_libcw_keying_event, easy_rec);

	callback_data_t data = { .accumulator = { 0 }, .acc_iter = 0, .cte = cte };
	/* Configure callback to be called each time a helper receiver makes a
	   successful receive of something sent by tested generator. */
	cw_easy_rec_register_receive_callback(easy_rec, receive_callback, &data);
	cw_easy_rec_start(easy_rec);


	bool failure = false;
	int t = 0;
	while (NULL != g_test_data[t].expected_result) {

		memset(&data, 0, sizeof (data));
		data.cte = cte;


		/* Enqueue characters from test data. Normally enqueued characters
		   would be generated and then received as two (or more) separate
		   characters. But since the function-under-test doesn't insert
		   inter-character-space between them, then the generator will play
		   something that is a sum of input characters. That "sum of input
		   characters" will be then received by helper receiver as a single
		   character, different from the individual input characters. */

		   int i = 0;
		while ('\0' != g_test_data[t].input[i]) {
			cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character_no_ics)(gen, g_test_data[t].input[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret,
			                                    "Call to cw_gen_enqueue_character_no_ics(), input #%d ([%s], character #%d ('%c))",
			                                    t, g_test_data[t].input, i, g_test_data[t].input[i])) {
				failure = true;
				break;
			}
			i++;
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_usleep_internal(1 * CW_USECS_PER_SEC);

		// Clearing of receiver appears to be unnecessary.
		//cw_easy_rec_clear(easy_rec);

		/* The main part of the test: comparing enqueued string with what has
		   been played and received. */
		cte->expect_strcasecmp(cte, g_test_data[t].expected_result, data.accumulator,
		                       "Enqueue of character without ics, in: [%s], out: [%s]",
		                       g_test_data[t].input, data.accumulator);
		t++;
	}

	cw_easy_rec_stop(easy_rec);
	cw_easy_rec_delete(&easy_rec);

	cw_gen_stop(gen);
	gen_destroy(&gen);

	cte->expect_op_int(cte, false, "==", failure, "Enqueue character without ics");

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

