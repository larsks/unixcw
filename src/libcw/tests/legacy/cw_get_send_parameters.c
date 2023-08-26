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
  with this program. If not, see <https://www.gnu.org/licenses/>.
*/




/**
   @file cw_get_send_parameters.c

   Tests of cw_get_send_parameters().
*/




#include "libcw_gen.h"
#include "cw_get_send_parameters.h"
#include "common.h"




/**
   @reviewed on 2019-10-13
*/
int legacy_api_test_cw_get_send_parameters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);
	legacy_api_standalone_test_setup(cte, true);

	for (int speed = CW_SPEED_MIN; speed <= CW_SPEED_MAX; speed++) {

		cw_set_send_speed(speed);

		cw_gen_durations_t params = {
			.dot_duration = -1,
			.dash_duration = -1,
			.ims_duration = -1,
			.ics_duration = -1,
			.iws_duration = -1,
			.additional_space_duration = -1,
			.adjustment_space_duration = -1,
		};

		LIBCW_TEST_FUT(cw_get_send_parameters)
			(&params.dot_duration,
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

	legacy_api_standalone_test_teardown(cte);
	cte->print_test_footer(cte, __func__);

	return cwt_retv_ok;
}




