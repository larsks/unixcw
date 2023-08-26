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

#include <cwutils/lib/elements.h>
#include <cwutils/lib/elements_detect.h>
#include <cwutils/lib/element_stats.h>
#include <cwutils/lib/misc.h>

#include "libcw2.h"
#include "libcw_data.h"
#include "libcw_gen_tests_state_callback.h"
#include "libcw_gen.h"
#include "libcw_utils.h"




/**
   @file libcw_gen_tests_state_callback.c

   This test is verifying if callback called on each change of state of
   generator is called at proper intervals.

   Client code can use cw_gen_register_value_tracking_callback_internal() to
   register a callback. The callback will be called each time the generator
   changes it's state between mark/space. The changes of state should occur
   at time intervals specified by duration of marks (dots, dashes) and
   spaces.

   libcw is using sound card's (sound system's) blocking write property to
   measure how often and for how long a generator should stay in particular
   state. Generator will write e.g. mark to sound system, the blocking write
   will block the generator for specified time, and then the generator will
   be able to get from tone queue the next element (mark or space) and do
   another blocking write to sound system.

   Calls to the callback are made between each blocking write. The calls to
   the callback will be made with smaller or larger precision, depending on:

   1. the exact mechanism used by libcw. For now libcw uses blocking write,
      but e.g. ALSA is offering other mechanisms that are right now
      unexplored.

   2. how well the libcw has configured the mechanism. Commit
      cb99e7884dc5370519dc3a3eacfb3184959b0f87 has fixed an error in
      configuration of ALSA's HW period size. That incorrect configuration
      has led to the callback being called at totally invalid (very, very
      short) intervals.

   I started writing this test to verify that the fix from commit
   cb99e7884dc5370519dc3a3eacfb3184959b0f87 has been working, but then I
   thought that the test can work for any sound system supported by libcw. I
   don't need to verify if low-level configuration of ALSA is working
   correctly, I just need to verify if the callback mechanism (relying on
   proper configuration of a sound system) is working correctly.
*/




/*
  This parameter is selected experimentally.

  Ideally the count of elements should be calculated from input string, but
  I didn't want to write a function for it. Instead I chose to select a value
  that is "big enough". If I'm wrong, there are checks in elements code that
  detect attempt to overflow array of elements.
*/
#define ELEMENTS_COUNT_MAX 128




/* Type of data passed to callback.
   We need to store a persistent state of some data between callback calls. */
typedef struct callback_data_t {
	struct timeval prev_timestamp; /* Timestamp at which previous callback was made. */

	int element_idx; /* Index to string_elements->array[]. */
	cw_elements_t * string_elements;

	/* Ideal durations of dots, dashes and spaces, as reported by libcw for given
	   wpm speed [microseconds]. */
	cw_gen_durations_t * durations;
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
	struct cw_element_stats_divergences_t reference_div_dots;
	struct cw_element_stats_divergences_t reference_div_dashes;
	struct cw_element_stats_divergences_t reference_div_ims;
	struct cw_element_stats_divergences_t reference_div_ics;
	struct cw_element_stats_divergences_t reference_div_iws;

	/* Values obtained in current test run. */
	struct cw_element_stats_divergences_t current_div_dots;
	struct cw_element_stats_divergences_t current_div_dashes;
	struct cw_element_stats_divergences_t current_div_ims;
	struct cw_element_stats_divergences_t current_div_ics;
	struct cw_element_stats_divergences_t current_div_iws;
} test_data_t;




static void gen_callback_fn(void * callback_arg, int state);
static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const cw_element_stats_divergences_t * divergences, const char * name, int duration_expected);
static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, const char * input_string);

static void calculate_test_results(const cw_elements_t * elements, test_data_t * test_data, const cw_gen_durations_t * durations);
static void evaluate_test_results(cw_test_executor_t * cte, test_data_t * test_data);




