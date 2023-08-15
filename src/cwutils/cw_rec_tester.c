#include <ctype.h>
#include <stdio.h>
#include <string.h>




#include <libcw_gen.h>
#include <libcw_utils.h>




#include "cw_rec_tester.h"




/**
   \file cw_rec_tester.c

   Tester of easy legacy receiver.

   Generate some Morse code by a helper generator (using known input text),
   forward the keying events to receiver, and see what the receiver received
   and if the received text matches known input text).
*/
/* TODO: since this file is for legacy api receiver, rename the file to
   something better (e.g. cw_easy_legacy_receiver_tester.c). */




static bool cw_rec_tester_input_and_received_match(cw_rec_tester_t * tester);
static void cw_rec_tester_normalize_input_and_received(cw_rec_tester_t * tester);

static void test_callback_func(void * arg, int morse_state);
static void low_tone_queue_callback(void * arg);

static void * cw_rec_tester_play_string(void * arg_tester);

static void cw_rec_tester_init_text_buffers(cw_rec_tester_t * tester, bool make_short);

static void cw_rec_tester_display_differences(const cw_rec_tester_t * tester);




void cw_rec_tester_init(cw_rec_tester_t * tester)
{
	memset(tester, 0, sizeof (cw_rec_tester_t));

	/* Configure test parameters. */
	tester->characters_to_enqueue = 5;
}




int cw_rec_tester_evaluate_receive_correctness(cw_rec_tester_t * tester)
{
	/* Use multiple newlines to clearly present sent and received
	   string. It will be easier to do visual comparison of the
	   two strings if they are presented that way. */

	fprintf(stderr, "[II] Sent:     \n\n'%s'\n\n", tester->input_string);
	fprintf(stderr, "[II] Received: \n\n'%s'\n\n", tester->received_string);

	cw_rec_tester_normalize_input_and_received(tester);

	fprintf(stderr, "[II] Sent (normalized):     \n\n'%s'\n\n", tester->input_string);
	fprintf(stderr, "[II] Received (normalized): \n\n'%s'\n\n", tester->received_string);

	tester->acceptable_error_rate_percent = 1.0F;
	tester->acceptable_last_mismatch_index = 10;
	const bool match = cw_rec_tester_input_and_received_match(tester);

	cw_rec_tester_display_differences(tester);
	if (match) {
		fprintf(stderr, "[II] Test result: %s\n", get_test_result_string(test_result_pass));
		return 0;
	} else {
		fprintf(stderr, "[EE] Test result: %s\n", get_test_result_string(test_result_fail));
		fprintf(stderr, "\n");
		fprintf(stderr, "[EE] '%s' != '%s'\n", tester->input_string, tester->received_string);
		fprintf(stderr, "\n");
		return -1;
	}
}




/**
   @brief Initialize main text buffers of tester

   The output buffer is just cleared (that's where the received text will go).

   The input buffer is filled with a text that will be played by helper generator and then received by tested receiver.

   @reviewedon 2023.08.15

   @param[in/out] tester Tester in which to initialize the text buffers
   @param[in] make_short Whether the text put into input buffer should be short (for quick tests) or long
*/
static void cw_rec_tester_init_text_buffers(cw_rec_tester_t * tester, bool make_short)
{
	memset(tester->input_string, 0, sizeof (tester->input_string));
	tester->input_string_i = 0;

	memset(tester->received_string, 0, sizeof (tester->received_string));
	tester->received_string_i = 0;

	/* TODO (acerion) 2023.08.15: generate the text randomly, with given length. */
	/* TODO (acerion) 2023.08.15: replace 'make_short' argument with expected length of generated input text. */

	if (make_short) {
		/* Short text for occasions where I need a quick test. */
#define BASIC_SET_SHORT "one two three four paris"
		const char input[REC_TEST_BUFFER_SIZE] = BASIC_SET_SHORT;
		snprintf(tester->input_string, sizeof (tester->input_string), "%s", input);
	} else {
		/* Long text for longer tests. */
#define BASIC_SET_LONG \
	"the quick brown fox jumps over the lazy dog. 01234567890 paris paris paris "    \
	"abcdefghijklmnopqrstuvwxyz0123456789\"'$()+,-./:;=?_@<>!&^~ paris paris paris " \
	"one two three four five six seven eight nine ten eleven paris paris paris "
		const char input[REC_TEST_BUFFER_SIZE] = BASIC_SET_LONG BASIC_SET_LONG;
		snprintf(tester->input_string, sizeof (tester->input_string), "%s", input);
	}
}




