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


#include "config.h"




#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include <assert.h>

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif




#include "common.h"
#include "libcw.h"
#include "libcw_rec.h"
#include "libcw_debug.h"
//#include "libcw_legacy_api_tests.h"
#include "cw_get_receive_parameters.h"







int legacy_api_test_cw_get_receive_parameters(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, __func__);

	if (0 != CW_TOLERANCE_MIN) {
		/* We really want to test at zero tolerance, so if for some reason
		   MIN is not zero, then we will be unhappy. */
		cte->cte_log(cte, LOG_ERR, "Unexpected value of CW_TOLERANCE_MIN: %d (expected zero)\n", CW_TOLERANCE_MIN);
		return -1;
	}

	legacy_api_standalone_test_setup(cte, false);

	const int orig_tolerance = cw_get_tolerance();
	const int tolerances[] = { CW_TOLERANCE_MIN, CW_TOLERANCE_INITIAL, CW_TOLERANCE_MAX };
	const size_t n_tolerances = sizeof (tolerances) / sizeof (tolerances[0]);

	for (size_t i = 0; i < n_tolerances; i++) {
		const int tolerance = tolerances[i];

		if (CW_SUCCESS != cw_set_tolerance(tolerance)) {
			cte->cte_log(cte, LOG_ERR, "Failed to set minimum tolerance of receiver\n");
			return -1;
		}

		cw_rec_parameters_t params = { 0 };
		LIBCW_TEST_FUT(cw_get_receive_parameters)
			(&params.dot_duration_ideal,
			 &params.dash_duration_ideal,
			 &params.dot_duration_min,
			 &params.dot_duration_max,
			 &params.dash_duration_min,
			 &params.dash_duration_max,
			 &params.ims_duration_min,
			 &params.ims_duration_max,
			 &params.ims_duration_ideal,
			 &params.ics_duration_min,
			 &params.ics_duration_max,
			 &params.ics_duration_ideal,
			 &params.adaptive_threshold);


		if (0 != test_rec_params_relations(cte, &params, tolerance)) {
			return -1;
		}

	}

	/* Restore original tolerance of a receiver. Other receiver tests will
	   rely on the receiver using default tolerance. */
	if (CW_SUCCESS != cw_set_tolerance(orig_tolerance)) {
		cte->cte_log(cte, LOG_ERR, "Failed to restore original tolerance of receiver\n");
		return -1;
	}
	legacy_api_standalone_test_teardown(cte);

	cte->print_test_footer(cte, __func__);

	return 0;
}




