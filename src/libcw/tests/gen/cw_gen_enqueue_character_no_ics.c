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

#include <cw_easy_rec.h>

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
	char buffer[20 + 1];
	int iter;
	cw_test_executor_t * cte;
} callback_data_t;




static void callback(cw_easy_rec_data_t * erd, void * cdata)
{
	callback_data_t * data = (callback_data_t *) cdata;
	if (erd->is_iws) {
		data->cte->cte_log(data->cte, LOG_DEBUG, "Received character ' '\n");
		data->buffer[data->iter] = ' ';
	} else {
		data->cte->cte_log(data->cte, LOG_DEBUG, "Received character '%c'\n", erd->character);
		data->buffer[data->iter] = erd->character;
	}
	data->iter++;
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
	   them. */
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

   @reviewed on 2023-07-25
*/
cwt_retv test_cw_gen_enqueue_character_no_ics(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	cw_gen_t * gen = NULL;
	if (cwt_retv_ok != gen_setup(cte, &gen)) {
		cte->log_error(cte, "%s:%d: Failed to create generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen);

	cw_easy_rec_t * easy_rec = cw_easy_rec_new();
	cw_easy_rec_set_speed(easy_rec, cw_gen_get_speed(gen));

	/*
	  15%; few times lower than the default. With relatively low and constant
	  generator speed this should be enough. With 10% and PulseAudio I
	  already experienced problems.

	  TODO: the value of tolerance should be different for different sound
	  systems.
	*/
	cw_easy_rec_set_tolerance(easy_rec, 15);

	cw_gen_register_value_tracking_callback_internal(gen, cw_easy_rec_handle_libcw_keying_event, easy_rec);

	callback_data_t data = { .buffer = { 0 }, .iter = 0, .cte = cte };
	cw_easy_rec_register_receive_callback(easy_rec, callback, &data);
	cw_easy_rec_start(easy_rec);


	bool failure = false;
	int t = 0;
	while (NULL != g_test_data[t].expected_result) {

		memset(&data, 0, sizeof (data));
		data.cte = cte;


		int i = 0;
		while ('\0' != g_test_data[t].input[i]) {
			cw_ret_t cwret = LIBCW_TEST_FUT(cw_gen_enqueue_character_no_ics)(gen, g_test_data[t].input[i]);
			if (!cte->expect_op_int_errors_only(cte, CW_SUCCESS, "==", cwret,
			                                    "Call to cw_gen_enqueue_character_no_ics(), input #%d ([%s], character #%d ('%c)",
			                                    t, g_test_data[t].input, i, g_test_data[t].input[i])) {
				failure = true;
				break;
			}
			i++;
		}

		cw_gen_wait_for_queue_level(gen, 0);
		cw_usleep_internal(1000 * 1000);

		// Clearing of receiver appears to be unnecessary.
		//cw_easy_rec_clear(easy_rec);

		/* The main part of the test: comparing enqueued string with what has
		   been played and received. */
		cte->expect_strcasecmp(cte, g_test_data[t].expected_result, data.buffer, "Enqueue of character without ics, in: [%s], out: [%s]",
		                       g_test_data[t].input, data.buffer);
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
