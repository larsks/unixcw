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
   \file test_framework.c

   \brief Test framework for libcw test code
*/




#include "config.h"




#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef __FreeBSD__
#include <sys/sysinfo.h>
#endif

#if defined(HAVE_STRING_H)
#include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif




#include "libcw.h"
#include "libcw_debug.h"
#include "libcw_utils.h"
#include <cwutils/cw_cmdline.h>
#include <cwutils/sleep.h>
#include <cwutils/lib/random.h>

#include "test_framework.h"
#include <test_framework/basic_utils/param_ranger.h>
#include <test_framework/basic_utils/test_result.h>




/* Make pause between tests.

   Let the resources measurement tool go back to zero, so that
   e.g. high CPU usage in test N is visible only in that test, but not
   in test N+1 that will be executed right after test N.

   TODO acerion 2023.08.27: sometimes, to make the tests faster, the tester
   may request the pause to be zero. We can't make the pause value dependent
   on value of meas interval. The meas will either have to be interrupted, or
   the meas will have to provide wait() method to allow waiting for meas to
   complete its current cycle of meas.
*/
#define LIBCW_TEST_INTER_TEST_PAUSE_MSECS (2 * LIBCW_TEST_MEAS_CPU_MEAS_INTERVAL_MSECS)

#define MSG_BUF_SIZE 1024
#define VA_BUF_SIZE   128




static bool cw_test_expect_op_int(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_int_errors_only(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_int_sub(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, bool errors_only, const char * va_buf);

static bool cw_test_expect_op_float(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_float_errors_only(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_float_sub(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, bool errors_only, const char * va_buf);

static bool cw_test_expect_op_double(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_op_double_errors_only(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));

static bool cw_test_expect_strcasecmp(struct cw_test_executor_t * self, const char * expected_value, const char * received_value, const char * fmt, ...) __attribute__ ((format (printf, 4, 5)));

static bool cw_test_expect_between_int(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));
static bool cw_test_expect_between_int_errors_only(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...) __attribute__ ((format (printf, 5, 6)));

static bool cw_test_expect_null_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
static bool cw_test_expect_null_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

static bool cw_test_expect_valid_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));
static bool cw_test_expect_valid_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

static void cw_assert2(struct cw_test_executor_t * self, bool condition, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));


static void cw_test_print_test_header(cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void cw_test_print_test_footer(cw_test_executor_t * self, const char * test_name);
static void cw_test_append_status_string(cw_test_executor_t * self, char * msg_buf, int message_len, const char * status_string);

static int cw_test_process_args(cw_test_executor_t * self, int argc, char * const argv[]);
static int cw_test_get_loops_count(cw_test_executor_t * self);

static void cw_test_print_test_options(cw_test_executor_t * self);

static bool test_topic_is_in_cmdline_options(cw_test_executor_t * self, int libcw_test_topic);
static bool sound_system_is_in_cmdline_options(cw_test_executor_t * self, cw_sound_system sound_system);

static const char * cw_test_get_current_topic_label(cw_test_executor_t * self);
static const char * cw_test_get_current_sound_system_label(cw_test_executor_t * self);
static const char * cw_test_get_current_sound_device(cw_test_executor_t * self);

static void cw_test_set_current_topic_and_gen_config(cw_test_executor_t * self, int topic, int sound_system);

static void cw_test_print_test_stats(cw_test_executor_t * self);

static int cw_test_log_info(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void cw_test_log_info_cont(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void cw_test_flush_info(struct cw_test_executor_t * self);
static void cw_test_log_error(struct cw_test_executor_t * self, const char * fmt, ...) __attribute__ ((format (printf, 2, 3)));

static int print_active_sound_systems(cw_test_executor_t * self);
static int print_active_topics(cw_test_executor_t * self);

static bool cw_test_test_topic_is_member(cw_test_executor_t * cte, int topic, const int * topics, int max);
static bool cw_test_sound_system_is_member(cw_test_executor_t * cte, cw_sound_system sound_system, const cw_sound_system * sound_systems, int max);

static cwt_retv cw_test_main_test_loop(cw_test_executor_t * cte, cw_test_set_t * test_sets);
static unsigned int cw_test_get_total_errors_count(cw_test_executor_t * cte);




static cwt_retv iterate_over_topics(cw_test_executor_t * cte, cw_test_set_t * test_set);
static cwt_retv iterate_over_sound_systems(cw_test_executor_t * cte, cw_test_set_t * test_set, int topic);
static cwt_retv iterate_over_test_objects(cw_test_executor_t * cte, cw_test_object_t * test_objects, int topic, cw_sound_system sound_system);




static int msg_buff_prepare(struct cw_test_executor_t * executor, char * msg_buf, size_t msg_buf_size, const char * va_buff);



static int select_random_seed(cw_test_executor_t * cte, const cw_config_t * config);
static int select_sound_systems(cw_test_executor_t * cte);
static int select_topics(cw_test_executor_t * cte);




static int select_random_seed(cw_test_executor_t * cte, const cw_config_t * config)
{
	if (config->test_random_seed > 0) {
		cte->random_seed = cw_random_srand(config->test_random_seed);
		kite_log(cte, LOG_DEBUG, "Random seed is manually assigned value %u\n", cte->random_seed);
	} else {
		/* 0: allow internal algo to select good seed. */
		cte->random_seed = cw_random_srand(0);
		kite_log(cte, LOG_DEBUG, "Random seed is internally picked to be %u\n", cte->random_seed);
	}

	return 0;
}




static int select_sound_systems(cw_test_executor_t * cte)
{
	/* First decide which sound systems are requested according to command
	   line options. Set them as active, but beware: the "active" state will
	   be later verified in this function by additional conditions. */
	if (sound_system_is_in_cmdline_options(cte, CW_AUDIO_NULL)) {
		cte->configuration.sound_systems[CW_AUDIO_NULL].active = true;
	}
	if (sound_system_is_in_cmdline_options(cte, CW_AUDIO_CONSOLE)) {
		cte->configuration.sound_systems[CW_AUDIO_CONSOLE].active = true;
	}
	if (sound_system_is_in_cmdline_options(cte, CW_AUDIO_OSS)) {
		cte->configuration.sound_systems[CW_AUDIO_OSS].active = true;
	}
	if (sound_system_is_in_cmdline_options(cte, CW_AUDIO_ALSA)) {
		cte->configuration.sound_systems[CW_AUDIO_ALSA].active = true;

		/* Notice that for ALSA we use device that was potentially
		   configured through dedicated command line option:
		   "-X, --test-alsa-device=device". */
		snprintf(cte->configuration.sound_systems[CW_AUDIO_ALSA].sound_device,
		         sizeof (cte->configuration.sound_systems[CW_AUDIO_ALSA].sound_device),
		         "%s", cte->config->test_alsa_device_name);
	}
	if (sound_system_is_in_cmdline_options(cte, CW_AUDIO_PA)) {
		cte->configuration.sound_systems[CW_AUDIO_PA].active = true;
	}


	/* Now test the requested sound systems: see if they are possible on
	   current machine. If not, deactivate them. */
	for (cw_sound_system sound_system = CW_SOUND_SYSTEM_FIRST; sound_system <= CW_SOUND_SYSTEM_LAST; sound_system++) {
		sound_system_config_t * ssc = &(cte->configuration.sound_systems[sound_system]);
		if (!ssc->active) {
			/* Not requested in command line. */
			continue;
		}
		switch (sound_system) {
		case CW_AUDIO_NULL:
			if (!cw_is_null_possible(ssc->sound_device)) {
				cte->log_info(cte, "Null sound system is not available on this machine - will skip it\n");
				ssc->active = false;
			}
			break;
		case CW_AUDIO_CONSOLE:
			if (!cw_is_console_possible(ssc->sound_device)) {
				cte->log_info(cte, "Console sound system is not available on this machine - will skip it\n");
				ssc->active = false;
			}
			break;
		case CW_AUDIO_OSS:
			if (!cw_is_oss_possible(ssc->sound_device)) {
				cte->log_info(cte, "OSS sound system is not available on this machine - will skip it\n");
				ssc->active = false;
			}
			break;
		case CW_AUDIO_ALSA:
			if (!cw_is_alsa_possible(ssc->sound_device)) {
				cte->log_info(cte, "ALSA sound system is not available on this machine - will skip it\n");
				ssc->active = false;
			}
			break;
		case CW_AUDIO_PA:
			if (!cw_is_pa_possible(ssc->sound_device)) {
				cte->log_info(cte, "PulseAudio sound system is not available on this machine - will skip it\n");
				ssc->active = false;
			}
			break;
		case CW_AUDIO_NONE:
		case CW_AUDIO_SOUNDCARD:
		default:
			kite_log(cte, LOG_ERR, "%s:%d: unexpected sound system %d\n", __func__, __LINE__, sound_system);
			return -1;
		}
	}

	return 0;
}




static int select_topics(cw_test_executor_t * cte)
{
	int idx = 0;
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_TQ)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_TQ;
	}
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_GEN)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_GEN;
	}
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_KEY)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_KEY;
	}
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_REC)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_REC;
	}
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_DATA)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_DATA;
	}
	if (test_topic_is_in_cmdline_options(cte, LIBCW_TEST_TOPIC_OTHER)) {
		cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_OTHER;
	}
	cte->configuration.topics[idx++] = LIBCW_TEST_TOPIC_MAX; /* Guard element. */

	return 0;
}




