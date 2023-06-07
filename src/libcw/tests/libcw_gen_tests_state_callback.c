/*
  Copyright (C) 2022-2023  Kamil Ignacak (acerion@wp.pl)

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




#include <math.h> /* fabs() */
#include <stdlib.h> /* exit() */




#include "libcw2.h"




#include "libcw_data.h"
#include "libcw_gen_tests_state_callback.h"
#include "libcw_gen.h"
#include "libcw_utils.h"




/*
  This test is verifying if callback called on each change of state of
  generator is called at proper intervals.

  Client code can use cw_gen_register_value_tracking_callback_internal() to
  register a callback. The callback will be called each time the generator
  changes it's state between mark/space. The changes of state should occur at
  time intervals specified by length of marks (dots, dashes) and spaces.

  libcw is using sound card's (sound system's) blocking write property to
  measure how often and for how long a generator should stay in particular
  state. Generator will write e.g. mark to sound system, the blocking write
  will block the generator for specified time, and then the generator will be
  able to get from tone queue the next element (mark or space) and do another
  blocking write to sound system.

  Calls to the callback are made between each blocking write. The calls to
  the callback will be made with smaller or larger precision, depending on:

  1. the exact mechanism used by libcw. For now libcw uses blocking write,
     but e.g. ALSA is offering other mechanisms that are right now
     unexplored.

  2. how well the libcw has configured the mechanism. Commit
     cb99e7884dc5370519dc3a3eacfb3184959b0f87 has fixed an error in
     configuration of ALSA's HW period size. That incorrect configuration has
     led to the callback being called at totally invalid (very, very short)
     intervals.

  I started writing this test to verify that the fix from commit
  cb99e7884dc5370519dc3a3eacfb3184959b0f87 has been working, but then I
  thought that the test can work for any sound system supported by libcw. I
  don't need to verify if low-level configuration of ALSA is working
  correctly, I just need to verify if the callback mechanism (relying on
  proper configuration of a sound system) is working correctly.
*/




/*
  Custom type for dots, dashes and spaces just for the purpose of this test.
  The enum members are also visual representations of the dots, dashes and
  spaces.
*/
typedef enum {
	dot  = '.',
	dash = '-',
	ims  = 'M',
	ics  = 'C',
	iws  = 'W'
} element_type_t;




/* Ideal durations of dots, dashes and spaces, as reported by libcw for given
   wpm speed [microseconds]. */
static cw_durations_t g_durations;




/* How much durations of real dots, dashes and spaces diverge from ideal
   ones? */
typedef struct divergence_t {
	double min; /* [percentages] */
	double avg; /* [percentages] */
	double max; /* [percentages] */
} divergence_t;




typedef struct cw_element_t {
	element_type_t type;
	int duration; /* microseconds */
} cw_element_t;




typedef struct cw_element_stats_t {
	int duration_min;
	int duration_avg;
	int duration_max;

	int duration_total;
	int count;
} cw_element_stats_t;




/* Type of data passed to callback.
   We need to store a persistent state of some data between callback calls. */
typedef struct callback_data_t {
	struct timeval prev_timestamp; /* Timestamp at which previous callback was made. */
	int element_idx; /* Index to g_test_input_elements[]. */
} callback_data_t;




typedef struct test_data_t {
	/* Sound system for which we have reference data, and with which the
	   current run of a test should be done. */
	enum cw_audio_systems sound_system;

	/* Speed (WPM) for which we have reference data, and at which current
	   run of a test should be done (otherwise we will be comparing
	   results of tests made in different conditions). */
	int speed;

	/* Reference values from tests in post_3.5.1 branch. */
	struct divergence_t reference_div_dots;
	struct divergence_t reference_div_dashes;
	struct divergence_t reference_div_ims;
	struct divergence_t reference_div_ics;
	struct divergence_t reference_div_iws;

	/* Values obtained in current test run. */
	struct divergence_t current_div_dots;
	struct divergence_t current_div_dashes;
	struct divergence_t current_div_ims;
	struct divergence_t current_div_ics;
	struct divergence_t current_div_iws;
} test_data_t;