/**
   Results of test from reference branch post_3.5.1 with:
   1. fixed ALSA HW period size,
   2. modified PA parameters, copied from these two commits:
   https://github.com/m5evt/unixcw-3.5.1/commit/2d5491a461587ac4686e2d1b897619c98be05c9e
   https://github.com/m5evt/unixcw-3.5.1/commit/c86785b595a6d711aae915150df2ccb848ace05c

   For OSS sound system I'm copying results for ALSA.

   For Console sound system I'm copying results for Null sound system (both
   systems simulate a blocking write with sleep function).

   For ims/ics/iws I'm just copying data for dot, at least for now. Maybe I
   will adjust the data in the future. (TODO acerion 2023.08.26: review the
   data).
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
	const cw_elements_t * string_elements = callback_data->string_elements;
	const size_t this_idx = callback_data->element_idx;


	struct timeval now_timestamp = { 0 };
	gettimeofday(&now_timestamp, NULL); /* TODO acerion 2023.08.26: use monotonic clock instead of wall clock. */
	struct timeval prev_timestamp = callback_data->prev_timestamp;


	cw_element_t * this_element = &string_elements->array[this_idx];
	if (execute_nonessential) {
		/* Check that state is consistent with element. */
		if (state) {
			if (cw_element_type_dot != this_element->type && cw_element_type_dash != this_element->type) {
				fprintf(stderr, "[ERROR] Unexpected element #%03zd: '%c' for state 'closed'\n", this_idx, cw_element_type_get_representation(this_element->type));
			}
		} else {
			if (cw_element_type_iws != this_element->type && cw_element_type_ics != this_element->type && cw_element_type_ims != this_element->type) {
				fprintf(stderr, "[ERROR] Unexpected element #%03zd: '%c' for state 'open'\n", this_idx, cw_element_type_get_representation(this_element->type));
			}
		}
	}

	callback_data->element_idx++;
	callback_data->prev_timestamp = now_timestamp;
	/* Don't increment string_elements->curr_count because curr_count is
	   indicating how many non-empty elements are there in string_elements.
	   All elements are already appended in there, we are just filling
	   durations. */

	cw_element_t * prev_element = NULL;
	if (this_idx == 0) {
		/* Don't do anything for zero-th element, for which there is no 'prev
		   timestamp'. */
		return;
	} else {
		/* Update previous element. We are at the beginning of new element,
		   and currently calculated duration is how long *previous* element
		   was. */
		prev_element = &string_elements->array[this_idx - 1];
		prev_element->duration = cw_timestamp_compare_internal(&prev_timestamp, &now_timestamp);
	}

	if (execute_nonessential) {
		int prev_duration_expected = 0;
		cw_element_type_to_duration(prev_element->type, callback_data->durations, &prev_duration_expected);
		const double divergence = 100.0 * (prev_element->duration - prev_duration_expected) / (1.0 * prev_duration_expected);

#if 0 /* For debugging only. */
		fprintf(stderr, "[DEBUG] prev element type = '%c', prev duration = %12.2f, prev duration expected = %7d\n",
		        cw_element_type_get_representation(prev_element->type), prev_element->duration, prev_duration_expected);
#endif
		fprintf(stderr, "[INFO ] Element %3zd, state %d, type = '%c'; previous element: duration = %12.2f us, divergence = %8.3f%%\n",
		        this_idx, state, cw_element_type_get_representation(this_element->type), prev_element->duration, divergence);
	}
}




static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const cw_element_stats_divergences_t * divergences, const char * name, int duration_expected)
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

	/*
	  Test string that will be played by test. Length of string may have
	  impact on required value of ELEMENTS_COUNT_MAX.

	  Make sure to include some inter-word-spaces in the text.

	  TODO acerion 2023.08.26: generate this string randomly. Make sure that
	  it contains inter-word-spaces (sometimes even at the beginning and
	  end).
	*/
	const char * const input_string = "one two three four";
	//const char * const input_string = "ooo""ooo""ooo sss""sss""sss";

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
		if (cwt_retv_ok != test_cw_gen_state_callback_sub(cte, test_data, cte->current_gen_conf.sound_device, input_string)) {
			retv = cwt_retv_err;
			break;
		}
	}

	cte->print_test_footer(cte, __func__);

	return retv;
}