int cw_test_process_args(cw_test_executor_t * cte, int argc, char * const argv[])
{
	if (CW_SUCCESS != cw_process_program_arguments(argc, argv, cte->config)) {
		kite_log(cte, LOG_ERR, "%s:%d: failed to process program arguments\n", __func__, __LINE__);
		return -1;
	}
	if (0 != select_random_seed(cte, cte->config)) {
		kite_log(cte, LOG_ERR, "%s:%d: failed to select random seed\n", __func__, __LINE__);
		return -1;
	}
	if (0 != select_sound_systems(cte)) {
		kite_log(cte, LOG_ERR, "%s:%d: failed to select sound systems to test\n", __func__, __LINE__);
		return -1;
	}
	if (0 != select_topics(cte)) {
		kite_log(cte, LOG_ERR, "%s:%d: failed to select topics to test\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}




static int cw_test_get_loops_count(cw_test_executor_t * self)
{
	return self->config->test_loops;
}




bool cw_test_expect_op_int(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	/* FIXME: this vsnprintf() introduces large delays when running tests under valgrind/callgrind. */
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_int_sub(self, expected_value, operator, received_value, false, va_buf);
}




bool cw_test_expect_op_int_errors_only(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	/* FIXME: this vsnprintf() introduces large delays when running tests under valgrind/callgrind. */
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_int_sub(self, expected_value, operator, received_value, true, va_buf);
}




static bool cw_test_expect_op_int_sub(struct cw_test_executor_t * self, int expected_value, const char * operator, int received_value, bool errors_only, const char * va_buf)
{
	bool as_expected = false;

	char msg_buf[MSG_BUF_SIZE] = { 0 };

	bool success = false;
	if (operator[0] == '=' && operator[1] == '=') {
		success = expected_value == received_value;

	} else if (operator[0] == '<' && operator[1] == '=') {
		success = expected_value <= received_value;

	} else if (operator[0] == '>' && operator[1] == '=') {
		success = expected_value >= received_value;

	} else if (operator[0] == '!' && operator[1] == '=') {
		success = expected_value != received_value;

	} else if (operator[0] == '<' && operator[1] == '\0') {
		success = expected_value < received_value;

	} else if (operator[0] == '>' && operator[1] == '\0') {
		success = expected_value > received_value;

	} else {
		self->log_error(self, "Unhandled operator '%s'\n", operator);
		assert(0);
	}


	if (success) {
		if (!errors_only) {
			self->stats->successes++;

			const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

			cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));

			self->log_info(self, "%s\n", msg_buf);
		}
		as_expected = true;
	} else {
		self->stats->failures++;

		const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected %d, got %d   ***\n", expected_value, received_value);

		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_op_float(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_float_sub(self, expected_value, operator, received_value, false, va_buf);
}




bool cw_test_expect_op_float_errors_only(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_float_sub(self, expected_value, operator, received_value, true, va_buf);
}




static bool cw_test_expect_op_float_sub(struct cw_test_executor_t * self, float expected_value, const char * operator, float received_value, bool errors_only, const char * va_buf)
{
	char msg_buf[MSG_BUF_SIZE] = { 0 };

	bool success = false;
	if (operator[0] == '<' && operator[1] == '\0') {
		success = expected_value < received_value;

	} else if (operator[0] == '>' && operator[1] == '\0') {
		success = expected_value > received_value;

	} else {
		self->log_error(self, "Unhandled operator '%s'\n", operator);
		assert(0);
	}


	bool as_expected = false;
	if (success) {
		if (!errors_only) {
			self->stats->successes++;

			const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);
			cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));
			self->log_info(self, "%s\n", msg_buf);
		}
		as_expected = true;
	} else {
		self->stats->failures++;

		const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);
		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected %f, got %f   ***\n", (double) expected_value, (double) received_value);

		as_expected = false;
	}
	return as_expected;
}