static void gen_callback_fn(void * callback_arg, int state);
static void update_element_stats(cw_element_stats_t * stats, int element_duration);
static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const divergence_t * divergences, const char * name, int duration_expected);
static void calculate_divergences_from_stats(const cw_element_stats_t * stats, divergence_t * divergences, int duration_expected);
static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, cw_durations_t * durations);

static void calculate_test_results(const cw_element_t * elements, int n_elements, test_data_t * test_data, const cw_durations_t * durations);
static void evaluate_test_results(cw_test_executor_t * cte, test_data_t * test_data);

static int ideal_duration_of_element(element_type_t type, cw_durations_t * durations);

static int initialize_elements(const char * string, cw_element_t * elements);
static void clear_data(cw_element_t * elements);




/**
  Results of test from reference branch post_3.5.1 with:
  1. fixed ALSA HW period size,
  2. modified PA parameters, copied from these two commits:
  https://github.com/m5evt/unixcw-3.5.1/commit/2d5491a461587ac4686e2d1b897619c98be05c9e
  https://github.com/m5evt/unixcw-3.5.1/commit/c86785b595a6d711aae915150df2ccb848ace05c

  For OSS sound system I'm copying results for ALSA.

  For Console sound system I'm copying results for Null sound system
  (both systems simulate a blocking write with sleep function).

  For ims/ics/iws I'm just copying data for dot, at least for now. Maybe I
  will adjust the data in the future..
*/
static test_data_t g_test_data[] = {

	/* sound system                      speed       expected divergence: dots      expected divergence: dashes    expected divergence: ims       expected divergence: ics       expected divergence: iws */
	{ .sound_system = CW_AUDIO_NULL,    .speed =  4, {   0.041,   0.068,   0.075 }, {   0.014,   0.023,   0.027 }, {   0.041,   0.068,   0.075 }, {   0.041,   0.068,   0.075 }, {   0.041,   0.068,   0.075 } },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 12, {   0.113,   0.189,   0.241 }, {   0.034,   0.064,   0.079 }, {   0.113,   0.189,   0.241 }, {   0.113,   0.189,   0.241 }, {   0.113,   0.189,   0.241 } },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 24, {   0.220,   0.316,   0.444 }, {   0.077,   0.131,   0.170 }, {   0.220,   0.316,   0.444 }, {   0.220,   0.316,   0.444 }, {   0.220,   0.316,   0.444 } },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 36, {   0.354,   0.492,   0.702 }, {   0.150,   0.218,   0.261 }, {   0.354,   0.492,   0.702 }, {   0.354,   0.492,   0.702 }, {   0.354,   0.492,   0.702 } },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 60, {   0.630,   0.890,   1.145 }, {   0.238,   0.343,   0.427 }, {   0.630,   0.890,   1.145 }, {   0.630,   0.890,   1.145 }, {   0.630,   0.890,   1.145 } },

	{ .sound_system = CW_AUDIO_CONSOLE, .speed =  4, {   0.041,   0.068,   0.075 }, {   0.014,   0.023,   0.027 }, {   0.041,   0.068,   0.075 }, {   0.041,   0.068,   0.075 }, {   0.041,   0.068,   0.075 } },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 12, {   0.113,   0.189,   0.241 }, {   0.034,   0.064,   0.079 }, {   0.113,   0.189,   0.241 }, {   0.113,   0.189,   0.241 }, {   0.113,   0.189,   0.241 } },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 24, {   0.220,   0.316,   0.444 }, {   0.077,   0.131,   0.170 }, {   0.220,   0.316,   0.444 }, {   0.220,   0.316,   0.444 }, {   0.220,   0.316,   0.444 } },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 36, {   0.354,   0.492,   0.702 }, {   0.150,   0.218,   0.261 }, {   0.354,   0.492,   0.702 }, {   0.354,   0.492,   0.702 }, {   0.354,   0.492,   0.702 } },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 60, {   0.630,   0.890,   1.145 }, {   0.238,   0.343,   0.427 }, {   0.630,   0.890,   1.145 }, {   0.630,   0.890,   1.145 }, {   0.630,   0.890,   1.145 } },

	{ .sound_system = CW_AUDIO_OSS,     .speed =  4, {  -2.304,  -1.263,  -0.172 }, {  -0.838,  -0.475,  -0.191 }, {  -2.304,  -1.263,  -0.172 }, {  -2.304,  -1.263,  -0.172 }, {  -2.304,  -1.263,  -0.172 } },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 12, {  -6.692,  -3.542,  -1.060 }, {  -2.413,  -1.410,  -0.301 }, {  -6.692,  -3.542,  -1.060 }, {  -6.692,  -3.542,  -1.060 }, {  -6.692,  -3.542,  -1.060 } },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 24, { -10.538,  -5.278,  -1.056 }, {  -5.349,  -1.480,   3.331 }, { -10.538,  -5.278,  -1.056 }, { -10.538,  -5.278,  -1.056 }, { -10.538,  -5.278,  -1.056 } },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 36, { -14.388,  -1.671,  23.991 }, {  -6.334,  -1.211,   6.537 }, { -14.388,  -1.671,  23.991 }, { -14.388,  -1.671,  23.991 }, { -14.388,  -1.671,  23.991 } },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 60, { -39.465, -19.180, -16.215 }, { -12.443,  -6.295,  -3.437 }, { -39.465, -19.180, -16.215 }, { -39.465, -19.180, -16.215 }, { -39.465, -19.180, -16.215 } },

	{ .sound_system = CW_AUDIO_ALSA,    .speed =  4, {  -2.304,  -1.263,  -0.172 }, {  -0.838,  -0.475,  -0.191 }, {  -2.304,  -1.263,  -0.172 }, {  -2.304,  -1.263,  -0.172 }, {  -2.304,  -1.263,  -0.172 } },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 12, {  -6.692,  -3.542,  -1.060 }, {  -2.413,  -1.410,  -0.301 }, {  -6.692,  -3.542,  -1.060 }, {  -6.692,  -3.542,  -1.060 }, {  -6.692,  -3.542,  -1.060 } },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 24, { -10.538,  -5.278,  -1.056 }, {  -5.349,  -1.480,   3.331 }, { -10.538,  -5.278,  -1.056 }, { -10.538,  -5.278,  -1.056 }, { -10.538,  -5.278,  -1.056 } },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 36, { -14.388,  -1.671,  23.991 }, {  -6.334,  -1.211,   6.537 }, { -14.388,  -1.671,  23.991 }, { -14.388,  -1.671,  23.991 }, { -14.388,  -1.671,  23.991 } },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 60, { -39.465, -19.180, -16.215 }, { -12.443,  -6.295,  -3.437 }, { -39.465, -19.180, -16.215 }, { -39.465, -19.180, -16.215 }, { -39.465, -19.180, -16.215 } },

	{ .sound_system = CW_AUDIO_PA,      .speed =  4, {  -1.830,  -0.156,   1.110 }, {  -0.288,   0.022,   0.754 }, {  -1.830,  -0.156,   1.110 }, {  -1.830,  -0.156,   1.110 }, {  -1.830,  -0.156,   1.110 } },
	{ .sound_system = CW_AUDIO_PA,      .speed = 12, {  -2.726,  -0.063,   6.259 }, {  -1.479,  -0.041,   1.003 }, {  -2.726,  -0.063,   6.259 }, {  -2.726,  -0.063,   6.259 }, {  -2.726,  -0.063,   6.259 } },
	{ .sound_system = CW_AUDIO_PA,      .speed = 24, {  -9.252,   0.004,   6.846 }, {  -4.668,  -0.306,   1.743 }, {  -9.252,   0.004,   6.846 }, {  -9.252,   0.004,   6.846 }, {  -9.252,   0.004,   6.846 } },
	{ .sound_system = CW_AUDIO_PA,      .speed = 36, { -16.257,  -0.690,   7.242 }, {  -2.522,   0.153,   5.280 }, { -16.257,  -0.690,   7.242 }, { -16.257,  -0.690,   7.242 }, { -16.257,  -0.690,   7.242 } },
	{ .sound_system = CW_AUDIO_PA,      .speed = 60, { -24.430,  -0.205,  27.640 }, {  -4.085,   0.083,   8.597 }, { -24.430,  -0.205,  27.640 }, { -24.430,  -0.205,  27.640 }, { -24.430,  -0.205,  27.640 } },
};




