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
   \file common.c

   \brief Common code for different tests
*/




#include <errno.h>
#include <signal.h>

#include "../libcw_gen.h"
#include "../libcw_gen_internal.h"
#include "../libcw_rec.h"
#include "../libcw_utils.h"
#include "common.h"




int legacy_api_standalone_test_setup(cw_test_executor_t * cte, bool start_gen)
{
	if (CW_SUCCESS != cw_generator_new(cte->current_gen_conf.sound_system, cte->current_gen_conf.sound_device)) {
		cte->log_error(cte, "Can't create generator, stopping the test\n");
		return -1;
	}
	if (start_gen) {
		if (CW_SUCCESS != cw_generator_start()) {
			cte->log_error(cte, "Can't start generator, stopping the test\n");
			cw_generator_delete();
			return -1;
		}
	}

	/* Reset all parameters to defaults. Recalculate duration parameters
	   based on defaults. */
	cw_reset_send_receive_parameters();

	if (CW_SUCCESS != cw_set_gap(0)) { /* This function sets gap in both generator and receiver. */
		kite_log(cte, LOG_ERR, "%s:%d: Can't prepare legacy API testing, failed at setting gen/rec gap\n", __func__, __LINE__);
		return -1;
	}
	if (CW_SUCCESS != cw_set_send_speed(cte->config->send_speed)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't prepare legacy API testing, failed at setting generator speed %d\n", __func__, __LINE__, cte->config->send_speed);
		return -1;
	}
	if (CW_SUCCESS != cw_set_frequency(cte->config->frequency)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't prepare legacy API testing, failed at setting generator frequency %d\n", __func__, __LINE__, cte->config->frequency);
		return -1;
	}
	if (CW_SUCCESS != cw_set_volume(cte->config->volume)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't prepare legacy API testing, failed at setting generator volume %d\n", __func__, __LINE__, cte->config->volume);
		return -1;
	}
	/* Duration parameters have been recalculated internally on new values of
	   gap and speed, in calls to set_gap() or set_speed(). */

	cw_disable_adaptive_receive();
	cw_reset_receive_statistics();
	cw_unregister_signal_handler(SIGUSR1);
	errno = 0;

	return 0;
}




int legacy_api_standalone_test_teardown(__attribute__((unused)) cw_test_executor_t * cte)
{
	cw_usleep_internal(1 * CW_USECS_PER_SEC);
	cw_generator_stop();
	cw_usleep_internal(1 * CW_USECS_PER_SEC);
	cw_generator_delete();

	return 0;
}




int gen_setup(cw_test_executor_t * cte, cw_gen_t ** gen)
{
	*gen = cw_gen_new(&cte->current_gen_conf);
	if (!*gen) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't create generator, stopping the test\n", __func__, __LINE__);
		return -1;
	}
	cw_gen_reset_parameters_internal(*gen);
	if (CW_SUCCESS != cw_gen_set_speed(*gen, cte->config->send_speed)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't create generator, failed at setting speed %d\n", __func__, __LINE__, cte->config->send_speed);
		return -1;
	}
	if (CW_SUCCESS != cw_gen_set_frequency(*gen, cte->config->frequency)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't create generator, failed at setting frequency %d\n", __func__, __LINE__, cte->config->frequency);
		return -1;
	}
	if (CW_SUCCESS != cw_gen_set_volume(*gen, cte->config->volume)) {
		kite_log(cte, LOG_ERR, "%s:%d: Can't create generator, failed at setting volume %d\n", __func__, __LINE__, cte->config->volume);
		return -1;
	}
	cw_gen_sync_parameters_internal(*gen);

	return 0;
}




void gen_destroy(cw_gen_t ** gen)
{
	cw_gen_delete(gen); /* This function sets **gen to NULL. */
}