bool cw_test_expect_op_double(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_float_sub(self, (float) expected_value, operator, (float) received_value, false, va_buf);
}




bool cw_test_expect_op_double_errors_only(struct cw_test_executor_t * self, double expected_value, const char * operator, double received_value, const char * fmt, ...)
{
	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	return cw_test_expect_op_float_sub(self, (float) expected_value, operator, (float) received_value, true, va_buf);
}




bool cw_test_expect_strcasecmp(struct cw_test_executor_t * self, const char * expected_value, const char * received_value, const char * fmt, ...)
{
	bool as_expected = false;

	char va_buf[VA_BUF_SIZE] = { 0 };
	char msg_buf[MSG_BUF_SIZE] = { 0 };

	bool success = 0 == strcasecmp(expected_value, received_value);
	if (success) {
		const bool errors_only = false; /* In the future this may be function's argument (see other 'expect' functions). */
		if (!errors_only) {
			self->stats->successes++;

			va_list ap;
			va_start(ap, fmt);
			vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
			va_end(ap);

			const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

			/* FIXME: believe it or not, this line
			   introduces large delays when running tests
			   under valgrind/callgrind. */
			cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));

			self->log_info(self, "%s\n", msg_buf);
		}
		as_expected = true;
	} else {
		self->stats->failures++;

		va_list ap;
		va_start(ap, fmt);
		vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
		va_end(ap);

		const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);
		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected [%s], got [%s]   ***\n", expected_value, received_value);

		as_expected = false;
	}

	return as_expected;
}





/**
   @brief Append given status string at the end of buffer, but within cw_test::console_n_cols limit

   This is a private function so it is not put into cw_test_executor_t
   class.

   FIXME: believe it or not, this function introduces large delays when
   running tests under valgrind/callgrind.
*/
void cw_test_append_status_string(cw_test_executor_t * self, char * msg_buf, int message_len, const char * status_string)
{
	const char * separator = " "; /* Separator between test message and test status string, for better visibility of status string. */
	const int space_left = self->console_n_cols - message_len;

	const int separator_len = (int) strlen(separator);
	const int status_string_len = (int) strlen(status_string);

	if (space_left > separator_len + status_string_len) {
		sprintf(msg_buf + self->console_n_cols - separator_len - status_string_len, "%s%s", separator, status_string);
	} else {
		sprintf(msg_buf + self->console_n_cols - strlen("...") - separator_len - status_string_len, "...%s%s", separator, status_string);
	}
}




bool cw_test_expect_between_int(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...)
{
	bool as_expected = true;

	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[MSG_BUF_SIZE] = { 0 };
	const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

	if (expected_lower <= received_value && received_value <= expected_higher) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));
		self->log_info(self, "%s %d %d %d\n", msg_buf, expected_lower, received_value, expected_higher);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected within %d-%d, got %d   ***\n", expected_lower, expected_higher, received_value);

		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_between_int_errors_only(struct cw_test_executor_t * self, int expected_lower, int received_value, int expected_higher, const char * fmt, ...)
{
	bool as_expected = true;
	char buf[VA_BUF_SIZE] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof (buf), fmt, ap);
	va_end(ap);

	if (expected_lower <= received_value && received_value <= expected_higher) {
		as_expected = true;
	} else {
		const int n = fprintf(self->file_err, "%s%s", self->msg_prefix, buf);
		self->stats->failures++;
		self->log_error(self, "%*s", self->console_n_cols - n, "failure: ");
		self->log_error(self, "expected value within %d-%d, got %d\n", expected_lower, expected_higher, received_value);
		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_null_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;

	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[MSG_BUF_SIZE] = { 0 };
	const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

	if (NULL == pointer) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));
		self->log_info(self, "%s\n", msg_buf);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected NULL, got %p   ***\n", pointer);

		as_expected = false;
	}


	return as_expected;
}




bool cw_test_expect_null_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;

	if (NULL == pointer) {
		as_expected = true;
	} else {
		self->stats->failures++;

		char va_buf[VA_BUF_SIZE] = { 0 };
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
		va_end(ap);

		char msg_buf[MSG_BUF_SIZE] = { 0 };
		const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected NULL, got %p   ***\n", pointer);

		as_expected = false;
	}

	return as_expected;
}




bool cw_test_expect_valid_pointer(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;

	char va_buf[VA_BUF_SIZE] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	char msg_buf[MSG_BUF_SIZE] = { 0 };
	const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

	if (NULL != pointer) {
		self->stats->successes++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_pass));
		self->log_info(self, "%s\n", msg_buf);

		as_expected = true;
	} else {
		self->stats->failures++;

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected valid pointer, got NULL   ***\n");

		as_expected = false;
	}


	return as_expected;
}