/* Test string that will be played by test. */
//static const char * const g_input_string = "one two three four";
static const char * const g_input_string = "ooo""ooo""ooo sss""sss""sss";




#define INPUT_ELEMENTS_COUNT 128
static cw_element_t g_test_input_elements[INPUT_ELEMENTS_COUNT];
/* Count of valid elements in the array. Depends on length of g_input_string. */
static int g_test_input_elements_count;




/**
   Get ideal (expected) duration of given element (dot, dash, spaces)
*/
static int ideal_duration_of_element(element_type_t type, cw_durations_t * durations)
{
	switch (type) {
	case dot:
		return durations->dot_usecs;
	case dash:
		return durations->dash_usecs;
	case ims:
		return durations->ims_usecs;
	case ics:
		return durations->ics_usecs;
	case iws:
		return durations->iws_usecs;
	default:
		fprintf(stderr, "[ERROR] Unexpected element type '%c'\n", type);
		return 1;
	}
}




/**
   @brief Callback function called on change of state of generator (open -> closed, or closed -> open)

   @param[in/out] callback_arg callback's private data, registered in generator together with callback itself
   @param[in] state state of generator: 0 == opened (no sound, space) or 1 == closed (sound, mark)
*/
static void gen_callback_fn(void * callback_arg, int state)
{
	/*
	  The callback should be as fast as possible. This flag controls the speed.
	  true: allow non-essential code executed by callback (increases execution time of callback).
	  false: disable non-essential code.

	  TODO: measure difference in time of execution of the callback. Maybe
	  the flag doesn't have that much impact.
	*/
	const bool execute_nonessential = true;

	callback_data_t * callback_data = (callback_data_t *) callback_arg;

	struct timeval now_timestamp = { 0 };
	gettimeofday(&now_timestamp, NULL);
	struct timeval prev_timestamp = callback_data->prev_timestamp;

	const int this_idx = callback_data->element_idx;
	cw_element_t * this_element = &g_test_input_elements[this_idx];
	if (execute_nonessential) {
		/* Check that state is consistent with element. */
		if (state) {
			if (dot != this_element->type && dash != this_element->type) {
				fprintf(stderr, "[ERROR] Unexpected element #%03d: '%c' for state 'closed'\n", this_idx, this_element->type);
			}
		} else {
			if (iws != this_element->type && ics != this_element->type && ims != this_element->type) {
				fprintf(stderr, "[ERROR] Unexpected element #%03d: '%c' for state 'open'\n", this_idx, this_element->type);
			}
		}
	}

	callback_data->element_idx++;
	callback_data->prev_timestamp = now_timestamp;

	cw_element_t * prev_element = NULL;
	if (this_idx == 0) {
		/* Don't do anything for zero-th element, for which there is no 'prev
		   timestamp'. */
		return;
	} else {
		/* Update previous element. We are at the beginning of new element,
		   and currently calculated duration is how long *previous* element
		   was. */
		prev_element = &g_test_input_elements[this_idx - 1];
		prev_element->duration = cw_timestamp_compare_internal(&prev_timestamp, &now_timestamp);
	}

	if (execute_nonessential) {
		const int prev_duration_expected = ideal_duration_of_element(prev_element->type, &g_durations);
		const double divergence = 100.0 * (prev_element->duration - prev_duration_expected) / (1.0 * prev_duration_expected);

#if 0 /* For debugging only. */
		fprintf(stderr, "[DEBUG] type = '%c', prev_duration = %7d, prev_duration_expected = %7d\n", prev_element->type, prev_element->duration, prev_duration_expected);
#endif
		fprintf(stderr, "[INFO ] Element %3d, state %d, type = '%c'; previous element: duration = %7d us, divergence = %8.3f%%\n",
		        this_idx, state, this_element->type, prev_element->duration, divergence);
	}
}




