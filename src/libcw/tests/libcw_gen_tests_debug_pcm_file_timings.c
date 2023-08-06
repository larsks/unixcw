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



#include "cwutils/lib/elements.h"
#include "cwutils/lib/elements_detect.h"
#include "cwutils/lib/element_stats.h"
#include "cwutils/lib/misc.h"
#include "cwutils/lib/wav.h"


#include "libcw_data.h"
#include "libcw_gen_tests_debug_pcm_file_timings.h"
#include "libcw_gen.h"
#include "libcw_utils.h"




/*
  This test is verifying if marks and spaces in wav (sound) file produced by
  generator have proper lengths.

  The sound file is produced by libcw if libcw is compiled in development
  mode ("./configure --enable-dev"). In such case the generator copies all
  sound samples to dedicated file in /tmp.

  This test is checking if durations of the marks and spaces are as expected.
*/




/*
  This parameter is selected experimentally.

  Ideally the count of elements should be calculated from input string, but
  I didn't want to write a function for it. Instead I chose to select a value
  that is "big enough". If I'm wrong, there are checks in elements code that
  detect attempt to overflow array of elements.
*/
#define ELEMENTS_COUNT_MAX 128




typedef struct test_data_t {
	/* Sound system for which we have reference data, and with which the
	   current run of a test should be done. */
	enum cw_audio_systems sound_system;

	/* Speed (WPM) for which we have reference data, and at which current
	   run of a test should be done (otherwise we will be comparing
	   results of tests made in different conditions). */
	int speed;
} test_data_t;




static cwt_retv test_cw_gen_debug_pcm_file_timings_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, const char * input_string);
static int get_elements_from_wav_file(const char * path, cw_elements_t * elements);
static void elements_set_ideal_durations(cw_elements_t * elements, cw_gen_durations_t * durations);
static void print_test_results(FILE * file, cw_elements_t * string_elements, cw_elements_t * wav_elements);
static void evaluate_test_results(cw_test_executor_t * cte, cw_elements_t * string_elements, cw_elements_t * wav_elements);




static test_data_t g_test_data[] = {
	{ .sound_system = CW_AUDIO_NULL,    .speed =  4 },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 12 },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 24 },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 36 },
	{ .sound_system = CW_AUDIO_NULL,    .speed = 60 },

	{ .sound_system = CW_AUDIO_CONSOLE, .speed =  4 },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 12 },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 24 },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 36 },
	{ .sound_system = CW_AUDIO_CONSOLE, .speed = 60 },

	{ .sound_system = CW_AUDIO_OSS,     .speed =  4 },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 12 },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 24 },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 36 },
	{ .sound_system = CW_AUDIO_OSS,     .speed = 60 },

	{ .sound_system = CW_AUDIO_ALSA,    .speed =  4 },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 12 },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 24 },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 36 },
	{ .sound_system = CW_AUDIO_ALSA,    .speed = 60 },
#if 0
	{ .sound_system = CW_AUDIO_PA,      .speed =  4 },
	{ .sound_system = CW_AUDIO_PA,      .speed = 12 },
#endif
	{ .sound_system = CW_AUDIO_PA,      .speed = 24 },
	{ .sound_system = CW_AUDIO_PA,      .speed = 36 },
	{ .sound_system = CW_AUDIO_PA,      .speed = 60 },
};




/* Top-level test function. */
cwt_retv test_cw_gen_debug_pcm_file_timings(cw_test_executor_t * cte)
{
	cte->print_test_header(cte, "%s", __func__);

	/*
	  Test string that will be played by test. Length of string may have
	  impact on required value of ELEMENTS_COUNT_MAX.

	  Make sure to include some inter-word-spaces in the text.
	*/
	const char * const input_string = " The fox over the lazy dog";
	//const char * const input_string = " abc ";


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
		if (cwt_retv_ok != test_cw_gen_debug_pcm_file_timings_sub(cte, test_data, cte->current_gen_conf.sound_device, input_string)) {
			retv = cwt_retv_err;
			break;
		}
	}

	cte->print_test_footer(cte, __func__);

	return retv;
}




void elements_set_ideal_durations(cw_elements_t * elements, cw_gen_durations_t * durations)
{
	for (size_t i = 0; i < elements->curr_count; i++) {
		elements->array[i].duration = ideal_duration_of_element(elements->array[i].type, durations);
	}
}