bool cw_test_expect_valid_pointer_errors_only(struct cw_test_executor_t * self, const void * pointer, const char * fmt, ...)
{
	bool as_expected = false;

	if (NULL != pointer) {
		as_expected = true;
	} else {
		self->stats->failures++;

		char va_buf[VA_BUF_SIZE] = { 0 };
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
		va_end(ap);

		char msg_buf[MSG_BUF_SIZE] = { 0 };
		const int message_len = msg_buff_prepare(self, msg_buf, sizeof (msg_buf), va_buf);

		cw_test_append_status_string(self, msg_buf, message_len, get_test_result_string(test_result_fail));
		self->log_error(self, "%s\n", msg_buf);
		self->log_error(self, "   ***   expected valid pointer, got NULL   ***\n");

		as_expected = false;
	}

	return as_expected;
}




void cw_assert2(struct cw_test_executor_t * self, bool condition, const char * fmt, ...)
{
	if (!condition) {

		char va_buf[VA_BUF_SIZE] = { 0 };
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
		va_end(ap);

		self->log_error(self, "Assertion failed: %s\n", va_buf);

		exit(EXIT_FAILURE);
	}

	return;
}




/**
   See whether or not a given test topic was requested from command line. By
   default, if not specified in command line, all test topics are requested.
*/
static bool test_topic_is_in_cmdline_options(cw_test_executor_t * self, int libcw_test_topic)
{
	const int n = sizeof (self->config->tested_areas) / sizeof (self->config->tested_areas[0]);

	switch (libcw_test_topic) {
	case LIBCW_TEST_TOPIC_TQ:
	case LIBCW_TEST_TOPIC_GEN:
	case LIBCW_TEST_TOPIC_KEY:
	case LIBCW_TEST_TOPIC_REC:
	case LIBCW_TEST_TOPIC_DATA:
	case LIBCW_TEST_TOPIC_OTHER:
		for (int i = 0; i < n; i++) {
			if (LIBCW_TEST_TOPIC_MAX == self->config->tested_areas[i]) {
				/* Found guard element. */
				return false;
			}
			if (libcw_test_topic == self->config->tested_areas[i]) {
				return true;
			}
		}
		return false;

	case LIBCW_TEST_TOPIC_MAX:
	default:
		fprintf(stderr, "Unexpected test topic %d\n", libcw_test_topic);
		exit(EXIT_FAILURE);
	}
}




/**
   See whether or not a given sound system was requested from command line.
   By default, if not specified in command line, all sound systems are
   requested.

   However, if a host machine does not support some sound system (e.g.
   because a library is missing), such sound system is excluded from list of
   requested sound systems.
*/static bool sound_system_is_in_cmdline_options(cw_test_executor_t * self, cw_sound_system sound_system)
{
	const int n = sizeof (self->config->tested_sound_systems) / sizeof (self->config->tested_sound_systems[0]);

	switch (sound_system) {
	case CW_AUDIO_NULL:
	case CW_AUDIO_CONSOLE:
	case CW_AUDIO_OSS:
	case CW_AUDIO_ALSA:
	case CW_AUDIO_PA:
		for (int i = 0; i < n; i++) {
			if (CW_AUDIO_NONE == self->config->tested_sound_systems[i]) {
				/* Found guard element. */
				return false;
			}
			if (sound_system == self->config->tested_sound_systems[i]) {
				return true;
			}
		}
		return false;

	case CW_AUDIO_NONE:
	case CW_AUDIO_SOUNDCARD:
	default:
		kite_log(self, LOG_ERR, "%s:%d: Unexpected sound system %d\n", __func__, __LINE__, sound_system);
		exit(EXIT_FAILURE);
	}
}




/**
   The length of this string must be as long as maximal length of messages
   printed to console.
 */
static char g_dashes[MSG_BUF_SIZE] = { 0 };




/**
   @brief Write a text with dashes on left and right side

   @reviewedon 2023.11.04

   @param[in/out] kite Test executor
   @param[in] severity Severity of log to use when printing the separator
   @param[in] dashes Buffer with dashes
   @param[in] text Text to be displayed between dashes

   @return Count of printed characters
*/
static size_t write_message_in_dashes(cw_test_executor_t * kite, int severity, const char * text)
{
	if (NULL == kite->file_out) {
		return 0;
	}

	/* TODO acerion 2023.11.04: check if severity is sufficient to print to
	   output. */

	/* One-time init of global buffer. */
	if (g_dashes[0] == '\0') {
		memset(g_dashes, '-', sizeof (g_dashes) - 1);
		g_dashes[sizeof (g_dashes) - 1] = '\0';
	}

	const size_t severity_len = kite_log(kite, severity, "%s", ""); /* To have "INFO" prefix in the printed line. */
	const size_t text_len = strlen(" ") + strlen(text) + strlen(" "); /* "text" is surrounded by space on each side. */
	int n_dashes = 0;
	if ((size_t) kite->console_n_cols > severity_len + text_len) {
		n_dashes = kite->console_n_cols - severity_len - text_len;
	}
	const int n_dashes_left = n_dashes / 2; /* Count of dashes on left side of text. */
	const int n_dashes_right = n_dashes - n_dashes_left; /* Count of dashes on right side of text. */

	size_t written = 0;
	written += fwrite(g_dashes, sizeof (char), n_dashes_left, kite->file_out);
	written += fwrite(" ", sizeof (char), strlen(" "), kite->file_out);
	written += fwrite(text, sizeof (char), strlen(text), kite->file_out);
	written += fwrite(" ", sizeof (char), strlen(" "), kite->file_out);
	written += fwrite(g_dashes, sizeof (char), n_dashes_right, kite->file_out);

	fwrite("\n", sizeof (char), 1, kite->file_out);

	return written;
}