static void update_element_stats(cw_element_stats_t * stats, int element_duration)
{
	stats->duration_total += element_duration;
	stats->count++;
	stats->duration_avg = stats->duration_total / stats->count;

	if (element_duration > stats->duration_max) {
		stats->duration_max = element_duration;
	}
	if (element_duration < stats->duration_min) {
		stats->duration_min = element_duration;
	}
}




static void calculate_divergences_from_stats(const cw_element_stats_t * stats, divergence_t * divergences, int duration_expected)
{
	divergences->min = 100.0 * (stats->duration_min - duration_expected) / (1.0 * duration_expected);
	divergences->avg = 100.0 * (stats->duration_avg - duration_expected) / (1.0 * duration_expected);
	divergences->max = 100.0 * (stats->duration_max - duration_expected) / (1.0 * duration_expected);
}




static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const divergence_t * divergences, const char * name, int duration_expected)
{
	fprintf(stderr, "[INFO ] duration of %-6s: min/avg/max = %7d/%7d/%7d, expected = %7d, divergence min/avg/max = %8.3f%%/%8.3f%%/%8.3f%%\n",
	        name,
	        stats->duration_min,
	        stats->duration_avg,
	        stats->duration_max,
	        duration_expected,
	        divergences->min,
	        divergences->avg,
	        divergences->max);
}