/**
   @brief Make detailed comparison of input and received strings

   The function does more than just simple strcmp(). We accept that
   for different reasons the receiver doesn't work 100% correctly, and
   we allow some differences between input and received strings. The
   function uses some criteria (error rate and position of last
   mismatch) to check how similar the two strings are.

   Start comparing from the end.  At the beginning the receiver may
   not be tuned into incoming data, so at the beginning the errors are
   very probable, but after that there should be no errors.

   Comparing from the end makes sure that after first 5-10 characters
   the receiver performs 100% correctly.

   Also because of possible receive errors, the input string and
   received string may have different lengths. If we started comparing
   from the beginning, all received characters may be recognized as
   non-matching.

   @return true if input and received string are similar enough
   @return false otherwise
*/
static bool cw_rec_tester_input_and_received_match(cw_rec_tester_t * tester)
{
	const size_t input_len = strlen(tester->input_string);
	const size_t received_len = strlen(tester->received_string);

	/* Find shorter string's length. */
	const size_t len = input_len <= received_len ? input_len : received_len;

	size_t mismatch_count = 0;
	/* Index of last mismatched character. "Last" when looking
	   from the beginning of the string. */
	size_t last_mismatch_index = (size_t) -1;

	for (size_t i = 0; i < len; i++) {
		const size_t input_index = input_len - 1 - i;
		const size_t received_index = received_len - 1 - i;

		if (tester->input_string[input_index] != tester->received_string[received_index]) {
#if 0
			fprintf(stderr, "[WW] mismatch of '%c' vs '%c'\n",
				tester->input_string[input_index],
				tester->received_string[received_index]);
#endif
			mismatch_count++;

			if ((size_t) -1 == last_mismatch_index) {
				last_mismatch_index = input_index;
			}
		}
	}

#define PERC_FMT "%.3f%%"
	if (0 != mismatch_count) {
		const float error_rate_percent = 100.0F * mismatch_count / len;
		if (error_rate_percent > tester->acceptable_error_rate_percent) {
			/* High error rate is never acceptable. */
			fprintf(stderr, "[EE] Input len %zd, mismatch cnt %zd, err rate "PERC_FMT" (too high, thresh "PERC_FMT")\n",
				len, mismatch_count,
				(double) error_rate_percent,
				(double) tester->acceptable_error_rate_percent);
			return false;
		} else {
			fprintf(stderr, "[NN] Input len %zd, mismatch cnt %zd, err rate "PERC_FMT" (acceptable, thresh "PERC_FMT")\n",
				len, mismatch_count,
				(double) error_rate_percent,
				(double) tester->acceptable_error_rate_percent);
		}
	} else {
		fprintf(stderr, "[II] Input len %zd, mismatch cnt 0\n",
			len);
	}
#undef PERC_FMT

	if (((size_t) -1 != last_mismatch_index)) {
		if (last_mismatch_index > tester->acceptable_last_mismatch_index) {
			/* Errors are acceptable only at the beginning, where
			   receiver didn't tune yet into stream of incoming
			   data. */
			fprintf(stderr, "[EE] Input len %zd, last mismatch idx %zd (too far from beginning, thresh %zd)\n",
				len, last_mismatch_index, tester->acceptable_last_mismatch_index);
			return false;
		} else {
			fprintf(stderr, "[NN] Input len %zd, last mismatch idx %zd (acceptable, thresh %zd)\n",
				len, last_mismatch_index, tester->acceptable_last_mismatch_index);
		}
	} else {
		fprintf(stderr, "[II] Input len %zd, last mismatch idx none\n",
			len);
	}


	return true;
}