/**
   @brief Print separator line consisting of N dash characters

   @reviewedon 2023.11.04

   @param[in/out] kite Text executor
   @param[in] severity Severity of log to use when printing the separator
*/
static void write_dashes(cw_test_executor_t * kite, int severity)
{
	if (NULL == kite->file_out) {
		return;
	}

	/* TODO acerion 2023.11.04: check if severity is sufficient to print to
	   output. */

	/* One-time init of global buffer. */
	if (g_dashes[0] == '\0') {
		memset(g_dashes, '-', sizeof (g_dashes) - 1);
		g_dashes[sizeof (g_dashes) - 1] = '\0';
	}

	const int severity_len = kite_log(kite, severity, "%s", ""); /* To have "INFO" prefix in the printed line. */
	fwrite(g_dashes, sizeof (char), kite->console_n_cols - severity_len, kite->file_out);
	fwrite("\n", sizeof (char), 1, kite->file_out);
}




void cw_test_print_test_header(cw_test_executor_t * self, const char * fmt, ...)
{
	self->log_info_cont(self, "\n");

#if 0
	/* For tests of overflows in function. */
	const char * long_text = "abcdefghijklmnopqrstuvwyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwyz";
#endif
	write_message_in_dashes(self, LOG_INFO, "Info about this test");

	char va_buf[256] = { 0 };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	self->log_info(self, "Test name: %s\n", va_buf);
	self->log_info(self, "Current test topic: %s\n", self->get_current_topic_label(self));
	self->log_info(self, "Current sound system: %s\n", self->get_current_sound_system_label(self));
	self->log_info(self, "Current sound device: '%s'\n", self->get_current_sound_device(self));

	write_dashes(self, LOG_INFO);
}




void cw_test_print_test_footer(cw_test_executor_t * self, const char * test_name)
{
	self->log_info(self, "End of test: %s\n", test_name);
}




void kite_on_test_completion(cw_test_executor_t * kite, const char * test_name, test_result_t test_result)
{
	write_message_in_dashes(kite, LOG_INFO, "Overall result of this test");

	char msg_buf[MSG_BUF_SIZE] = { 0 };
	size_t n = msg_buff_prepare(kite, msg_buf, sizeof (msg_buf), test_name);

	cw_test_append_status_string(kite, msg_buf, n, get_test_result_string(test_result));
	kite->log_info(kite, "%s\n", msg_buf);

	write_dashes(kite, LOG_INFO);

	switch (test_result) {
	case test_result_pass:
		kite->stats->successes++;
		break;
	case test_result_fail:
		kite->stats->failures++;
		break;
	default:
		kite_log(kite, LOG_ERR, "Unexpected test result value %d\n", test_result);
		break;
	}
}




const char * cw_test_get_current_sound_system_label(cw_test_executor_t * self)
{
	return cw_get_audio_system_label(self->current_gen_conf.sound_system);
}




const char * cw_test_get_current_sound_device(cw_test_executor_t * self)
{
	return self->current_gen_conf.sound_device;
}




const char * cw_test_get_current_topic_label(cw_test_executor_t * self)
{
	switch (self->current_topic) {
	case LIBCW_TEST_TOPIC_TQ:
		return "tq";
	case LIBCW_TEST_TOPIC_GEN:
		return "gen";
	case LIBCW_TEST_TOPIC_KEY:
		return "key";
	case LIBCW_TEST_TOPIC_REC:
		return "rec";
	case LIBCW_TEST_TOPIC_DATA:
		return "data";
	case LIBCW_TEST_TOPIC_OTHER:
		return "other";
	default:
		return "*** unknown ***";
	}
}




/**
   @brief Set a test topic and sound system that is about to be tested

   This is a private function so it is not put into cw_test_executor_t
   class.

   Call this function before calling each test function. @p topic and @p
   sound_system values to be passed to this function should be taken from the
   same test set that the test function is taken.
*/
static void cw_test_set_current_topic_and_gen_config(cw_test_executor_t * self, int topic, int sound_system)
{
	self->current_topic = topic;
	self->current_gen_conf.sound_system = sound_system;

	/* TODO: we have to somehow organize copying of these values from
	   program config to test executor config. For now this is ad-hoc
	   solution. */
	self->current_gen_conf.alsa_period_size = self->config->gen_conf.alsa_period_size;

	self->current_gen_conf.sound_device[0] = '\0'; /* Clear value from previous run of test. */
	switch (self->current_gen_conf.sound_system) {
	case CW_AUDIO_ALSA:
		if ('\0' != self->config->test_alsa_device_name[0]) {
			snprintf(self->current_gen_conf.sound_device,
				 sizeof (self->current_gen_conf.sound_device),
				 "%s", self->config->test_alsa_device_name);
		}
		break;
	case CW_AUDIO_NULL:
	case CW_AUDIO_CONSOLE:
	case CW_AUDIO_OSS:
	case CW_AUDIO_PA:
		/* We don't have a buffer with device name for this sound system. */
		break;
	case CW_AUDIO_NONE:
	case CW_AUDIO_SOUNDCARD:
	default:
		/* Technically speaking this is an error, but we shouldn't
		   get here because test binary won't accept such sound
		   systems through command line. */
		break;
	}

	self->stats = &self->all_stats[sound_system][topic];
}