/* Top-level test function. */
cwt_retv test_cw_gen_state_callback(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	g_test_input_elements_count = initialize_elements(g_input_string, g_test_input_elements);

	cwt_retv retv = cwt_retv_ok;
	const size_t n_tests = sizeof (g_test_data) / sizeof (g_test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		test_data_t * test_data = &g_test_data[i];
		if (CW_AUDIO_NONE == test_data->sound_system) {
			continue;
		}
		if (cte->current_gen_conf.sound_system != test_data->sound_system) {
			continue;
		}
		if (cwt_retv_ok != test_cw_gen_state_callback_sub(cte, test_data, cte->current_gen_conf.sound_device, &g_durations)) {
			retv = cwt_retv_err;
			break;
		}
	}

	cte->print_test_footer(cte, __func__);

	return retv;
}




static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, cw_durations_t * durations)
{
	cw_gen_config_t gen_conf = { .sound_system = test_data->sound_system };
	snprintf(gen_conf.sound_device, sizeof (gen_conf.sound_device), "%s", sound_device);
	cw_gen_t * gen = cw_gen_new(&gen_conf);
	cw_gen_set_speed(gen, test_data->speed);
	cw_gen_set_frequency(gen, cte->config->frequency);

	callback_data_t callback_data = { 0 };
	cw_gen_register_value_tracking_callback_internal(gen, gen_callback_fn, &callback_data);



	cw_gen_get_durations_internal(gen, durations);
	fprintf(stderr, "[INFO ] dot duration        = %7d us\n", durations->dot_usecs);
	fprintf(stderr, "[INFO ] dash duration       = %7d us\n", durations->dash_usecs);
	fprintf(stderr, "[INFO ] ims duration        = %7d us\n", durations->ims_usecs);
	fprintf(stderr, "[INFO ] ics duration        = %7d us\n", durations->ics_usecs);
	fprintf(stderr, "[INFO ] iws duration        = %7d us\n", durations->iws_usecs);
	fprintf(stderr, "[INFO ] additional duration = %7d us\n", durations->additional_usecs);
	fprintf(stderr, "[INFO ] adjustment duration = %7d us\n", durations->adjustment_usecs);
	fprintf(stderr, "[INFO ] speed               = %d WPM\n", test_data->speed);


	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, g_input_string);
	cw_gen_wait_for_queue_level(gen, 0);


	cw_gen_stop(gen);
	cw_gen_delete(&gen);


	calculate_test_results(g_test_input_elements, g_test_input_elements_count, test_data, durations);
	evaluate_test_results(cte, test_data);
	clear_data(g_test_input_elements);

	return 0;
}