static void string_trim_end(char * string)
{
	if (NULL == string) {
		return;
	}

	const size_t len = strlen(string);
	if (0 == len) {
		return;
	}

	size_t i = len - 1;
	while (string[i] == ' ') {
		string[i] = '\0';
		i--;
	}
}




static void string_tolower(char * string)
{
	if (NULL == string) {
		return;
	}

	const size_t len = strlen(string);
	if (0 == len) {
		return;
	}

	for (size_t i = 0; i < len; i++) {
		string[i] = tolower((int) string[i]);
	}
}




/**
   @brief Remove all non-consequential differences in input and received string

   Remove ending space characters make strings lower case case.
*/
static void cw_rec_tester_normalize_input_and_received(cw_rec_tester_t * tester)
{
	/* Normalize input string. */
	string_trim_end(tester->input_string);

	/* Normalize received string. */
	string_trim_end(tester->received_string);
	string_tolower(tester->received_string);
}




/**
   @brief Display differences between input and received string

   Start displaying differences from the end of string.  Few
   differences at the beginning may be inevitable because the receiver
   didn't tune yet into incoming data. But if there are some
   differences at the end of the string, then those are much more
   interesting and should be displayed first.

   Also because of possible receive errors, the input string and
   received string may have different lengths. If we started comparing
   from the beginning, all received characters may be recognized as
   non-matching.
*/
static void cw_rec_tester_display_differences(const cw_rec_tester_t * tester)
{
	if (0 == strcmp(tester->input_string, tester->received_string)) {
		/* No differences to display. */
		return;
	}

	/* If there are 1000 differences, there is no reason to
	   display them all. */
	const size_t diffs_to_report_max = 10;
	size_t diffs_reported = 0;
	fprintf(stderr,
		"[II] Displaying at most last %zd different characters\n",
		diffs_to_report_max);

	const size_t input_len = strlen(tester->input_string);
	const size_t received_len = strlen(tester->received_string);
	/* Find shorter string's length. */
	const size_t len = input_len <= received_len ? input_len : received_len;

	for (size_t i = 0; i < len; i++) {
		const size_t input_index = input_len - 1 - i;
		const size_t received_index = received_len - 1 - i;

		if (tester->input_string[input_index] != tester->received_string[received_index]) {
			fprintf(stderr, "[WW] char input[%6zd] = %4d/0x%02x/'%c' vs. received[%6zd] = %4d/0x%02x/'%c'\n",

				input_index,
				(int) tester->input_string[input_index],
				(unsigned int) tester->input_string[input_index],
				tester->input_string[input_index],

				received_index,
				(int) tester->received_string[received_index],
				(unsigned int) tester->received_string[received_index],
				tester->received_string[received_index]);

			diffs_reported++;
		}
		if (diffs_reported == diffs_to_report_max) {
			/* Don't print them all if there are more of X differences. */
			fprintf(stderr, "[EE] more differences may be present, but not showing them\n");
			break;
		}
	}

	if (0 != strcmp(tester->input_string, tester->received_string)) {
		if (0 == diffs_reported) {
			/* Because of condition in 'for' loop we might
			   skipped checking end of one of strings. */
			fprintf(stderr, "[EE] difference appears to be at beginning of one of strings\n");
		}
	}

	return;
}