void cw_test_print_test_stats(cw_test_executor_t * self)
{
	const char sound_systems[] = " NCOAP";

	fprintf(self->file_err, "\n\nlibcw tests: Statistics of tests (failures/total)\n\n");

	//                           12345 12345678901 12345678901 12345678901 12345678901 12345678901 12345678901
	#define SEPARATOR_LINE      "   --+-----------+-----------+-----------+-----------+-----------+-----------+\n"
	#define FRONT_FORMAT        "%s %c |"
	#define BACK_FORMAT         "%s\n"
	#define CELL_FORMAT_D       "% 10d |"
	#define CELL_FORMAT_S       "%10s |"

	fprintf(self->file_err,     "     | tone queue| generator |    key    |  receiver |    data   |    other  |\n");
	fprintf(self->file_err,     "%s", SEPARATOR_LINE);

	for (int sound = CW_AUDIO_NULL; sound <= CW_AUDIO_PA; sound++) {

		/* If a row with error counter has non-zero values,
		   use arrows at the beginning and end of the row to
		   highlight/indicate row that has non-zero error
		   counters. We want the errors to be visible and
		   stand out. */
		char error_indicator_empty[3] = "  ";
		char error_indicator_front[3] = "  ";
		char error_indicator_back[3] = "  ";
		{
			bool has_errors = false;
			for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
				if (self->all_stats[sound][topic].failures) {
					has_errors = true;
					break;
				}
			}

			if (has_errors) {
				snprintf(error_indicator_front, sizeof (error_indicator_front), "%s", "->");
				snprintf(error_indicator_back, sizeof (error_indicator_back), "%s", "<-");
			}
		}



		/* Print line with errors. Print numeric values only
		   if some tests for given combination of sound
		   system/topic were performed. */
		fprintf(self->file_err, FRONT_FORMAT, error_indicator_front, sound_systems[sound]);
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			int total = self->all_stats[sound][topic].failures + self->all_stats[sound][topic].successes;
			int failures = self->all_stats[sound][topic].failures;

			if (0 == total && 0 == failures) {
				fprintf(self->file_err, CELL_FORMAT_S, " ");
			} else {
				fprintf(self->file_err, CELL_FORMAT_D, failures);
			}
		}
		fprintf(self->file_err, BACK_FORMAT, error_indicator_back);



		/* Print line with totals. Print numeric values only
		   if some tests for given combination of sound
		   system/topic were performed. */
		fprintf(self->file_err, FRONT_FORMAT, error_indicator_empty, sound_systems[sound]);
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			int total = self->all_stats[sound][topic].failures + self->all_stats[sound][topic].successes;
			int failures = self->all_stats[sound][topic].failures;

			if (0 == total && 0 == failures) {
				fprintf(self->file_err, CELL_FORMAT_S, " ");
			} else {
				fprintf(self->file_err, CELL_FORMAT_D, total);
			}
		}
		fprintf(self->file_err, BACK_FORMAT, error_indicator_empty);



		fprintf(self->file_err,       "%s", SEPARATOR_LINE);
	}

#ifndef __FreeBSD__
	struct sysinfo sys_info;
	sysinfo(&sys_info);
	self->uptime_end = sys_info.uptime;
	const long test_duration = self->uptime_end - self->uptime_begin;
	fprintf(self->file_err, "Duration of tests = %ld minutes, %ld seconds\n",
		test_duration / 60, test_duration % 60);
#endif

	return;
}




void cw_test_init(cw_test_executor_t * self, FILE * stdout, FILE * stderr, const char * msg_prefix)
{
	memset(self, 0, sizeof (cw_test_executor_t));

	self->config = cw_config_new("libcw tests");

	self->file_out = stdout;
	self->file_err = stderr;

	self->use_resource_meas = false;

	self->expect_op_int = cw_test_expect_op_int;
	self->expect_op_int_errors_only = cw_test_expect_op_int_errors_only;
	self->expect_op_float = cw_test_expect_op_float;
	self->expect_op_float_errors_only = cw_test_expect_op_float_errors_only;
	self->expect_op_double = cw_test_expect_op_double;
	self->expect_op_double_errors_only = cw_test_expect_op_double_errors_only;
	self->expect_strcasecmp = cw_test_expect_strcasecmp;

	self->expect_between_int = cw_test_expect_between_int;
	self->expect_between_int_errors_only = cw_test_expect_between_int_errors_only;

	self->expect_null_pointer = cw_test_expect_null_pointer;
	self->expect_null_pointer_errors_only = cw_test_expect_null_pointer_errors_only;

	self->expect_valid_pointer = cw_test_expect_valid_pointer;
	self->expect_valid_pointer_errors_only = cw_test_expect_valid_pointer_errors_only;

	self->assert2 = cw_assert2;

	self->print_test_header = cw_test_print_test_header;
	self->print_test_footer = cw_test_print_test_footer;

	self->process_args = cw_test_process_args;

	self->get_loops_count = cw_test_get_loops_count;

	self->print_test_options = cw_test_print_test_options;

	self->get_current_topic_label = cw_test_get_current_topic_label;
	self->get_current_sound_system_label = cw_test_get_current_sound_system_label;
	self->get_current_sound_device = cw_test_get_current_sound_device;

	self->print_test_stats = cw_test_print_test_stats;

	self->log_info = cw_test_log_info;
	self->log_info_cont = cw_test_log_info_cont;
	self->flush_info = cw_test_flush_info;
	self->log_error = cw_test_log_error;

	self->main_test_loop = cw_test_main_test_loop;
	self->get_total_errors_count = cw_test_get_total_errors_count;


	self->console_n_cols = default_cw_test_print_n_chars;

	snprintf(self->msg_prefix, sizeof (self->msg_prefix), "%s: ", msg_prefix);
}




void cw_test_deinit(cw_test_executor_t * self)
{
	cw_config_delete(&self->config);
}




size_t kite_log(struct cw_test_executor_t * executor, int severity, const char * fmt, ...)
{
	if (NULL == executor->file_out) {
		return 0;
	}

	const char * tag = "[??]";
	switch (severity) {
	case LOG_ERR:
		tag = "[EE]";
		break;
	case LOG_WARNING:
		tag = "[WW]";
		break;
	case LOG_NOTICE:
		tag = "[NN]";
		break;
	case LOG_INFO:
		tag = "[II]";
		break;
	case LOG_DEBUG:
		tag = "[DD]";
		break;
	default:
		break;
	}

	char va_buf[512] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	/* FIXME: this vsnprintf() introduces *some* delays when
	   running tests under valgrind/callgrind. Fixing this FIXME
	   will have very small impact, so try this as last. */
	size_t va_buf_len = (size_t) vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	if (va_buf_len >= sizeof (va_buf)) {
		/* vsnprintf() would want to write va_buf_len, but it wrote less. */
		va_buf_len = sizeof (va_buf) - 1;
	}
	va_end(ap);

#if 1
	/* Version (hopefully) optimized for speed. */
	size_t n = 0;
	n += fwrite(tag, 4, 1, executor->file_out);
	n += fwrite(" ", 1, 1, executor->file_out);
	n += fwrite(va_buf, va_buf_len, 1, executor->file_out);
#else
	/* Version not optimized for speed. */
	const int n = fprintf(executor->file_out, "%s %s", tag, va_buf);
#endif
	fflush(executor->file_out);

	return n;
}