/**
   Calculate current divergences (from current run of test) that will be
   compared with reference values
*/
static void calculate_test_results(const cw_element_t * elements, int n_elements, test_data_t * test_data, const cw_durations_t * durations)
{
	const int initial = 1000000000;
	cw_element_stats_t stats_dot  = { .duration_min = initial, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };
	cw_element_stats_t stats_dash = { .duration_min = initial, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };
	cw_element_stats_t stats_ims  = { .duration_min = initial, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };
	cw_element_stats_t stats_ics  = { .duration_min = initial, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };
	cw_element_stats_t stats_iws  = { .duration_min = initial, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };

	/* Skip first and last element. The way the test is structured may impact
	   correctness of values of these elements. TODO: make the elements
	   correct. */
	for (int i = 1; i < n_elements - 1; i++) {
		switch (elements[i].type) {
		case dot:
			update_element_stats(&stats_dot, elements[i].duration);
			break;
		case dash:
			update_element_stats(&stats_dash, elements[i].duration);
			break;
		case ims:
			update_element_stats(&stats_ims, elements[i].duration);
			break;
		case ics:
			update_element_stats(&stats_ics, elements[i].duration);
			break;
		case iws:
			update_element_stats(&stats_iws, elements[i].duration);
			break;
		default:
			break;
		}
	}

	calculate_divergences_from_stats(&stats_dot, &test_data->current_div_dots, durations->dot_usecs);
	calculate_divergences_from_stats(&stats_dash, &test_data->current_div_dashes, durations->dash_usecs);
	calculate_divergences_from_stats(&stats_ims, &test_data->current_div_ims, durations->ims_usecs);
	calculate_divergences_from_stats(&stats_ics, &test_data->current_div_ics, durations->ics_usecs);
	calculate_divergences_from_stats(&stats_iws, &test_data->current_div_iws, durations->iws_usecs);

	print_element_stats_and_divergences(&stats_dot, &test_data->current_div_dots, "dots", durations->dot_usecs);
	print_element_stats_and_divergences(&stats_dash, &test_data->current_div_dashes, "dashes", durations->dash_usecs);
	print_element_stats_and_divergences(&stats_ims, &test_data->current_div_ims, "ims", durations->ims_usecs);
	print_element_stats_and_divergences(&stats_ics, &test_data->current_div_ics, "ics", durations->ics_usecs);
	print_element_stats_and_divergences(&stats_iws, &test_data->current_div_iws, "iws", durations->iws_usecs);
}