int test_rec_params_relations(cw_test_executor_t * cte, const cw_rec_parameters_t * params, int tolerance)
{
	/* With zero tolerance on duration of elements, the minimum and maximum
	   of an element's duration equals the ideal duration of element (the
	   margin of durations is zero microseconds). */
	const char * operator = 0 == tolerance ? "==" : "<";

	/* Lower and upper limit of range of durations for dot are in proper order. */
	cte->expect_op_int(cte, params->dot_duration_min,    operator, params->dot_duration_ideal,  "Receiver's params relations: dot range min");
	cte->expect_op_int(cte, params->dot_duration_ideal,  operator, params->dot_duration_max,    "Receiver's params relations: dot range max");

	/* Lower and upper limit of range of durations for dash are in proper order. */
	cte->expect_op_int(cte, params->dash_duration_min,   operator, params->dash_duration_ideal, "Receiver's params relations: dash range min");
	cte->expect_op_int(cte, params->dash_duration_ideal, operator, params->dash_duration_max,   "Receiver's params relations: dash range max");

	/* Lower and upper limit of range of durations for inter-mark-space are in proper order. */
	cte->expect_op_int(cte, params->ims_duration_min,    operator, params->ims_duration_ideal,  "Receiver's params relations: ims range min");
	cte->expect_op_int(cte, params->ims_duration_ideal,  operator, params->ims_duration_max,    "Receiver's params relations: ims range max");

	/* Lower and upper limit of durations for inter-character-space are in proper order. */
	cte->expect_op_int(cte, params->ics_duration_min,    operator, params->ics_duration_ideal,  "Receiver's params relations: ics range min");
	cte->expect_op_int(cte, params->ics_duration_ideal,  operator, params->ics_duration_max,    "Receiver's params relations: ics range max");

	/* Range of durations of dot must not overlap with range of durations of
	   dash. */
	cte->expect_op_int(cte, params->dot_duration_max, "<", params->dash_duration_min, "Receiver's params relations: dot vs. dash: overlap");

	/* Range of durations of inter-mark-space must not overlap with range of durations of
	   inter-character-space. */
	cte->expect_op_int(cte, params->ims_duration_max, "<", params->ics_duration_min, "Receiver's params relations: ims vs ics: overlap");

	/* By "standard", ideal dash is 3 times ideal dot. */
	cte->expect_op_int(cte, params->dash_duration_ideal, "==", 3 * params->dot_duration_ideal, "Receiver's params relations: dot vs dash: duration");

	/* By "standard", an ideal inter-character-space is 3 times an ideal inter-mark-space. */
	cte->expect_op_int(cte, params->ics_duration_ideal, "==", 3 * params->ims_duration_ideal, "Receiver's params relations: ics vs ims: duration");

	/* By "standard", an inter-mark-space has the same duration as dot. */
	cte->expect_op_int(cte, params->ims_duration_ideal, "==", params->dot_duration_ideal, "Receiver's params relations: ims vs dot: duration");

	return 0;
}




/*
  This function assumes that weighting and gap are zero, and therefore don't
  influence durations' relations.
*/
int test_gen_params_relations(cw_test_executor_t * cte, const cw_gen_durations_t * params, int speed)
{
	/* Let's hope that those divisions (one when calculating dot duration
	   in gen, and the other here) work out correctly. */
	cte->expect_op_int_errors_only(cte, CW_DOT_CALIBRATION / speed, "==", params->dot_duration,  "confirm dot duration at speed %d wpm", speed);

	/* TODO: acerion 2023.08.26: "dot_duration" or "unit_duration"? */
	cte->expect_op_int(cte,             3 * params->dot_duration, "==", params->dash_duration, "confirm dash duration relative to dot at %d wpm", speed);
	cte->expect_op_int(cte, UNITS_PER_IMS * params->dot_duration, "==", params->ims_duration,  "confirm ims duration relative to dot at %d wpm", speed);
	cte->expect_op_int(cte, UNITS_PER_ICS * params->dot_duration, "==", params->ics_duration,  "confirm ics duration relative to dot at %d wpm", speed);
	cte->expect_op_int(cte, UNITS_PER_IWS * params->dot_duration, "==", params->iws_duration,  "confirm iws duration relative to dot at %d wpm", speed);

	/* As far as I know these two should be zero in generator running
	   with default configuration. */
	cte->expect_op_int(cte, 0, "==", params->additional_space_duration, "confirm additional space duration at %d wpm", speed);
	cte->expect_op_int(cte, 0, "==", params->adjustment_space_duration, "confirm adjustment space duration at %d wpm", speed);

	/* Confirm a statement from comment in
	   cw_timestamp_compare_internal():

	   "At 4 WPM, the dash duration is 3*(1200000/4)=900,000 usecs, and
	   the inter-word-space is 2,100,000 usecs. */
	if (4 == speed) {
		cte->expect_op_int(cte,  900000, "==", params->dash_duration, "confirm specific dash duration at %d wpm", speed);
		cte->expect_op_int(cte, 2100000, "==", params->iws_duration,  "confirm specific iws duration at %d wpm", speed);
	}

	return 0;
}

