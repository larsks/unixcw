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
   @file cw_gen_get_timing_parameters_internal.c

   Test of cw_gen_get_timing_parameters_internal()
*/




#include "common.h"
#include "libcw_gen.h"
#include "cw_gen_get_timing_parameters_internal.h"




/**
   @brief Test cw_gen_get_timing_parameters_internal()

   The tested function returns durations of elements used by cw generator.
   The test verifies relations between the durations. These relations are
   described in "man 7 cw" page distributed with unixcw:

       The duration of a dash is three dots.
       The time between each element (dot or dash) is one dot length.
       The space between characters is three dot lengths.
       The space between words is seven dot lengths.
       The following formula calculates the dot period in microseconds from the Morse code speed in words per minute:
          dot period = ( 1200000 / speed )

   Notice that the tested function returns ideal durations. Whether libcw's
   generator really uses them is another matter and it should be tested
   separately.

   @reviewedon 2023.08.26

   @param cte test executor

   @return cwt_retv_ok if execution of the test was carried out without interruptions
   @return cwt_retv_err if execution of the test had to be aborted
*/
cwt_retv test_cw_gen_get_timing_parameters_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	/* Tested generator. */
	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
	if (NULL == gen) {
		cte->log_error(cte, "%s:%d: Failed to create tested generator\n", __func__, __LINE__);
		return cwt_retv_err;
	}
	cw_gen_start(gen); /* TODO acerion 2023.08.26: do we really need to start the generator for this test? */

	for (int speed = CW_SPEED_MIN; speed <= CW_SPEED_MAX; speed++) {

		/* FIXME (acerion) 2023.08.01. Trying to call legacy API here
		   (cw_set_send_speed()) results in segmentation fault. It's expected
		   that calling legacy API in program using modern API won't have
		   impact on a 'gen', but we should not get a sigsegv. Investigate
		   this. */
		cw_gen_set_speed(gen, speed);

		cw_gen_durations_t params = {
			.dot_duration  = -1,
			.dash_duration = -1,
			.ims_duration  = -1,
			.ics_duration  = -1,
			.iws_duration  = -1,
			.additional_space_duration = -1,
			.adjustment_space_duration = -1,
		};

		LIBCW_TEST_FUT(cw_gen_get_timing_parameters_internal)
			(gen,
			 &params.dot_duration,
			 &params.dash_duration,
			 &params.ims_duration,
			 &params.ics_duration,
			 &params.iws_duration,
			 &params.additional_space_duration,
			 &params.adjustment_space_duration);

		kite_log(cte, LOG_DEBUG,
		             "generator's sending parameters at %2d wpm:\n"
		             "    dot duration = %7d us\n"
		             "   dash duration = %7d us\n"
		             "    ims duration = %7d us\n"
		             "    ics duration = %7d us\n"
		             "    iws duration = %7d us\n"
		             "additional space duration = %7d us\n"
		             "adjustment space duration = %7d us\n",
		             speed,
		             params.dot_duration,
		             params.dash_duration,
		             params.ims_duration,
		             params.ics_duration,
		             params.iws_duration,
		             params.additional_space_duration,
		             params.adjustment_space_duration);


		if (0 != test_gen_params_relations(cte, &params, speed)) {
			kite_log(cte, LOG_ERR, "Failed to test generator's relations at %2d wpm", speed);
			return cwt_retv_err;
		}
	}

	cw_gen_delete(&gen);
	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}