/**
   Compare test results from current test run with reference data. Update
   test results in @p cte.
*/
static void evaluate_test_results(cw_test_executor_t * cte, test_data_t * test_data)
{
	/* Margin above 1.0: allow current results to be slightly worse than reference.
	   Margin below 1.0: accept current results only if they are better than reference. */
	const double margin = 1.5;

	{
		const double expected_div = fabs(test_data->reference_div_dots.min) * margin;
		const double current_div = fabs(test_data->current_div_dots.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dots.avg) * margin;
		const double current_div = fabs(test_data->current_div_dots.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dots.max) * margin;
		const double current_div = fabs(test_data->current_div_dots.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, max");
	}


	{
		const double expected_div = fabs(test_data->reference_div_dashes.min) * margin;
		const double current_div = fabs(test_data->current_div_dashes.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dashes.avg) * margin;
		const double current_div = fabs(test_data->current_div_dashes.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dashes.max) * margin;
		const double current_div = fabs(test_data->current_div_dashes.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, max");
	}


	{
		const double expected_div = fabs(test_data->reference_div_ims.min) * margin;
		const double current_div = fabs(test_data->current_div_ims.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ims, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_ims.avg) * margin;
		const double current_div = fabs(test_data->current_div_ims.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ims, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_ims.max) * margin;
		const double current_div = fabs(test_data->current_div_ims.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ims, max");
	}


	{
		const double expected_div = fabs(test_data->reference_div_ics.min) * margin;
		const double current_div = fabs(test_data->current_div_ics.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ics, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_ics.avg) * margin;
		const double current_div = fabs(test_data->current_div_ics.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ics, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_ics.max) * margin;
		const double current_div = fabs(test_data->current_div_ics.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of ics, max");
	}


	{
		const double expected_div = fabs(test_data->reference_div_iws.min) * margin;
		const double current_div = fabs(test_data->current_div_iws.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of iws, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_iws.avg) * margin;
		const double current_div = fabs(test_data->current_div_iws.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of iws, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_iws.max) * margin;
		const double current_div = fabs(test_data->current_div_iws.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of iws, max");
	}

	/* TODO: the test should also have test for absolute
	   value of divergence, not only for comparison with
	   post_3.5.1 branch. The production code should aim
	   at low absolute divergence, e.g. no higher than 3%. */
}




/**
   Convert given string into test elements' types

   @param[in] string string to be used as input of tests
   @param[out] elements array of marks, spaces, their types and their timings
*/
static int initialize_elements(const char * string, cw_element_t * elements)
{
	int e = 0;

	int s = 0;
	while (string[s] != '\0') {

		if (string[s] == ' ') {
			/* ' ' character is represented by iws. This is a special case
			   because this character doesn't have its "natural"
			   representation in form of dots and dashes. */
			if (e > 0 && (elements[e - 1].type == ims || elements[e - 1].type == ics)) {
				/* Overwrite last end-of-element. */
				elements[e - 1].type = iws;
				/* No need to increment 'e' as we are not adding new element. */
			} else {
				elements[e].type = iws;
				e++;
			}
		} else {
			/* Regular (non-space) character has its Morse representation.
			   Get the representation, and copy each dot/dash into
			   'elements'. Add ims after each dot/dash. */
			const char * representation = cw_character_to_representation_internal(string[s]);
			int r = 0;
			while (representation[r] != '\0') {
				switch (representation[r]) {
				case '.':
					elements[e].type = dot;
					break;
				case '-':
					elements[e].type = dash;
					break;
				default:
					break;
				};
				r++;
				e++;
				elements[e].type = ims;
				e++;

			}
			/* Turn ims after last mark (the last mark in character) into ics. */
			elements[e - 1].type = ics;
		}
		s++;
	}

	if (e > INPUT_ELEMENTS_COUNT) {
		fprintf(stderr, "[ERROR] Count of elements (%d) exceeds available space (%d)\n", e, INPUT_ELEMENTS_COUNT);
		exit(EXIT_FAILURE);
	}
	const int elements_count = e;

#if 0 /* For debugging only. */
	for (int i = 0; i < elements_count; i++) {
		fprintf(stderr, "[DEBUG] Initialized element %3d with type '%c'\n", i, elements[i].type);
	}
#endif

	return elements_count;
}




/**
   Clear data accumulated in current test run. The function should be used as
   a preparation for next test run.
*/
static void clear_data(cw_element_t * elements)
{
	/* Clear durations calculated in current test before next call of
	   this test function. */
	for (int e = 0; e < INPUT_ELEMENTS_COUNT; e++) {
		elements[e].duration = 0;
	}
}