/**
   @brief Configure a receiver's tester before using the tester

   @reviewedon 2023.08.15

   @param[in/out] tester Tester to configure
   @param[in] easy_rec Easy receiver that should be tested
   @param[in] use_ranger Whether to vary speed of test generator when generating Morse Code
*/
void cw_rec_tester_configure(cw_rec_tester_t * tester, cw_easy_legacy_receiver_t * easy_rec, bool use_ranger)
{
	cw_rec_tester_init_text_buffers(tester, false);
	/* Using Null sound system because this generator is only
	   used to enqueue text and control key. Sound will be played
	   by main generator used in tested code. */
	cw_gen_config_t gen_conf = {
		  .sound_system = CW_AUDIO_NULL
		, .sound_device = ""
	};

	tester->helper_gen = cw_gen_new(&gen_conf);

	/* TODO (acerion) 2023.08.15: create cw_gen_register_low_level_callback_internal() to hide tq. */
	cw_tq_register_low_level_callback_internal(tester->helper_gen->tq, low_tone_queue_callback, tester, 5);

	/* TODO (acerion) 2023.08.15: it appears that the call to
	   cw_key_register_generator() is not necessary. */
	cw_key_register_generator(&tester->key, tester->helper_gen);
	cw_gen_register_value_tracking_callback_internal(tester->helper_gen, test_callback_func, (void *) easy_rec);

	if (use_ranger) {
		/* TODO: use full range of allowed speeds. */
		/* TODO (acerion) 2023.08.15: it appears that the situations where
		   ranger is not being used are not well documented. xcwcp uses
		   tester, but doesn't use ranger. See if accidental use of
		   uninitialized ranger is handled correctly. */
		cwtest_param_ranger_init(&tester->speed_ranger, 6 /* CW_SPEED_MIN */, 40 /* CW_SPEED_MAX */, 1, cw_gen_get_speed(tester->helper_gen));
		cwtest_param_ranger_set_interval_sec(&tester->speed_ranger, 4);
		cwtest_param_ranger_set_plateau_length(&tester->speed_ranger, 6);
	}
}




/**
   @brief Function called on each change of state of helper generator

   The function tells the easy receiver (obtained from @p arg) that a change
   of key state (@p key_state) has occurred.

   @reviewedon 2023.08.15

   @param[in/out] arg Pointer to easy receiver under test
   @param[in] key_state Current state of helper generator (here called a 'state of key'): mark or space
*/
static void test_callback_func(void * arg, int key_state)
{
	/*
	  Inform easy receiver about new state of helper generator. From
	  receiver's point of view the mark/space states of generator look like
	  states of straight key, so use the straight key ("sk") function of the
	  easy receiver.

	  Easy receiver (and the libcw receiver embedded in the easy receiver)
	  will process the new state and we will later try to poll a character
	  or space from the easy receiver.
	*/

	cw_easy_legacy_receiver_t * easy_rec = (cw_easy_legacy_receiver_t *) arg;
	//fprintf(stderr, "Callback function, key state = %d\n", key_state);
	cw_easy_legacy_receiver_sk_event(easy_rec, key_state);
}




/**
   @brief Add new characters to helper generator's queue

   This callback is called each time the helper generator's internal queue
   runs low. The callback adds new characters to the queue. The characters
   come from tester's input string.

   Register this callback in generator with
   cw_tq_register_low_level_callback_internal().

   @reviewedon 2023.08.15

   @param[in/out] arg Pointer to receiver's tester
*/
static void low_tone_queue_callback(void * arg)
{
	cw_rec_tester_t * tester = (cw_rec_tester_t *) arg;

	for (int i = 0; i < tester->characters_to_enqueue; i++) {
		const char c = tester->input_string[tester->input_string_i];
		if ('\0' == c) {
			/* End of input string. We can/should now unregister
			   ourselves. */
			cw_tq_register_low_level_callback_internal(tester->helper_gen->tq, NULL, NULL, 0);
			break;
		} else {
			cw_gen_enqueue_character(tester->helper_gen, c);
			tester->input_string_i++;
		}
	}
}