int cw_test_log_info(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->file_out) {
		return 0;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	/* FIXME: this vsnprintf() introduces *some* delays when
	   running tests under valgrind/callgrind. Fixing this FIXME
	   will have very small impact, so try this as last. */
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	const int n = fprintf(self->file_out, "[II] %s", va_buf);
	fflush(self->file_out);

	return n;
}




void cw_test_log_info_cont(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->file_out) {
		return;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	fprintf(self->file_out, "%s", va_buf);
	fflush(self->file_out);

	return;
}




void cw_test_flush_info(struct cw_test_executor_t * self)
{
	if (NULL == self->file_out) {
		return;
	}
	fflush(self->file_out);
	return;
}




void cw_test_log_error(struct cw_test_executor_t * self, const char * fmt, ...)
{
	if (NULL == self->file_out) {
		return;
	}

	char va_buf[256] = { 0 };

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(va_buf, sizeof (va_buf), fmt, ap);
	va_end(ap);

	fprintf(self->file_out, "[EE] %s", va_buf);
	fflush(self->file_out);

	return;
}




/**
   @brief Print labels of sound systems that are active in given test executor

   Active sound systems will be used during current test execution.

   @reviewedon 2023.09.30

   @param cte[in] test executor containing configuration of sound systems

   @return 0 if no unexpected situation was found
   @return -1 otherwise
*/
static int print_active_sound_systems(cw_test_executor_t * cte)
{
	for (cw_sound_system_t s = CW_SOUND_SYSTEM_FIRST; s <= CW_SOUND_SYSTEM_LAST; s++) {
		if (!cte->configuration.sound_systems[s].active) {
			continue;
		}
		switch (s) {
		case CW_AUDIO_NULL:
			cte->log_info_cont(cte, "null ");
			break;
		case CW_AUDIO_CONSOLE:
			cte->log_info_cont(cte, "console ");
			break;
		case CW_AUDIO_OSS:
			cte->log_info_cont(cte, "OSS ");
			break;
		case CW_AUDIO_ALSA:
			cte->log_info_cont(cte, "ALSA ");
			break;
		case CW_AUDIO_PA:
			cte->log_info_cont(cte, "PulseAudio ");
			break;
		case CW_AUDIO_NONE:
		case CW_AUDIO_SOUNDCARD:
		default:
			kite_log(cte, LOG_ERR, "%s:%d: unexpected sound system %d\n", __func__, __LINE__, s);
			return -1;
		}
	}

	return 0;
}




/**
   @brief Print labels of test topics active in given test executor

   Active test topics will be used during current test execution.

   @reviewedon 2023.09.30

   @param cte[in] test executor containing configuration of test topics

   @return 0 if no unexpected situation was found
   @return -1 otherwise
*/
int print_active_topics(cw_test_executor_t * cte)
{
	const size_t count = sizeof (cte->configuration.topics) / sizeof (cte->configuration.topics[0]);
	for (size_t i = 0; i < count; i++) {
		if (LIBCW_TEST_TOPIC_MAX == cte->configuration.topics[i]) {
			/* Found guard element. */
			return 0;
		}

		switch (cte->configuration.topics[i]) {
		case LIBCW_TEST_TOPIC_TQ:
			cte->log_info_cont(cte, "tq ");
			break;
		case LIBCW_TEST_TOPIC_GEN:
			cte->log_info_cont(cte, "gen ");
			break;
		case LIBCW_TEST_TOPIC_KEY:
			cte->log_info_cont(cte, "key ");
			break;
		case LIBCW_TEST_TOPIC_REC:
			cte->log_info_cont(cte, "rec ");
			break;
		case LIBCW_TEST_TOPIC_DATA:
			cte->log_info_cont(cte, "data ");
			break;
		case LIBCW_TEST_TOPIC_OTHER:
			cte->log_info_cont(cte, "other ");
			break;
		default:
			kite_log(cte, LOG_ERR, "%s:%d: unexpected test topic at position %zu: %d\n",
			         __func__, __LINE__, i, cte->configuration.topics[i]);
			return -1;
		}
	}
	cte->log_info_cont(cte, "\n");

	return 0;
}




void cw_test_print_test_options(cw_test_executor_t * self)
{
	self->log_info(self, "Sound systems that will be tested: ");
	if (0 != print_active_sound_systems(self)) {
		exit(EXIT_FAILURE);
	}
	self->log_info_cont(self, "\n");

	self->log_info(self, "Areas that will be tested: ");
	if (0 != print_active_topics(self)) {
		exit(EXIT_FAILURE);
	}
	self->log_info_cont(self, "\n");

	self->log_info(self, "Random seed = %u / 0x%08x\n", self->random_seed, self->random_seed);

	if (strlen(self->config->test_function_name)) {
		self->log_info(self, "Single function to be tested: '%s'\n", self->config->test_function_name);
	}

	fflush(self->file_out);
}




/**
   @brief See if given @param topic is a member of given list of test topics @param topics

   The size of @param topics is specified by @param max.
*/
bool cw_test_test_topic_is_member(__attribute__((unused)) cw_test_executor_t * cte, int topic, const int * topics, int max)
{
	for (int i = 0; i < max; i++) {
		if (LIBCW_TEST_TOPIC_MAX == topics[i]) {
			/* Found guard element. */
			return false;
		}
		if (topic == topics[i]) {
			return true;
		}
	}
	return false;
}




/**
   @brief See if given @param sound_system is a member of given list of test topics @param sound_system

   The size of @param sound_system is specified by @param max.
*/
bool cw_test_sound_system_is_member(__attribute__((unused)) cw_test_executor_t * cte, cw_sound_system sound_system, const cw_sound_system * sound_systems, int max)
{
	for (int i = 0; i < max; i++) {
		if (CW_AUDIO_NONE == sound_systems[i]) {
			/* Found guard element. */
			return false;
		}
		if (sound_system == sound_systems[i]) {
			return true;
		}
	}
	return false;
}