static cwt_retv test_cw_gen_debug_pcm_file_timings_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device, const char * input_string)
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
	cw_durations_print(stderr, &durations);
	fprintf(stderr, "[INFO ] speed               = %d WPM\n", test_data->speed);


	cw_elements_t * string_elements = cw_elements_new(ELEMENTS_COUNT_MAX);
	if (NULL == string_elements) {
		/* This is treated as developer's error, therefore we exit. Developer
		   should ensure sufficient count of elements for given string. */
		fprintf(stderr, "[ERROR] Failed to allocate string elements for string '%s'\n", input_string);
		exit(EXIT_FAILURE);
	}
	if (0 != cw_elements_from_string(input_string, string_elements)) {
		/* This is treated as developer's error, therefore we exit. Developer
		   should ensure that cw_elements_from_string() work correctly for valid
		   input strings. */
		fprintf(stderr, "[ERROR] Failed to get elements from input string '%s'\n", input_string);
		cw_elements_delete(&string_elements);
		exit(EXIT_FAILURE);
	}
	/* Set how long each element should be (ideally, in ideal generation
	   conditions). Expected durations of elements depend on generator's wpm,
	   so we can set them in string elements only here, after a generator has
	   been created and configured. */
	elements_set_ideal_durations(string_elements, &durations);


	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, input_string);
	cw_gen_wait_for_queue_level(gen, 0);
	cw_gen_stop(gen);

	char pcm_file_path[256] = { 0 };
	char wav_file_path[256 + 4] = { 0 };
	snprintf(pcm_file_path, sizeof (pcm_file_path), "%s", gen->dev_raw_sink_path);
	snprintf(wav_file_path, sizeof (wav_file_path), "%s.wav", gen->dev_raw_sink_path);
	char cmd[1024] = { 0 };
	snprintf(cmd, sizeof (cmd), "sox -e signed-integer -b 16 -c 1 -r %u %s %s",
	         gen->sample_rate,
	         pcm_file_path, wav_file_path);
	if (0 != system(cmd)) {
		fprintf(stderr, "[ERROR] Running command '%s' failed\n", cmd);
		return cwt_retv_err;
	}
	cw_gen_delete(&gen);

	cw_elements_t * wav_elements = cw_elements_new(ELEMENTS_COUNT_MAX);
	const int wav_elements_count = get_elements_from_wav_file(wav_file_path, wav_elements);
	if (-1 == wav_elements_count) {
		cw_elements_delete(&wav_elements);
		exit(EXIT_FAILURE);
	}

	print_test_results(stderr, string_elements, wav_elements);
	evaluate_test_results(cte, string_elements, wav_elements);

	cw_elements_delete(&string_elements);
	cw_elements_delete(&wav_elements);
	return cwt_retv_ok;
}




/**
   @return 0 on success
   @return -1 on failure
*/
static int get_elements_from_wav_file(const char * path, cw_elements_t * elements)
{
	int input_fd = open(path, O_RDONLY);
	if (-1 == input_fd) {
		fprintf(stderr, "[ERROR] Can't open input file '%s'\n", path);
		return -1;
	}

	wav_header_t header = { 0 };
	read_wav_header(input_fd, &header);

	const float sample_spacing = (1000.0F * 1000.0F) / header.sample_rate; // [us]
	fprintf(stderr, "[INFO ] Sample rate    = %d Hz\n", header.sample_rate);
	fprintf(stderr, "[INFO ] Sample spacing = %.4f us\n", (double) sample_spacing);

	const int retval = cw_elements_detect_from_wav(input_fd, elements, sample_spacing);
	close(input_fd);

	return retval;
}




static void print_test_results(FILE * file, cw_elements_t * string_elements, cw_elements_t * wav_elements)
{
	const size_t count = string_elements->curr_count;
	fprintf(file, "[DEBUG]  Num. | str state  type     duration | wav state     duration |\n");
	fprintf(file, "[DEBUG] ------+------------------------------+------------------------|\n");
	for (size_t i = 0; i < count; i++) {
		fprintf(file, "[DEBUG] %5zd | %5s     '%c' %12.2fus | %5s   %12.2fus |\n",
		        i,
		        string_elements->array[i].state == cw_state_mark ? "mark" : "space",
		        cw_element_type_get_representation(string_elements->array[i].type),
		        string_elements->array[i].duration,
		        wav_elements->array[i].state == cw_state_mark ? "mark" : "space",
		        wav_elements->array[i].duration);
	}

	if (0) {
		fprintf(stderr, "\n[DEBUG] wav elements:\n");
		cw_elements_print_to_file(stderr, wav_elements);

		fprintf(stderr, "\n[DEBUG] string elements:\n");
		cw_elements_print_to_file(stderr, string_elements);
	}
}




/**
  @p string_elements are treated as reference values against which the @p
  wav_elements will be compared.
*/
static void evaluate_test_results(cw_test_executor_t * cte, cw_elements_t * string_elements, cw_elements_t * wav_elements)
{
	if (!cte->expect_op_int(cte, string_elements->curr_count, "==", wav_elements->curr_count, "The same count of elements")) {
		/* If count of characters doesn't match then there is no point in
		   checking other properties of element sets. Therefore return. */
		return;
	}

	/* cw_element_t::type is set only in string_elements, so we won't be
	   checking for mismatch of types. */
	int states_mismatch = 0;
	int durations_mismatch = 0;

	/*
	  FIXME (acerion) 2023.08.06. For last element (which is an
	  ics) the diff will be few percents. Check why, fix the root
	  cause, and then remove "-1" in the condition of the loop.
	*/
	for (size_t i = 0; i < string_elements->curr_count - 1; i++) {
		cw_element_t * string_element = &string_elements->array[i];
		cw_element_t * wav_element = &wav_elements->array[i];

		/* dot/dash states should be set correctly in both element sets. */
		if (string_element->state != wav_element->state) {
			states_mismatch++;
			continue;
		}

		const double threshold = 1.0; /* 1% of difference is still allowed. */
		const double duration_expected = string_element->duration;
		const double duration_actual = wav_element->duration;
		const double diff_percent = 100.0 * (duration_actual - duration_expected) / duration_expected;
		fprintf(stderr, "[DEBUG] string = %12.2f, wav = %12.2f, diff = %7.3f%%, threshold = %.1f%%\n",
		        string_element->duration, wav_element->duration, diff_percent, threshold);
		if (fabs(diff_percent) > threshold) {
			durations_mismatch++;
		}
	}

	cte->expect_op_int(cte, 0, "==", states_mismatch, "All states are the same in input and output elements");
	cte->expect_op_int(cte, 0, "==", durations_mismatch, "All durations are the same in input and output elements");
}

