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




#include "libcw_gen.h"
#include "cw_gen_get_timing_parameters_internal.h"




/**
   @reviewed on 2020-05-07
*/
cwt_retv test_cw_gen_get_timing_parameters_internal(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	int initial = -5;

	int dot_duration = initial;
	int dash_duration = initial;
	int ims_duration = initial;
	int ics_duration = initial;
	int iws_duration = initial;
	int additional_space_duration = initial;
	int adjustment_space_duration = initial;

	cw_gen_t * gen = cw_gen_new(&cte->current_gen_conf);
	cw_gen_start(gen);


	cw_gen_reset_parameters_internal(gen);
	/* Reset requires resynchronization. */
	cw_gen_sync_parameters_internal(gen);


	LIBCW_TEST_FUT(cw_gen_get_timing_parameters_internal)(gen,
							      &dot_duration,
							      &dash_duration,
							      &ims_duration,
							      &ics_duration,
							      &iws_duration,
							      &additional_space_duration,
							      &adjustment_space_duration);

	bool failure = (dot_duration == initial)
		|| (dash_duration == initial)
		|| (ims_duration == initial)
		|| (ics_duration == initial)
		|| (iws_duration == initial)
		|| (additional_space_duration == initial)
		|| (adjustment_space_duration == initial);
	cte->expect_op_int(cte, false, "==", failure, "get timing parameters");

	cw_gen_delete(&gen);

	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