cwt_retv cw_test_main_test_loop(cw_test_executor_t * cte, cw_test_set_t * test_sets)
{
#ifndef __FreeBSD__
	struct sysinfo sys_info;
	sysinfo(&sys_info);
	cte->uptime_begin = sys_info.uptime;
#endif
	int set = 0;
	while (LIBCW_TEST_SET_VALID == test_sets[set].set_valid) {
		cw_test_set_t * test_set = &test_sets[set];
		if (cwt_retv_ok != iterate_over_topics(cte, test_set)) {
			cte->log_error(cte, "Test framework failed for set %d\n", set);
			return cwt_retv_err;
		}
		set++;
	}

	return cwt_retv_ok;
}




static cwt_retv iterate_over_topics(cw_test_executor_t * cte, cw_test_set_t * test_set)
{
	for (int i = 0; cte->configuration.topics[i] != LIBCW_TEST_TOPIC_MAX; i++) {
		int topic = cte->configuration.topics[i];

		const int topics_max = sizeof (test_set->tested_areas) / sizeof (test_set->tested_areas[0]);
		if (!cw_test_test_topic_is_member(cte, topic, test_set->tested_areas, topics_max)) {
			continue;
		}

		if (cwt_retv_ok != iterate_over_sound_systems(cte, test_set, topic)) {
			cte->log_error(cte, "Test framework failed for topic %d\n", topic);
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




static cwt_retv iterate_over_sound_systems(cw_test_executor_t * cte, cw_test_set_t * test_set, int topic)
{
	for (cw_sound_system sound_system = CW_SOUND_SYSTEM_FIRST; sound_system <= CW_SOUND_SYSTEM_LAST; sound_system++) {
		if (!cte->configuration.sound_systems[sound_system].active) {
			continue;
		}
		const int systems_max = sizeof (test_set->tested_sound_systems) / sizeof (test_set->tested_sound_systems[0]);
		if (!cw_test_sound_system_is_member(cte, sound_system, test_set->tested_sound_systems, systems_max)) {
			continue;
		}

		if (cwt_retv_ok != iterate_over_test_objects(cte, test_set->test_objects, topic, sound_system)) {
			cte->log_error(cte, "Test framework failed for topic %d, sound system %d\n", topic, sound_system);
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




static cwt_retv iterate_over_test_objects(cw_test_executor_t * cte, cw_test_object_t * test_objects, int topic, cw_sound_system sound_system)
{
	for (cw_test_object_t * test_obj = test_objects; NULL != test_obj->test_function; test_obj++) {
		bool execute = true;
		if (0 != strlen(cte->config->test_function_name)) {
			if (0 != strcmp(cte->config->test_function_name, test_obj->name)) {
				execute = false;
			}
		}
		if (cte->config->test_quick_only && !test_obj->is_quick) {
			continue;
		}

		if (!execute) {
			continue;
		}

		if (cte->use_resource_meas) {
			/* Starting measurement right before it has
			   something to measure.

			   Starting measurements resets old results from
			   previous measurement. This is significant when we
			   want to reset 'max resources' value - we want to
			   measure the 'max resources' value only per test
			   object, not per whole test set. */
			if (0 != resource_meas_start(&cte->resource_meas, LIBCW_TEST_MEAS_CPU_MEAS_INTERVAL_MSECS)) {
				kite_log(cte, LOG_ERR, "Failed to start resource meas when starting tests of '%s'\n", cte->config->test_function_name);
				return cwt_retv_err;
			}
		}

		cw_test_set_current_topic_and_gen_config(cte, topic, sound_system);
		//fprintf(stderr, "+++ %s +++\n", test_obj->name);
		const cwt_retv retv = test_obj->test_function(cte);


		if (cte->use_resource_meas) {
			cw_millisleep_internal(LIBCW_TEST_INTER_TEST_PAUSE_MSECS);
			/*
			  First stop the test, then display CPU usage summary.

			  Otherwise it may happen that the summary will say that max
			  CPU usage during test was zero, but then the meas object
			  will take the last measurement, detect high CPU usage, and will
			  display the high CPU usage information *after* the summary.
			*/
			resource_meas_stop(&cte->resource_meas);

			const float current_cpu_usage = resource_meas_get_current_cpu_usage(&cte->resource_meas);
			const float max_cpu_usage = resource_meas_get_maximal_cpu_usage(&cte->resource_meas);
			cte->log_info(cte, "CPU usage: last = "MEAS_CPU_FMT", max = "MEAS_CPU_FMT"\n",
			              (double) current_cpu_usage, (double) max_cpu_usage);
			if (max_cpu_usage > LIBCW_TEST_MEAS_CPU_OK_THRESHOLD_PERCENT) {
				cte->stats->failures++;
				cte->log_error(cte, "Registered high CPU usage "MEAS_CPU_FMT" during execution of '%s'\n",
				               (double) max_cpu_usage, test_obj->name);
			}
		}

		if (cwt_retv_ok != retv) {
			return cwt_retv_err;
		}
	}

	return cwt_retv_ok;
}




unsigned int cw_test_get_total_errors_count(cw_test_executor_t * cte)
{
	unsigned int result = 0;
	for (cw_sound_system sound_system = CW_AUDIO_NULL; sound_system <= CW_AUDIO_PA; sound_system++) {
		for (int topic = 0; topic < LIBCW_TEST_TOPIC_MAX; topic++) {
			result += cte->all_stats[sound_system][topic].failures;
		}
	}
	return result;
}




static int msg_buff_prepare(struct cw_test_executor_t * executor, char * msg_buf, size_t msg_buf_size, const char * va_buff)
{
	/* FIXME: these snprintf() call introduce large delays when running tests
	   under valgrind/callgrind. */
	int n = snprintf(msg_buf, msg_buf_size, "%s", executor->msg_prefix);
	const int message_len = n + snprintf(msg_buf + n, msg_buf_size - n, "%s", va_buff);
	snprintf(msg_buf + n, msg_buf_size - n, "%-*s", (executor->console_n_cols - n), va_buff);

	return message_len;
}