static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, const char * input_string)
{
	cw_gen_config_t gen_conf = { .sound_system = test_data->sound_system };
	snprintf(gen_conf.sound_device, sizeof (gen_conf.sound_device), "%s", sound_device);
	cw_gen_t * gen = cw_gen_new(&gen_conf);
	cw_gen_set_speed(gen, test_data->speed);
	cw_gen_set_frequency(gen, cte->config->frequency);

	/* Ideal durations of dots, dashes and spaces, as reported by libcw for given
	   wpm speed [microseconds]. */
	cw_gen_durations_t durations = { 0 };
	cw_gen_get_durations_internal(gen, &durations);
	cw_gen_durations_print(stderr, &durations);
	fprintf(stderr, "[INFO ] speed               = %d WPM\n", test_data->speed);


	/* Elements and their duration for input string. An output from wav file
	   will be compared against this reference data. */
	cw_elements_t * string_elements = cw_elements_new(ELEMENTS_COUNT_MAX);
	if (NULL == string_elements) {
		/* This is treated as developer's error, therefore we exit. Developer
		   should ensure sufficient count of elements for given string. */
		fprintf(stderr, "[ERROR] Failed to allocate string elements for string '%s'\n", input_string);
		exit(EXIT_FAILURE);
	}
	if (0 != cw_elements_detect_from_string(input_string, string_elements)) {
		/* This is treated as developer's error, therefore we exit. Developer
		   should ensure that cw_elements_detect_from_string() work correctly for valid
		   input strings. */
		fprintf(stderr, "[ERROR] Failed to get elements from input string '%s'\n", input_string);
		exit(EXIT_FAILURE);
	}
	/* TODO acerion 2023.08.26: tests in
	   libcw_gen_tests_debug_pcm_file_timings.c is assigning durations to the
	   elements (with elements_set_ideal_durations()) at this stage. Maybe
	   this test could do this now too. */


	callback_data_t callback_data = { 0 };
	callback_data.string_elements = string_elements;
	callback_data.durations = &durations;
	cw_gen_register_value_tracking_callback_internal(gen, gen_callback_fn, &callback_data);


	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, input_string);
	cw_gen_wait_for_queue_level(gen, 0);


	cw_gen_stop(gen);
	cw_gen_delete(&gen);


	calculate_test_results(string_elements, test_data, &durations);
	evaluate_test_results(cte, test_data);

	cw_elements_delete(&string_elements);

	return 0;
}




/**
   Calculate current divergences (from current run of test) that will be
   compared with reference values
*/
static void calculate_test_results(const cw_elements_t * elements, test_data_t * test_data, const cw_gen_durations_t * durations)
{
	cw_element_stats_t stats_dot;
	cw_element_stats_t stats_dash;
	cw_element_stats_t stats_ims;
	cw_element_stats_t stats_ics;
	cw_element_stats_t stats_iws;
	cw_element_stats_init(&stats_dot);
	cw_element_stats_init(&stats_dash);
	cw_element_stats_init(&stats_ims);
	cw_element_stats_init(&stats_ics);
	cw_element_stats_init(&stats_iws);

	/* Skip first and last element. The way the test is structured may impact
	   correctness of values of these elements. TODO: make the elements
	   correct. */
	for (size_t i = 1; i < elements->curr_count - 1; i++) {
		const cw_element_t * element = &elements->array[i];
		switch (element->type) {
		case cw_element_type_dot:
			cw_element_stats_update(&stats_dot, element->duration);
			break;
		case cw_element_type_dash:
			cw_element_stats_update(&stats_dash, element->duration);
			break;
		case cw_element_type_ims:
			cw_element_stats_update(&stats_ims, element->duration);
			break;
		case cw_element_type_ics:
			cw_element_stats_update(&stats_ics, element->duration);
			break;
		case cw_element_type_iws:
			cw_element_stats_update(&stats_iws, element->duration);
			break;
		case cw_element_type_none: /* TODO: should we somehow log this? */
		default:
			break;
		}
	}

	cw_element_stats_calculate_divergences(&stats_dot, &test_data->current_div_dots, durations->dot_duration);
	cw_element_stats_calculate_divergences(&stats_dash, &test_data->current_div_dashes, durations->dash_duration);
	cw_element_stats_calculate_divergences(&stats_ims, &test_data->current_div_ims, durations->ims_duration);
	cw_element_stats_calculate_divergences(&stats_ics, &test_data->current_div_ics, durations->ics_duration);
	cw_element_stats_calculate_divergences(&stats_iws, &test_data->current_div_iws, durations->iws_duration);

	print_element_stats_and_divergences(&stats_dot, &test_data->current_div_dots, "dots", durations->dot_duration);
	print_element_stats_and_divergences(&stats_dash, &test_data->current_div_dashes, "dashes", durations->dash_duration);
	print_element_stats_and_divergences(&stats_ims, &test_data->current_div_ims, "ims", durations->ims_duration);
	print_element_stats_and_divergences(&stats_ics, &test_data->current_div_ics, "ics", durations->ics_duration);
	print_element_stats_and_divergences(&stats_iws, &test_data->current_div_iws, "iws", durations->iws_duration);
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





