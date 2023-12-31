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
#include <string.h>
#include <unistd.h>

#include <cwutils/cw_easy_rec.h>

#include "common.h"
#include "libcw.h"
#include "libcw2.h"
#include "libcw_debug.h"
#include "libcw_gen.h"
#include "libcw_gen_tests.h"
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




static int get_tolerance(cw_test_executor_t * cte, cw_sound_system_t sound_system, int * tolerance);




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

	int tolerance = 0;
	if (0 != get_tolerance(cte, cte->current_gen_conf.sound_system, &tolerance)) {
		return cwt_retv_err;
	}

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

	cw_easy_rec_set_tolerance(easy_rec, tolerance);

	/* Helper receiver will be notified through callback about each change of
	   tested generator's state (mark/space). */
	cw_gen_register_value_tracking_callback_internal(gen, cw_easy_rec_handle_libcw_keying_event, easy_rec);

	callback_data_t data = { .accumulator = { 0 }, .acc_iter = 0, .cte = cte };
	/* Configure callback to be called each time a helper receiver makes a
	   successful receive of something sent by tested generator. */
	cw_easy_rec_register_receive_callback(easy_rec, receive_callback, &data);
	cw_easy_rec_start(easy_rec);

#if 0  /* Debug. */
	kite_log(cte, LOG_DEBUG, "Generator speed = %d\n", cw_gen_get_speed(gen));
	float rec_speed = 0.0F;
	cw_easy_rec_get_speed(easy_rec, &rec_speed);
	kite_log(cte, LOG_DEBUG, "Receiver speed = %.2f\n", (double) rec_speed);
#endif

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
		if (!cte->expect_strcasecmp(cte, g_test_data[t].expected_result, data.accumulator,
									"Enqueue of char w/o ics, in: [%s], out: [%s]",
									g_test_data[t].input, data.accumulator)) {
			failure = true;
		}
		t++;
	}

	cw_easy_rec_stop(easy_rec);
	cw_easy_rec_delete(&easy_rec);

	cw_gen_stop(gen);
	gen_destroy(&gen);

	kite_on_test_completion(cte, __func__, failure ? test_result_fail : test_result_pass);

	return cwt_retv_ok;
}




/**
   @brief Get receiver's minimal tolerance for given sound system

   With different sound systems the receiver may be more or less tolerant to
   timings of incoming Morse code. This function is a wrapper that maps a
   sound system to a tolerance. If you want to know what is the minimal
   tolerance with which a receiver will receive correctly for given sound
   system, then use this function.

   @reviewedon 2023.08.29

   @param[in/out] cte Text executor - used only for logging
   @param[in] sound_system Sound system for which to get receiver tolerance
   @param[out] tolerance The receiver's minimal tolerance for given sound system

   @return 0 if getting a tolerance succeeded
   @return -1 otherwise (e.g. @p sound_system has unexpected value)
*/
static int get_tolerance(cw_test_executor_t * cte, cw_sound_system_t sound_system, int * tolerance)
{
	/* Definitions necessary just to avoid "magic number" warnings. */

	/* At 12 wpm and 5% I experienced problems. With 12% it was ok. 15%
	   should be safe for all my test machines. */
#define TOLERANCE_NULL    10

	/* TODO acerion 2023.08.29: update after tests with console sound
	   system. */
#define TOLERANCE_CONSOLE  5

	/* TODO acerion 2023.08.29: update after tests with OSS sound system. */
#define TOLERANCE_OSS      6

	/* FIXME acerion 2023.08.29: on my main PC even with MAX tolerance I
	   can't get a clean pass for ALSA. Something is clearly wrong with ALSA
	   sound system. I've got similar results when I back-ported the test to
	   unixcw 3.6.0. */
#define TOLERANCE_ALSA    CW_TOLERANCE_MAX
	/* At 12 wpm and 15% I experienced problems. With 22% it was ok. 25%
	   should be safe for all my test machines. */
#define TOLERANCE_PA      22

	switch (sound_system) {
	case CW_AUDIO_NULL:
		*tolerance = TOLERANCE_NULL;
		break;
	case CW_AUDIO_CONSOLE:
		*tolerance = TOLERANCE_CONSOLE;
		break;
	case CW_AUDIO_OSS:
		*tolerance = TOLERANCE_OSS;
		break;
	case CW_AUDIO_ALSA:
		*tolerance = TOLERANCE_ALSA;
		break;
	case CW_AUDIO_PA:
		 *tolerance = TOLERANCE_PA;
		break;
	case CW_AUDIO_SOUNDCARD:
		/* This sound system is known, but not expected in this
		   place. Tests are for specific sound systems, not for
		   catch-all "soundcard" sound system. */
		kite_log(cte, LOG_ERR, "Unexpected sound system %d\n", sound_system);
		return -1;
	case CW_AUDIO_NONE:
	default:
		kite_log(cte, LOG_ERR, "Unknown sound system %d\n", sound_system);
		return -1;
	};

	return 0;
}