/**
   @brief Generate input events for tested receiver

   Use helper generator to generate keying events (with their timestamps) for
   receiver.

   We could generate the events using a big array of timestamps and a call to
   usleep(), but instead we are using a helper generator that can inform the
   receiver when marks/spaces start.

   @param[in/out] arg_tester Tester of easy receiver
*/
static void * cw_rec_tester_play_string(void * arg_tester)
{
	cw_rec_tester_t * tester = arg_tester;

	/* Start sending the test string. Registered keying callback will be
	   called on every mark/space. Enqueue only initial part of string
	   ('initial_count'), just to start sending, the rest should be sent by
	   'low watermark' callback. */
    const int initial_count = 5;
	cw_gen_start(tester->helper_gen);
	for (int i = 0; i < initial_count; i++) {
		const char c = tester->input_string[tester->input_string_i];
		if ('\0' == c) {
			/* A very short input string. */
			break;
		} else {
			cw_gen_enqueue_character(tester->helper_gen, c);
			tester->input_string_i++;
		}
	}

	/* Wait for all characters to be played out. During this wait the above
	   'initial_count' characters are played, and the generator's 'low
	   watermark' callbacks adds more and more characters from tester's
	   input string. At some point the input string runs out of characters,
	   the last character is finally enqueued and played, and the 'wait'
	   returns. */
	cw_tq_wait_for_level_internal(tester->helper_gen->tq, 0);
	cw_usleep_internal(1 * CW_USECS_PER_SEC);

	cw_gen_delete(&tester->helper_gen); /* TODO (2022.01.03) if we are doing delete() in this function, then should we also do new() here? */
	tester->generating_in_progress = false;

	return NULL;
}




void cw_rec_tester_start_test_code(cw_rec_tester_t * tester)
{
	/* TODO 2022.01.03: should we set the flag here or in thread
	   function? */
	tester->generating_in_progress = true;

	pthread_create(&tester->receiver_test_code_thread_id, NULL, cw_rec_tester_play_string, tester);
}




void cw_rec_tester_stop_test_code(cw_rec_tester_t * tester)
{
	pthread_cancel(tester->receiver_test_code_thread_id);
}




int cw_rec_tester_on_character(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Character: '%c'\n", erd->character);

	tester->received_string[tester->received_string_i++] = erd->character;

	cw_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_iws, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Character: failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_iws != erd->is_iws) {
		fprintf(stderr, "[EE] Character: 'is end of word' markers mismatch: %d != %d\n", test_data.is_iws, erd->is_iws);
		return CW_FAILURE;
	}

	if (test_data.is_iws) {
		fprintf(stderr, "[EE] Character: 'is end of word' marker is unexpectedly 'true'\n");
		return CW_FAILURE;
	}

	const char looked_up = cw_representation_to_character(test_data.representation);
	if (0 == looked_up) {
		fprintf(stderr, "[EE] Character: Failed to look up character for representation\n");
		return CW_FAILURE;
	}

	if (looked_up != erd->character) {
		fprintf(stderr, "[EE] Character: Looked up character is different than received: %c != %c\n", looked_up, erd->character);
	}

	fprintf(stderr, "[II] Character: Representation: %c -> '%s'\n",
		erd->character, test_data.representation);

	/* Not entirely a success if looked up char does not match received
	   character, but returning failure here would lead to calling
	   exit(). */
	return CW_SUCCESS;
}




int cw_rec_tester_on_space(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer)
{
	fprintf(stderr, "[II] Space:\n");

	/* cw_receive_character() will return through 'c' variable the last
	   character that was polled before space.

	   Maybe this is good, maybe this is bad, but this is the legacy
	   behaviour that we will keep supporting. */
	if (' ' == erd->character) {
		fprintf(stderr, "[EE] Space: returned character should not be space\n");
		return CW_FAILURE;
	}


	tester->received_string[tester->received_string_i++] = ' ';

	cw_rec_data_t test_data = { 0 };
	int cw_ret = cw_receive_representation(timer, test_data.representation, &test_data.is_iws, &test_data.is_error);
	if (CW_SUCCESS != cw_ret) {
		fprintf(stderr, "[EE] Space: Failed to get representation\n");
		return CW_FAILURE;
	}

	if (test_data.is_iws != erd->is_iws) {
		fprintf(stderr, "[EE] Space: 'is end of word' markers mismatch: %d != %d\n", test_data.is_iws, erd->is_iws);
		return CW_FAILURE;
	}

	if (!test_data.is_iws) {
		fprintf(stderr, "[EE] Space: 'is end of word' marker is unexpectedly 'false'\n");
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}



