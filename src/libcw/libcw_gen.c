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
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/




/**
   @file libcw_gen.c

   @brief Generate pcm samples according to tones from tone queue, and
   send them to sound sink.

   Functions operating on one of core elements of libcw: a generator.

   Generator is an object that has access to sound sink (soundcard,
   console buzzer, null sound device) and that can generate dots and
   dashes using the sound sink.

   You can request generator to produce sound by using *_enqeue_*()
   functions.

   The inner workings of the generator seem to be quite simple:
   1. dequeue tone from tone queue
   2. recalculate tone duration in microseconds into tone length in samples
   3. for every sample in tone, calculate sine wave sample and
      put it in generator's constant size buffer
   4. if buffer is full of sine wave samples, push it to sound sink
   5. since buffer is shorter than (almost) any tone, you will
      recalculate contents of the buffer and push it to sound sink
      multiple times per tone
   6. if you iterated over all samples in tone, but you still didn't
      fill up that last buffer, go to step #1
   7. if there are no more tones in queue, pad the buffer with silence,
      and push the buffer to sound sink.

   Looks simple, right? But it's the little details that ruin it all.
   One of the details is tone's slopes.

   TODO: we need additional function that returns only after all tones have
   been sent to sound sink AND played. Currently client program can call
   cw_gen_wait_for_queue_level(gen, 0), but the function returns when the
   last tone is still being played. That's too early in some situations, we
   need a function that returns after the last tone has been played and
   generator returned to "empty,idle" state.
*/




#include "config.h"

#include <errno.h>
#include <inttypes.h> /* uint32_t */
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(HAVE_STRING_H)
# include <string.h>
#endif

#if defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#if defined(__linux__)
#include <sys/prctl.h> /* prctl() */
#elif defined(__FreeBSD__)
#include <pthread_np.h> /* pthread_set_name_np() */
#endif




#include "libcw2.h"
#include "libcw_alsa.h"
#include "libcw_console.h"
#include "libcw_data.h"
#include "libcw_debug.h"
#include "libcw_debug_internal.h"
#include "libcw_gen.h"
#include "libcw_gen_internal.h"
#include "libcw_null.h"
#include "libcw_oss.h"
#include "libcw_rec.h"
#include "libcw_signal.h"
#include "libcw_utils.h"




#define MSG_PREFIX "libcw/gen: "

/* Measuring how long some thread operations take. */
#define LIBCW_GEN_DEBUG_THREAD_TIMING   1

/* Our own definition, to have it as a float. */
static const float CW_PI = 3.14159265358979323846F;




/* From libcw_debug.c. */
extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




/* Most of sound systems (excluding console) should be configured to
   have specific sample rate. Some sound systems (with connection with
   given hardware) can support several different sample rates. Values of
   supported sample rates are standardized. Here is a list of them to be
   used by this library.
   When the library configures given sound system, it tries if the system
   will accept a sample rate from the table, starting from the first one.
   If a sample rate is accepted, rest of sample rates is not tested anymore. */
const unsigned int cw_supported_sample_rates[] = {
	44100,
	48000,
	32000,
	22050,
	16000,
	11025,

	 /* This is the lowest value, dictated by value of
	    CW_FREQUENCY_MAX, but in practice I found that with this
	    value the generation of sound rarely works at all. */
	 8000,

	    0 /* guard */
};




static cw_ret_t cw_gen_value_tracking_internal(cw_gen_t * gen, const cw_tone_t * tone, cw_queue_state_t queue_state);
static void cw_gen_value_tracking_set_value_internal(cw_gen_t * gen, volatile cw_key_t * key, cw_key_value_t value);
static void cw_gen_empty_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone);
static void cw_gen_silencing_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone);
static void cw_gen_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone);




/* Every sound system opens an sound device: a default device, or some
   other device. Default devices have their default names, and here is
   a list of them. It is indexed by values of "enum cw_audio_systems". */
static const char * default_sound_devices[] = {
	(char *) NULL,          /* CW_AUDIO_NONE */
	CW_DEFAULT_NULL_DEVICE, /* CW_AUDIO_NULL */
	CW_DEFAULT_CONSOLE_DEVICE,
	CW_DEFAULT_OSS_DEVICE,
	CW_DEFAULT_ALSA_DEVICE,
	CW_DEFAULT_PA_DEVICE,
	(char *) NULL }; /* just in case someone decided to index the table with CW_AUDIO_SOUNDCARD */




/* Generic constants - common for all sound systems (or not used in some of systems). */

static const int CW_AUDIO_VOLUME_RANGE = (1U << 15U);  /* 2^15 = 32768 */

/*
  Shortest duration of time (in microseconds) that is used by libcw for idle
  waiting and idle loops. If a libcw function needs to wait for something, or
  make an idle loop, it should call usleep(N * gen->quantum_duration)

  This is also duration of a single "forever" tone.

  Don't make the quantum duration too short. Short quantum duration will have
  two negative results:
  1. you can't create a nice slope with just 4 samples - you will hear a
  click.
  2. you will get very frequent dequeues of 'forever' tone that has quantum
  duration.

  n_samples = gen->sample_rate * duration / (usecs per sec);
  (code calculating n_samples uses slightly modified formula)

  sample rate | duration |   n || duration |   n
  ------------------------------------------------
  48000       |      100 |   4 ||      500 |  24
  44100       |      100 |   4 ||      500 |  22
  32000       |      100 |   3 ||      500 |  16
  22050       |      100 |   2 ||      500 |  11
  16000       |      100 |   1 ||      500 |   8
  11025       |      100 |   1 ||      500 |   5
   8000       |      100 |   0 ||      500 |   4
*/
static const int CW_AUDIO_QUANTUM_DURATION_INITIAL = 500;  /* [us] */




/**
   @brief Get a readable label of current sound system

   Get a human-readable string describing sound system associated currently
   with given @p gen.

   The function returns through @p buffer one of following strings: "None",
   "Null", "Console", "OSS", "ALSA", "PulseAudio", "Soundcard".

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in] gen generator for which to check sound system label
   @param[out] buffer output buffer where the label will be saved
   @param[in] size total size of the buffer (including space for terminating NUL)

   @return @p buffer
*/
char * cw_gen_get_sound_system_label_internal(const cw_gen_t * gen, char * buffer, size_t size)
{
	if (buffer) {
		snprintf(buffer, size, "%s", cw_get_audio_system_label(gen->sound_system));
	}

	return buffer;
}




/*
  TODO: the function must detect already running generator, and return
  failure when it is detected.
*/
cw_ret_t cw_gen_start(cw_gen_t * gen)
{
	gen->phase_offset = 0.0F;

#ifdef GENERATOR_CLIENT_THREAD
	/* This generator exists in client's application thread.
	   Generator's 'dequeue and generate' function will be a separate thread. */
	gen->library_client.thread_id = pthread_self();
#endif

	if (gen->sound_system != CW_AUDIO_NULL
	    && gen->sound_system != CW_AUDIO_CONSOLE
	    && gen->sound_system != CW_AUDIO_OSS
	    && gen->sound_system != CW_AUDIO_ALSA
	    && gen->sound_system != CW_AUDIO_PA) {

		gen->do_dequeue_and_generate = false;

		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "unsupported sound system %d", gen->sound_system);
		return CW_FAILURE;
	}
	/* This should be set to true before launching
	   cw_gen_dequeue_and_generate_internal(), because loop in the
	   function run only when the flag is set. */
	gen->do_dequeue_and_generate = true;


#if LIBCW_GEN_DEBUG_THREAD_TIMING
	/* Debug code to measure how long it takes to create thread. */
	struct timeval before;
	struct timeval after;
	gettimeofday(&before, NULL);
#endif


	/* cw_gen_dequeue_and_generate_internal() is THE
	   function that does the main job of generating
	   tones. */
	int rv = pthread_create(&gen->thread.id, &gen->thread.attr,
				cw_gen_dequeue_and_generate_internal,
				(void *) gen);
	if (rv != 0) {
		gen->do_dequeue_and_generate = false;

		cw_debug_msg (&cw_debug_object, CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
			      MSG_PREFIX "failed to create %s generator thread", cw_get_audio_system_label(gen->sound_system));
		return CW_FAILURE;
	} else {
		/* TODO: shouldn't we be doing it in generator's thread function? */
		gen->thread.running = true;

#if LIBCW_GEN_DEBUG_THREAD_TIMING
		/* Debug code to measure how long it takes to create thread.
		   My main laptop:
		       ALSA: 32 - 79 us
		       PulseAudio: 20 - 65 us
		*/
		gettimeofday(&after, NULL);
		const int delta = cw_timestamp_compare_internal(&before, &after);
		cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
		MSG_PREFIX "generator thread timing: creating thread took %d us", delta);
#endif


		/* FIXME: For some yet unknown reason we have to put
		   usleep() here, otherwise a generator may
		   work incorrectly */
		usleep(100000);
#ifdef ENABLE_DEV_LIBCW_DEBUGGING
		cw_dev_debug_print_generator_setup_internal(gen);
#endif
		return CW_SUCCESS;
	}
}




/**
   @brief Silence the generator

   Force a sound sink currently used by generator @p gen to go
   silent.

   The function does not clear/flush tone queue, nor does it stop the
   generator. It just makes sure that sound sink (console / OSS / ALSA /
   PulseAudio) does not produce a sound of any frequency and any volume.

   You probably want to call cw_tq_flush_internal(gen->tq) before calling
   this function. TODO: shouldn't cw_tq_flush_internal() be called inside of
   this function?

   @internal
   @reviewed 2020-10-12
   @endinternal

   @param[in] gen generator using a sound sink that should be silenced

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure to silence a sound sink
*/
cw_ret_t cw_gen_silence_internal(cw_gen_t * gen)
{
	if (NULL == gen) {
		/* This may happen because the process of finalizing
		   usage of libcw is rather complicated. This should
		   be somehow resolved. */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_WARNING,
			      MSG_PREFIX "called the function for NULL generator");
		return CW_SUCCESS;
	}

	if (!gen->thread.running) {
		/* Silencing a generator means enqueueing and generating
		   a tone with zero frequency.  We shouldn't do this
		   when a "dequeue-and-generate-a-tone" function is not
		   running (anymore). This is not an error situation,
		   so return CW_SUCCESS. */
		return CW_SUCCESS;
	}

#if 1
	/* Tell 'dequeue and generate' thread function to go silent.

	   TODO: What if the last tone on queue is a Very Long Tone,
	   and silencing the generator will take a long time? This is
	   a real problem that can be observed with large GAP
	   parameter (e.g. 60): a program that is using long gaps is
	   waiting for a long time before exiting on Ctrl-C. */
	gen->silencing_initialized = true;
	cw_gen_wait_for_queue_level(gen, 0);

	/* Somewhere there may be a key in "down" state and we need to make
	   it go "up", regardless of sound sink (even for CW_AUDIO_NULL,
	   because that sound system can also be used with a key).  Otherwise
	   the key may stay in "down" state forever and the sound will be
	   played after "silencing" of the generator.

	   TODO: this code is not being called in some of libcw tests,
	   e.g. in this one:
	   ./src/libcw/tests/libcw_tests -S c -d /dev/console -N legacy_api_test_send_character_and_string
	*/
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
	tone.debug_id = 'd';
	cw_ret_t cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	/* Reset on stopping of the generator. */
	gen->space_units_count = 0;
	cw_gen_wait_for_queue_level(gen, 0);
	cw_gen_wait_for_end_of_current_tone(gen);

	if (gen->sound_system == CW_AUDIO_CONSOLE) {
		/* Just in case...
		   TODO: is it still necessary after adding the quantum of
		   silence above? */
		cw_console_silence_internal(gen);
	}

	return cwret;

#else

	/* TODO: Tone queue may have e.g. 10 tones enqueued at this
	   moment. To have a "gentle" silencing of generator, we shouldn't
	   interrupt in the middle of current tone. What we should do is:
	   - remove all tones ahead of current tone (perhaps starting from
	   the end).
	   - see how long the current tone is. If it's relatively short, let
	   it play to the end, but if it's long tone, interrupt it.
	   - see if the current tone is "forever" tone. If it is, end the
	   "forever" tone using method suitable for such tone.
	   - only after all this you can enqueue a tone silent that will
	     ensure that a key is not in "down" state.

	   What if the last tone on queue is a Very Long Tone, and silencing
	   the generator will take a long time?
	*/

	/* Somewhere there may be a key in "down" state and we need to make
	   it go "up", regardless of sound sink (even for CW_AUDIO_NULL,
	   because that sound system can also be used with a key).  Otherwise
	   the key may stay in "down" state forever and the sound will be
	   played after "silencing" of the generator. */
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
	cw_ret_t status = cw_tq_enqueue_internal(gen->tq, &tone);
	/* Reset on stopping of the generator. */
	gen->space_units_count = 0;

	if (gen->sound_system == CW_AUDIO_NULL
	    || gen->sound_system == CW_AUDIO_OSS
	    || gen->sound_system == CW_AUDIO_ALSA
	    || gen->sound_system == CW_AUDIO_PA) {

		/* Allow some time for playing the last tone. */
		usleep(2 * tone.duration);

	} else if (gen->sound_system == CW_AUDIO_CONSOLE) {
		/* Sine wave generation should have been stopped
		   by a code generating dots/dashes, but
		   just in case...

		   TODO: is it still necessary after adding the
		   quantum of silence above? */
		cw_console_silence_internal(gen);
	} else {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR,
			      MSG_PREFIX "called silence() function for generator without sound system specified");
	}

	if (gen->sound_system == CW_AUDIO_ALSA) {
		/* "Stop a PCM dropping pending frames. " */
		cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
			      MSG_PREFIX "asking ALSA to drop frames on silence()");
		cw_alsa_drop_internal(gen);
	}

	/* TODO: we just want to silence the generator, right? So we don't stop it.
	   This line of code has been disabled some time before 2017-01-26. */
	//gen->do_dequeue_and_generate = false;

	return status;
#endif
}




cw_gen_t * cw_gen_new(const cw_gen_config_t * gen_conf)
{
#ifdef ENABLE_DEV_LIBCW_DEBUGGING
	fprintf(stderr, "libcw build %s %s\n", __DATE__, __TIME__);
#endif

	cw_assert (gen_conf->sound_system != CW_AUDIO_NONE, MSG_PREFIX "can't create generator with sound system '%s'", cw_get_audio_system_label(gen_conf->sound_system));

	cw_gen_t * gen = (cw_gen_t *) calloc(1, sizeof (cw_gen_t));
	if (NULL == gen) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_STDLIB, CW_DEBUG_ERROR, MSG_PREFIX "calloc()");
		return (cw_gen_t *) NULL;
	}



	/* Tone queue. */
	{
		gen->tq = cw_tq_new_internal();
		if (NULL == gen->tq) {
			cw_gen_delete(&gen);
			return (cw_gen_t *) NULL;
		} else {
			/* Sometimes tq needs to access a key associated with generator. */
			gen->tq->gen = gen;
		}
	}



	/* Parameters. */
	{
		/* Generator's basic parameters. */
		gen->send_speed = CW_SPEED_INITIAL;
		gen->frequency = CW_FREQUENCY_INITIAL;
		gen->volume_percent = CW_VOLUME_INITIAL;
		gen->volume_abs = (gen->volume_percent * CW_AUDIO_VOLUME_RANGE) / 100;
		gen->gap = CW_GAP_INITIAL;
		gen->weighting = CW_WEIGHTING_INITIAL;


		/* Generator's timing parameters. */
		gen->durations.dot_duration = 0;
		gen->durations.dash_duration = 0;
		gen->durations.ims_duration = 0;
		gen->durations.ics_duration = 0;
		gen->durations.iws_duration = 0;
		gen->durations.additional_space_duration = 0;
		gen->durations.adjustment_space_duration = 0;


		/* Generator's misc parameters. */
		gen->quantum_duration = CW_AUDIO_QUANTUM_DURATION_INITIAL;


		gen->parameters_in_sync = false;
	}



	/* Misc fields. */
	{
		/* Sound buffer and related items. */
		gen->buffer = NULL;
		gen->buffer_n_samples = -1;
		gen->buffer_sub_start = 0;
		gen->buffer_sub_stop  = 0;

		gen->sample_rate = 0;
		gen->phase_offset = -1;


		/* Tone parameters. */
		gen->tone_slope.duration = CW_AUDIO_SLOPE_DURATION;
		gen->tone_slope.shape = CW_TONE_SLOPE_SHAPE_RAISED_COSINE;
		gen->tone_slope.amplitudes = NULL;
		gen->tone_slope.n_amplitudes = 0;


		/* Library's client. */
#ifdef GENERATOR_CLIENT_THREAD
		gen->library_client.thread_id = -1;
#endif
		gen->library_client.name = (char *) NULL;


		/* CW key associated with this generator. */
		gen->key = (cw_key_t *) NULL;
	}


	/* pthread */
	{
		gen->thread.id = (pthread_t) -1; /* FIXME: thread id type is opaque. Don't assign -1. */
		pthread_attr_init(&gen->thread.attr);
		/* Thread must be joinable in order to make a safe call to
		   pthread_kill(thread_id, 0). pthreads are joinable by
		   default, but I take this explicit call as a good
		   opportunity to make this comment. */
		pthread_attr_setdetachstate(&gen->thread.attr, PTHREAD_CREATE_JOINABLE);
		gen->thread.running = false;

		/* TODO: doesn't this duplicate gen->thread.running flag? */
		gen->do_dequeue_and_generate = false;
	}


	/* Sound system. */
	{
		/* gen->sound_system = sound_system; */ /* We handle this field below. */

#ifdef ENABLE_DEV_PCM_SAMPLES_FILE
		gen->dev_raw_sink = -1;
#endif

		/* Sound system - console. */
		gen->console.sound_sink_fd = -1;
		gen->console.cw_value = CW_KEY_VALUE_OPEN;

		/* Sound system - OSS. */
#ifdef LIBCW_WITH_OSS
		gen->oss_data.sound_sink_fd = -1;
		gen->oss_data.version.x = 0;
		gen->oss_data.version.y = 0;
		gen->oss_data.version.z = 0;
#endif

		/* Sound system - ALSA. */
#ifdef LIBCW_WITH_ALSA
		gen->alsa_data.pcm_handle = NULL;
#endif

		/* Sound system - PulseAudio. */
#ifdef LIBCW_WITH_PULSEAUDIO
		gen->pa_data.simple = NULL;
#endif

		cw_ret_t cwret = cw_gen_new_open_internal(gen, gen_conf);
		if (cwret == CW_FAILURE) {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "failed to open sound sink for sound system '%s' and device '%s'",
				      cw_get_audio_system_label(gen_conf->sound_system),
				      gen_conf->sound_device);
			cw_gen_delete(&gen);
			return (cw_gen_t *) NULL;
		}

		if (gen_conf->sound_system == CW_AUDIO_NULL
		    || gen_conf->sound_system == CW_AUDIO_CONSOLE) {

			; /* The two types of sound output don't require audio buffer. */
		} else {
			gen->buffer = (cw_sample_t *) calloc(gen->buffer_n_samples, sizeof (cw_sample_t));
			if (!gen->buffer) {
				cw_debug_msg (&cw_debug_object, CW_DEBUG_STDLIB, CW_DEBUG_ERROR,
					      MSG_PREFIX "calloc()");
				cw_gen_delete(&gen);
				return (cw_gen_t *) NULL;
			}
		}

		/* Set slope that late, because it uses value of sample rate.
		   The sample rate value is set in
		   cw_gen_new_open_internal(). */
		cwret = cw_gen_set_tone_slope(gen, CW_TONE_SLOPE_SHAPE_RAISED_COSINE, CW_AUDIO_SLOPE_DURATION);
		if (cwret == CW_FAILURE) {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR,
				      MSG_PREFIX "failed to set slope");
			cw_gen_delete(&gen);
			return (cw_gen_t *) NULL;
		}
	}

	/* Tracking of generator's value. */
	{
		gen->value_tracking.value = CW_KEY_VALUE_OPEN;
		gen->value_tracking.value_tracking_callback_func = NULL;
		gen->value_tracking.value_tracking_callback_arg = NULL;
	}
#if 0
	/* Part of old inter-thread comm. Disabled on 2020-09-01. */
	cw_sigalrm_install_top_level_handler_internal();
#endif
	return gen;
}




void cw_gen_delete(cw_gen_t **gen)
{
	cw_assert (NULL != gen, MSG_PREFIX "generator is NULL");

	if (NULL == *gen) {
		return;
	}

	if ((*gen)->do_dequeue_and_generate) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_DEBUG,
			      MSG_PREFIX "you forgot to call cw_gen_stop()");
		cw_gen_stop(*gen);
	}

	/* Wait for "write" thread to end accessing output
	   file descriptor. I have come up with value 500
	   after doing some experiments.

	   FIXME: magic number. I think that we can come up
	   with algorithm for calculating the value. */
	usleep(500);

	free((*gen)->buffer);
	(*gen)->buffer = NULL;

	if ((*gen)->close_sound_device) {
		(*gen)->close_sound_device(*gen);
	} else {
		/* This may happen e.g. when generator was not created properly
		   because a requested sound system is not available. Such system
		   probably was never opened, so closing it is not required.

		   Notice that there may be also other situations when 'close' is not
		   set.

		   TODO acerion 2023.09.29: I noticed this message only because
		   cw_debug_object_dev object is being used in test binary, and the
		   test binary experienced some specific problem. It's possible that
		   this situation happens also in cw/cwcp/xcwcp, but I haven't
		   noticed it yet. Find the root cause of this problem and fix it.
		*/
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_WARNING, MSG_PREFIX "'close' function pointer is NULL");
	}

	pthread_attr_destroy(&(*gen)->thread.attr);

	free((*gen)->library_client.name);
	(*gen)->library_client.name = NULL;

	free((*gen)->tone_slope.amplitudes);
	(*gen)->tone_slope.amplitudes = NULL;

	cw_tq_delete_internal(&(*gen)->tq);

	(*gen)->sound_system = CW_AUDIO_NONE;

	free(*gen);
	*gen = NULL;

	return;
}




cw_ret_t cw_gen_stop(cw_gen_t * gen)
{
	if (NULL == gen) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_WARNING,
			      MSG_PREFIX "called the function for NULL generator");
		/* Not really a runtime error, so return
		   CW_SUCCESS. */
		return CW_SUCCESS;
	}


	/* FIXME: Something goes wrong when cw_gen_stop() is called
	   from signal handler. pthread_cond_destroy() hangs because
	   there is an interrupted pthread_cond_wait() in frame
	   #8. Signalling it won't help because even if a condition
	   variable is signalled, the function won't be able to
	   continue. Stopping of generator, especially in emergency
	   situations, needs to be re-thought.

	   This is probably fixed by not calling
	   pthread_cond_destroy() in cw_tq_delete_internal(). */

	/*
#0  __pthread_cond_destroy (cond=0x1b130f0) at pthread_cond_destroy.c:77
#1  0x00007f15b393179d in cw_tq_delete_internal (tq=0x1b13118) at libcw_tq.c:219
#2  0x00007f15b392e2ca in cw_gen_delete (gen=0x1b13118, gen@entry=0x6069e0 <generator>) at libcw_gen.c:608
#3  0x000000000040207f in cw_atexit () at cw.c:668
#4  0x00007f15b35b6bc9 in __run_exit_handlers (status=status@entry=0, listp=0x7f15b39225a8 <__exit_funcs>,
    run_list_atexit=run_list_atexit@entry=true) at exit.c:82
#5  0x00007f15b35b6c15 in __GI_exit (status=status@entry=0) at exit.c:104
#6  0x00000000004020d7 in signal_handler (signal_number=2) at cw.c:686
#7  <signal handler called>
#8  pthread_cond_wait@@GLIBC_2.3.2 () at ../nptl/sysdeps/unix/sysv/linux/x86_64/pthread_cond_wait.S:185
#9  0x00007f15b3931f3b in cw_tq_wait_for_level_internal (tq=0x1af5be0, level=level@entry=1) at libcw_tq.c:964
#10 0x00007f15b392f938 in cw_gen_wait_for_queue_level (gen=<optimized out>, level=level@entry=1) at libcw_gen.c:2701
#11 0x000000000040241e in send_cw_character (c=c@entry=102, is_partial=is_partial@entry=0) at cw.c:501
#12 0x0000000000401d3d in parse_stream (stream=0x7f15b39234e0 <_IO_2_1_stdin_>) at cw.c:538
#13 main (argc=<optimized out>, argv=<optimized out>) at cw.c:652
	*/

	cw_tq_flush_internal(gen->tq);

	if (CW_SUCCESS != cw_gen_silence_internal(gen)) {
		return CW_FAILURE;
	}

	cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
		      MSG_PREFIX "setting gen->do_dequeue_and_generate to false");

	gen->do_dequeue_and_generate = false;

	if (!gen->thread.running) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_INFO, MSG_PREFIX "EXIT: seems that thread function was not started at all");

		/* Don't call pthread_kill() on non-initialized
		   thread.id. The generator wasn't even started, so
		   let's return CW_SUCCESS. */

		/* TODO: what about code that doesn't use signals?
		   Should we return here? */
		return CW_SUCCESS;
	}


	/*
	  "while (gen->do_dequeue_and_generate)" loop in thread function may
	  be in a state where dequeue() function returned IDLE state, and the
	  loop is waiting for new tone.

	  This is to force the loop to start new cycle, make the loop notice
	  that gen->do_dequeue_and_generate is false, and to get the thread
	  function to return (and thus to end the thread).
	*/
	pthread_mutex_lock(&gen->tq->wait_mutex);
	pthread_cond_broadcast(&gen->tq->wait_var);
	pthread_mutex_unlock(&gen->tq->wait_mutex);
#if 0
	/* Original implementation using signals. */
	/* This was disabled some time before 2017-01-19. */
	pthread_kill(gen->thread.id, SIGALRM);
#endif


	/*
	  TODO: there is something wrong with the keyer machine state for
	  iambic keyer if we have to reset it here (I'm resetting straight
	  key too, just for completeness).

	  Replication scenario:
	  1. comment "#define LIBCW_KEY_TESTS_WORKAROUND" in
	     libcw_legacy_api_tests.c
	  2. comment these two function calls
	  3. run './src/libcw/tests/libcw_tests -S a -X plughw -A k'
	  4. see that 'legacy_api_test_iambic_key_dash' test hangs
	*/
	if (gen->key) {
		cw_key_ik_reset_state_internal(gen->key);
		cw_key_sk_reset_state_internal(gen->key);
	}

	return cw_gen_join_thread_internal(gen);
}




/**
   @brief Wrapper for pthread_join() and debug code

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in] gen generator for which to join its thread

   @return CW_SUCCESS if joining succeeded
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_gen_join_thread_internal(cw_gen_t * gen)
{
	/* TODO: this comment may no longer be true and necessary.
	   Sleep a bit to postpone closing a device.  This way we can
	   avoid a situation when "do_dequeue_and_generate" is set to false
	   and device is being closed while a new buffer is being
	   prepared, and while write() tries to write this new buffer
	   to already closed device.

	   Without this sleep(), writei() from ALSA library may
	   return "File descriptor in bad state" error - this
	   happened when writei() tried to write to closed ALSA
	   handle.

	   The delay also allows the generator function thread to stop
	   generating tone (or for tone queue to get out of CW_TQ_EMPTY state
	   (TODO: verify this comment, does it describe correct tq state?)
	   and exit before we resort to killing generator function thread. */
	cw_usleep_internal(1 * CW_USECS_PER_SEC);


#if LIBCW_GEN_DEBUG_THREAD_TIMING
	/* Debug code to measure how long it takes to join threads. */
	struct timeval before;
	struct timeval after;
	gettimeofday(&before, NULL);
#endif


	int rv = pthread_join(gen->thread.id, NULL);


#if LIBCW_GEN_DEBUG_THREAD_TIMING
	/* Debug code to measure how long it takes to join threads. */
	gettimeofday(&after, NULL);
	const int delta = cw_timestamp_compare_internal(&before, &after);
	cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
		      MSG_PREFIX "generator thread timing: joining thread took %d us", delta);
#endif


	if (rv == 0) {
		gen->thread.running = false;
		return CW_SUCCESS;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR, MSG_PREFIX "failed to join threads: '%s'", strerror(rv));
		return CW_FAILURE;
	}
}




/**
   @brief Open sound system

   A wrapper for code trying to open sound device specified by @p
   sound_system.  Open sound system will be assigned to given
   generator. Caller can also specify sound device to use instead of a
   default one. If @p device_name is NULL or empty string, library-default
   device will be used.

   @internal
   @reviewed 2020-11-14
   @endinternal

   @param[in] gen generator for which to open a sound system
   @param[in] gen_conf

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_gen_new_open_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf)
{
	/* FIXME: this functionality is partially duplicated in
	   src/cwutils/cw_common.c/cw_gen_new_from_config() */

	/* This function deliberately checks all possible values of
	   sound system name in separate 'if' clauses before it gives
	   up and returns CW_FAILURE. PA/OSS/ALSA are combined with
	   SOUNDCARD, so I have to check all three of them (because @p
	   sound_system may be set to SOUNDCARD). And since I check
	   the three in separate 'if' clauses, I can check all other
	   values of sound system as well. */


	if (gen_conf->sound_system == CW_AUDIO_NULL) {

		if (cw_is_null_possible(gen_conf->sound_device)) {
			cw_null_init_gen_internal(gen);
			return gen->open_and_configure_sound_device(gen, gen_conf);
		}
	}

	if (gen_conf->sound_system == CW_AUDIO_PA
	    || gen_conf->sound_system == CW_AUDIO_SOUNDCARD) {

		if (cw_is_pa_possible(gen_conf->sound_device)) {
			cw_pa_init_gen_internal(gen);
			return gen->open_and_configure_sound_device(gen, gen_conf);
		}
	}

	if (gen_conf->sound_system == CW_AUDIO_OSS
	    || gen_conf->sound_system == CW_AUDIO_SOUNDCARD) {

		if (cw_is_oss_possible(gen_conf->sound_device)) {
			cw_oss_init_gen_internal(gen);
			return gen->open_and_configure_sound_device(gen, gen_conf);
		}
	}

	if (gen_conf->sound_system == CW_AUDIO_ALSA
	    || gen_conf->sound_system == CW_AUDIO_SOUNDCARD) {

		if (cw_is_alsa_possible(gen_conf->sound_device)) {
			cw_alsa_init_gen_internal(gen);
			return gen->open_and_configure_sound_device(gen, gen_conf);
		}
	}

	if (gen_conf->sound_system == CW_AUDIO_CONSOLE) {

		if (cw_is_console_possible(gen_conf->sound_device)) {
			cw_console_init_gen_internal(gen);
			return gen->open_and_configure_sound_device(gen, gen_conf);
		}
	}

	/* There is no next sound system type to try. */
	return CW_FAILURE;
}




/**
   @brief Dequeue tones and push them to sound output

   This is a thread function.

   Function dequeues tones from tone queue associated with generator
   and then sends them to preconfigured sound output (soundcard, NULL
   or console).

   Function dequeues tones (or waits for new tones in queue) and pushes them
   to sound output as long as generator->do_dequeue_and_generate is true.

   The generator must be fully configured before creating thread with
   this function.

   @internal
   @reviewed 2020-10-17
   @endinternal

   @param[in] arg generator (cast to (void *)) to be used for generating tones

   @return NULL pointer
*/
void * cw_gen_dequeue_and_generate_internal(void * arg)
{
	cw_gen_t * gen = (cw_gen_t *) arg;

	const char name_prefix[] = "deq ";
	char name[sizeof (name_prefix) + sizeof (gen->label)] = { 0 };
	snprintf(name, sizeof (name), "%s%s\n", name_prefix, gen->label);
	name[15] = '\0';
#if defined(__linux__)
	/* Choosing prctl() over pthread_setname_np() for Linux
	   because prctl doesn't require explicit "#define
	   _GNU_SOURCE". */
	prctl(PR_SET_NAME, name, 0, 0, 0);
#elif defined(__FreeBSD__)
	pthread_set_name_np(pthread_self(), name);
#endif

	/* Tone dequeued in previous call to cw_tq_dequeue_internal(). */
	cw_tone_t prev_tone = { 0 };

	/* Tone dequeued in current call to cw_tq_dequeue_internal(). */
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, 0, CW_SLOPE_MODE_STANDARD_SLOPES);

	while (gen->do_dequeue_and_generate) {
		const cw_queue_state_t queue_state = cw_tq_dequeue_internal(gen->tq, &tone);
		if (CW_TQ_EMPTY == queue_state) {

			cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
				      MSG_PREFIX "Detected empty queue");

			cw_gen_value_tracking_internal(gen, &tone, queue_state);
#if 1
			if (gen->on_empty_queue) {
				if (CW_SUCCESS != gen->on_empty_queue(gen)) {
					cw_debug_msg (&cw_debug_object, CW_DEBUG_TONE_QUEUE, CW_DEBUG_ERROR,
						      MSG_PREFIX "handling of empty queue by generator has failed");
				}
			}
#endif

			/* We won't get here while there are some
			   accumulated tones in queue, because
			   cw_tq_dequeue_internal() will be handling
			   them just fine without any need for
			   synchronization or wait().  Only after the
			   queue has been completely drained, we will
			   be forced to wait() here.

			   It's much better to wait only sometimes
			   after cw_tq_dequeue_internal() than wait
			   always before cw_tq_dequeue_internal().

			   We are waiting for kick from enqueue()
			   function informing that a new tone appeared
			   in tone queue.

			   The kick may also come from cw_gen_stop()
			   that gently asks this function to stop
			   idling and nicely return. */

			/* The 'while' loop handles spurious wakeups of
			   pthread_cond_wait() and also ensures that the
			   wait() function is called only when a wait is
			   necessary. TODO: make sure that getting
			   gen->tq->state doesn't require locking a tq
			   mutex. */
			pthread_mutex_lock(&(gen->tq->wait_mutex));
			while (CW_TQ_EMPTY == gen->tq->state && gen->do_dequeue_and_generate) {
				pthread_cond_wait(&gen->tq->wait_var, &gen->tq->wait_mutex);
			}
			pthread_mutex_unlock(&(gen->tq->wait_mutex));

#if 0
			/* Original implementation using signals. */ /* This code has been disabled some time before 2017-01-19. */
			/* TODO: can we / should we specify on which
			   signal exactly we are waiting for? */
			cw_signal_wait_internal();
#endif
			continue;
		}

		const bool is_empty_tone = CW_TQ_EMPTY == queue_state;

		cw_gen_value_tracking_internal(gen, &tone, queue_state);

#ifdef IAMBIC_KEY_HAS_TIMER
		/* Also look at call to cw_key_ik_update_graph_state_internal()
		   made below. Both calls are about updating some internals of
		   key. This one is done before blocking write, the other is
		   done after blocking write. */
		if (gen->key) {
			cw_key_ik_increment_timer_internal(gen->key, tone.duration);
		}
#endif


#ifdef ENABLE_DEV_LIBCW_DEBUGGING
		cw_debug_ev (&cw_debug_object_ev, 0, tone.frequency ? CW_DEBUG_EVENT_TONE_HIGH : CW_DEBUG_EVENT_TONE_LOW);
#endif

		/* This is a blocking write. */
		if (gen->sound_system == CW_AUDIO_NULL || gen->sound_system == CW_AUDIO_CONSOLE) {
			cw_assert (NULL != gen->write_tone_to_sound_device, "'gen->write_tone_to_sound_device' pointer is NULL");
			gen->write_tone_to_sound_device(gen, &tone);
		} else {
			if (gen->silencing_initialized) {
				/* Don't play a tone that has been just
				   dequeued. Instead prepare a tone that will
				   silence current source sink. Use current
				   tone ('tone' variable) as starting
				   point/basis for this silencing tone. */
				CW_TONE_COPY(&tone, &prev_tone);
				cw_gen_silencing_tone_calculate_samples_size_internal(gen, &tone);
			} else if (is_empty_tone) {
				/* No valid tone dequeued from tone
				   queue. 'tone' argument doesn't represent a
				   valid tone. We need samples to complete
				   filling buffer, but they have to be empty
				   samples. */
				cw_gen_empty_tone_calculate_samples_size_internal(gen, &tone);
			} else {
				/* Valid tone dequeued from tone queue and
				   nothing prohibits us from playing it (we
				   aren't in 'silencing' phase). Use the tone
				   to calculate samples in buffer. */
				cw_gen_tone_calculate_samples_size_internal(gen, &tone);
			}

			cw_gen_write_to_soundcard_internal(gen, &tone);
		}

		if (prev_tone.is_forever && tone.is_forever) {
			/*
			  Don't notify about dequeueing two consecutive
			  'forever' tones. For any listener this is still the
			  same tone.

			  TODO: make the check more precise. What if first
			  'forever' tone is silent and the next is
			  non-silent, or if they have two different
			  frequencies?
			*/
			; /* NOOP */
		} else {
#ifdef GENERATOR_CLIENT_THREAD
			fprintf(stderr, MSG_PREFIX "      sending signal on dequeue, target thread id = %ld\n", gen->library_client.thread_id);
#endif
			pthread_mutex_lock(&gen->tq->wait_mutex);
			pthread_cond_broadcast(&gen->tq->wait_var);
			pthread_mutex_unlock(&gen->tq->wait_mutex);
		}

#ifdef GENERATOR_CLIENT_THREAD
		/* Original implementation using signals. */
		/* This code has been disabled some time before 2017-01-19. */

		/*
		  When sending text from text input, the signal:
		   - allows client code to observe moment when state of tone
		     queue is "low/critical"; client code then can add more
		     characters to the queue; the observation is done using
		     cw_tq_wait_for_level_internal();

		   - allows client code to observe any dequeue event
		     by waiting for signal in
		     cw_tq_wait_for_end_of_current_tone_internal();
		*/
		pthread_kill(gen->library_client.thread_id, SIGALRM);
#endif

		/* Generator may be used by iambic keyer to measure periods
		   of time (durations of Mark and Space). This is achieved by
		   enqueueing Marks and Spaces by keyer in generator. A
		   soundcard playing samples is surprisingly good at
		   measuring time intervals.

		   At this point the generator has finished generating
		   a tone of specified duration. A duration of Mark or
		   Space has elapsed. Inform iambic keyer that the
		   tone it has enqueued has elapsed. The keyer may
		   want to change state of its internal state machine.

		   (Whether iambic keyer has enqueued any tones or
		   not, and whether it is waiting for the
		   notification, is a different story. We will let the
		   iambic keyer function called below to decide what
		   to do with the notification. If keyer is in idle
		   state, it will ignore the notification.)

		   Notice that this mechanism is needed only for
		   iambic keyer. Inner workings of straight key are
		   much more simple, the straight key doesn't need to
		   use generator as a timer. */
		if (CW_FAILURE == cw_key_ik_update_graph_state_internal(gen->key)) {
			/* just try again, once */
			usleep(1000);
			cw_key_ik_update_graph_state_internal(gen->key);
		}

		if (gen->silencing_initialized) {
			/* We are in silencing phase. A last tone (silencing
			   tone) has been played, and we shouldn't play
			   anything else. Discard tones remaining in
			   queue.

			   Remember that we are in silencing phase, which may
			   or may not mean that generator is being
			   stopped and deleted. */
			cw_tq_flush_internal(gen->tq);
			gen->silencing_initialized = false;
		}

#ifdef ENABLE_DEV_LIBCW_DEBUGGING
		cw_debug_ev (&cw_debug_object_ev, 0, tone.frequency ? CW_DEBUG_EVENT_TONE_LOW : CW_DEBUG_EVENT_TONE_HIGH);
#endif
		/* And finally, at the very end... */
		CW_TONE_COPY(&prev_tone, &tone);

	} /* while (gen->do_dequeue_and_generate) */

	cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_INFO,
		      MSG_PREFIX "EXIT: generator stopped (gen->do_dequeue_and_generate = %d)", gen->do_dequeue_and_generate);

	/* Some functions in main thread may be waiting for the last
	   notification from the generator thread to continue/finalize
	   their business. Let's send that notification right before
	   exiting. */

	/* This small delay before sending the notification turns out
	   to be helpful.

	   TODO: this is one of most mysterious comments in this code
	   base. What was I thinking? */
	cw_usleep_internal(CW_USECS_PER_SEC / 2);

	pthread_mutex_lock(&gen->tq->wait_mutex);
	/* There may be many listeners, so use broadcast(). */
	pthread_cond_broadcast(&gen->tq->wait_var);
	pthread_mutex_unlock(&gen->tq->wait_mutex);

#ifdef GENERATOR_CLIENT_THREAD
	/* Original implementation using signals. */
	/* This code has been disabled some time before 2017-01-19. */
	pthread_kill(gen->library_client.thread_id, SIGALRM);
#endif

	gen->thread.running = false;
	return NULL;
}




/**
   @brief Calculate a fragment of sine wave

   Calculate a fragment of sine wave, as many samples as can be fitted in
   generator buffer's subarea.

   The function calculates values of (gen->buffer_sub_stop - gen->buffer_sub_start + 1)
   samples and puts them into gen->buffer[], starting from
   gen->buffer[gen->buffer_sub_start].

   The function takes into account all state variables from gen,
   so initial phase of new fragment of sine wave in the buffer matches
   ending phase of a sine wave generated in previous call.

   @internal
   @reviewed 2020-08-04
   @endinternal

   @param[in] gen generator that generates sine wave
   @param[in,out] tone specification of samples that should be calculated

   @return number of calculated samples
*/
int cw_gen_calculate_sine_wave_internal(cw_gen_t * gen, cw_tone_t * tone)
{
	assert (gen->buffer_sub_stop <= gen->buffer_n_samples);

	/* We need two separate iterators to correctly generate sine wave:
	    -- i -- for iterating through output buffer (generator
	            buffer's subarea), it can travel between buffer
	            cells delimited by start and stop (inclusive);
	    -- t -- for calculating phase of a sine wave; 't' always has to
	            start from zero for every calculated subarea (i.e. for
		    every call of this function);

	  Initial/starting phase of generated fragment is always retained
	  in gen->phase_offset, it is the only "memory" of previously
	  calculated fragment of sine wave (to be precise: it stores phase
	  of last sample in previously calculated fragment).
	  Therefore iterator used to calculate phase of sine wave can't have
	  the memory too. Therefore it has to always start from zero for
	  every new fragment of sine wave. Therefore a separate t. */

	float phase = 0.0F;
	int t = 0;

	for (int i = gen->buffer_sub_start; i <= gen->buffer_sub_stop; i++) {
		phase = (2.0F * CW_PI
			 * (float) (tone->frequency * t)
			 / (float) gen->sample_rate)
			+ gen->phase_offset;
		const int amplitude = cw_gen_calculate_sample_amplitude_internal(gen, tone);

		gen->buffer[i] = ((float) amplitude) * sinf(phase);

		tone->sample_iterator++;

		t++;
	}

	phase = (2.0F * CW_PI
		 * (float) (tone->frequency * t)
		 / (float) gen->sample_rate)
		+ gen->phase_offset;

	/* "phase" is now phase of the first sample in next fragment to be
	   calculated.
	   However, for long fragments this can be a large value, well
	   beyond <0; 2*Pi) range.
	   The value of phase may further accumulate in different
	   calculations, and at some point it may overflow. This would
	   result in an audible click.

	   Let's bring back the phase from beyond <0; 2*Pi) range into the
	   <0; 2*Pi) range, in other words lets "normalize" it. Or, in yet
	   other words, lets apply modulo operation to the phase.

	   The normalized phase will be used as a phase offset for next
	   fragment (during next function call). It will be added phase of
	   every sample calculated in next function call. */

	/* TODO: check if n_periods can be a float. We could avoid the casts. */
	const int n_periods = (int) floorf(phase / (2.0F * CW_PI));
	gen->phase_offset = phase - (float) n_periods * 2.0F * CW_PI;

	return t;
}




/**
   @brief Calculate value of a single sample of sine wave

   This function calculates an amplitude (a value) of a single sample
   in sine wave PCM data.

   Actually "calculation" is a bit too big word. The function just
   makes a decision which of precalculated values to return. There are
   no complicated arithmetical calculations being made each time the
   function is called, so the execution time should be pretty small.

   The precalcuated values depend on some factors, so the values
   should be re-calculated each time these factors change. See
   cw_gen_set_tone_slope() for list of these factors.

   A generator contains some of information needed to get an amplitude of
   every sample in a sine wave - this is why this function needs @p gen.  If
   tone's slopes are non-rectangular, the duration of slopes is defined in
   generator. If a tone is non-silent, the volume is also defined in
   generator.

   However, decision tree for getting the amplitude also depends on some
   parameters that are strictly bound to tone, such as what is the shape of
   slopes for a given tone - this is why we have @p tone.  The @p tone also
   stores iterator of samples - this is how we know for which sample in a
   tone to calculate the amplitude.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen generator used to generate a sine wave
   @param[in] tone tone being generated

   @return value of a sample of sine wave, a non-negative number
*/
int cw_gen_calculate_sample_amplitude_internal(cw_gen_t * gen, const cw_tone_t * tone)
{
#if 0   /* Blunt algorithm for calculating amplitude. For debug purposes only. */

	return tone->frequency ? gen->volume_abs : 0;

#else

	if (tone->frequency <= 0) {
		return 0;
	}


	float amplitude = 0.0F;

	/* Every tone, regardless of slope mode (CW_SLOPE_MODE_*), has
	   three components. It has rising slope + plateau + falling
	   slope.

	   There can be four variants of rising and falling slope
	   length, just as there are four CW_SLOPE_MODE_* values.

	   There can be also tones with zero-length plateau, and there
	   can be also tones with zero-length slopes. */

	if (tone->sample_iterator < tone->rising_slope_n_samples) {
		/* Beginning of tone, rising slope. */
		const int i = tone->sample_iterator;
		amplitude = gen->tone_slope.amplitudes[i];
		assert (amplitude >= 0);

	} else if (tone->sample_iterator >= tone->rising_slope_n_samples
		   && tone->sample_iterator < tone->n_samples - tone->falling_slope_n_samples) {

		/* Middle of tone, plateau, constant amplitude. */
		amplitude = (float) gen->volume_abs;
		assert (amplitude >= 0);

	} else if (tone->sample_iterator >= tone->n_samples - tone->falling_slope_n_samples) {
		/* Falling slope. */
		const cw_sample_iter_t i = tone->n_samples - tone->sample_iterator - 1;
		assert (i >= 0);
		amplitude = gen->tone_slope.amplitudes[i];
		assert (amplitude >= 0);

	} else {
		cw_assert (0, MSG_PREFIX "->sample_iterator out of bounds:\n"
			   "tone->sample_iterator: %ld\n"
			   "tone->n_samples: %"PRId64"\n"
			   "tone->rising_slope_n_samples: %d\n"
			   "tone->falling_slope_n_samples: %d\n",
			   tone->sample_iterator,
			   tone->n_samples,
			   tone->rising_slope_n_samples,
			   tone->falling_slope_n_samples);
	}

	assert (amplitude >= 0.0F);
	return (int) amplitude;
#endif
}




/**
   @brief Set parameters describing slopes of tones generated by generator

   Most of variables related to slope of tones is in @p tone, but there are
   still some variables that are generator-specific, as they are common for
   all tones.  This function sets two of these generator-specific variables.

   A: If you pass to function conflicting values of @p slope_shape and
   @p slope_duration, the function will return CW_FAILURE. These
   conflicting values are rectangular slope shape and larger than zero
   slope length. You just can't have rectangular slopes that have
   non-zero length.

   B: If you pass to function '\-1' as value of both @p slope_shape and
   @p slope_duration, the function won't change any of the related two
   generator's parameters.

   C1: If you pass to function '\-1' as value of either @p slope_shape or
   @p slope_duration, the function will attempt to set only this generator's
   parameter that is different than '\-1'.

   C2: However, if selected slope shape is rectangular, function will
   set generator's slope length to zero, even if value of \p
   slope_duration is '\-1'.

   D: Notice that the function allows non-rectangular slope shape with
   zero length of the slopes. The slopes will be non-rectangular, but
   just unusually short.

   @internal TODO: Seriously, these rules (A-D) for setting a slope are too complicated. Simplify them. Accept only a small subset of valid/sane values. Perhaps split the function into two separate functions: for setting slope shape and slope duration. @endinternal

   The function should be called every time one of following
   parameters change:

   @li shape of slope,
   @li duration of slope,
   @li generator's sample rate,
   @li generator's volume.

   There are four supported shapes of slopes:
   @li linear (the only one supported by libcw until version 4.1.1),
   @li raised cosine (supposedly the most desired shape),
   @li sine,
   @li rectangular.

   Use CW_TONE_SLOPE_SHAPE_* symbolic names as values of @p slope_shape.

   FIXME: first argument of this public function is gen, but no
   function provides access to generator variable.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen generator for which to set tone slope parameters
   @param[in] slope_shape shape of slope: linear, raised cosine, sine, rectangular
   @param[in] slope_duration duration of slope [microseconds]

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
int cw_generator_set_tone_slope(cw_gen_t * gen, int slope_shape, int slope_duration)
{
	return cw_gen_set_tone_slope(gen, slope_shape, slope_duration);
}




/**
   See comment for cw_generator_set_tone_slope()

   @internal
   @reviewed 2020-08-05
   @endinternal
*/
cw_ret_t cw_gen_set_tone_slope(cw_gen_t * gen, int slope_shape, int slope_duration)
{
	assert (gen);

	/* Handle conflicting values of arguments. */
	if (slope_shape == CW_TONE_SLOPE_SHAPE_RECTANGULAR
	    && slope_duration > 0) {

		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR,
			      MSG_PREFIX "requested a rectangular slope shape, but also requested slope duration > 0");

		return CW_FAILURE;
	}

	/* Assign new values from arguments. */
	if (slope_shape != -1) {
		gen->tone_slope.shape = slope_shape;
	}
	if (slope_duration != -1) {
		gen->tone_slope.duration = slope_duration;
	}


	/* Override of slope duration. */
	if (slope_shape == CW_TONE_SLOPE_SHAPE_RECTANGULAR) {
		/* TODO: what's going on here? Why do we set this to zero? */
		gen->tone_slope.duration = 0;
	}


	int slope_n_samples = ((gen->sample_rate / 100) * gen->tone_slope.duration) / 10000;
	cw_assert (slope_n_samples >= 0, MSG_PREFIX "negative slope_n_samples: %d", slope_n_samples);


	/* Reallocate the table of slope amplitudes only when necessary.

	   In practice the function will be called foremost when user changes
	   volume of tone, and then the function may be called several times
	   in a row if volume is changed in steps. In such situation the size
	   of amplitudes table doesn't change.

	   TODO: do we really need to change type/duration of slopes when
	   volume changes? Perhaps a call to
	   cw_gen_recalculate_slope_amplitudes_internal() would be enough? */

	if (gen->tone_slope.n_amplitudes != slope_n_samples) {

		 /* Remember that slope_n_samples may be zero. In that
		    case realloc() would equal to free(). We don't
		    want to have NULL ->amplitudes, so don't modify
		    ->amplitudes for zero-duration slopes.  Since with
		    zero-duration slopes we won't be referring to
		    ->amplitudes[], it is ok that the table will not
		    be up-to-date. */

		if (slope_n_samples > 0) {
			gen->tone_slope.amplitudes = realloc(gen->tone_slope.amplitudes, sizeof(float) * slope_n_samples);
			if (!gen->tone_slope.amplitudes) {
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR,
					      MSG_PREFIX "failed to realloc() table of slope amplitudes");
				return CW_FAILURE;
			}
		}

		gen->tone_slope.n_amplitudes = slope_n_samples;
	}

	cw_gen_recalculate_slope_amplitudes_internal(gen);

	return CW_SUCCESS;
}




/**
   @brief Recalculate amplitudes of PCM samples that form tone's slopes

   @internal
   @reviewed 2020-08-05
   @endinternal

   TODO: consider writing unit test code for the function.

   @param[in] gen generator
*/
void cw_gen_recalculate_slope_amplitudes_internal(cw_gen_t * gen)
{
	/* The values in amplitudes[] change from zero to max (at
	   least for any sane slope shape), so naturally they can be
	   used in forming rising slope. However they can be used in
	   forming falling slope as well - just iterate the table from
	   end to beginning. */
	for (int i = 0; i < gen->tone_slope.n_amplitudes; i++) {

		if (gen->tone_slope.shape == CW_TONE_SLOPE_SHAPE_LINEAR) {
			gen->tone_slope.amplitudes[i] = (float) (i * gen->volume_abs) / (float) gen->tone_slope.n_amplitudes;

		} else if (gen->tone_slope.shape == CW_TONE_SLOPE_SHAPE_SINE) {
			const float radian = (float) i * (CW_PI / 2.0F) / (float) gen->tone_slope.n_amplitudes;
			const float y = sinf(radian);
			gen->tone_slope.amplitudes[i] = y * (float) gen->volume_abs;

		} else if (gen->tone_slope.shape == CW_TONE_SLOPE_SHAPE_RAISED_COSINE) {
			const float radian = (float) i * CW_PI / (float) gen->tone_slope.n_amplitudes;
			const float y = (1 - ((1 + cosf(radian)) / 2));
			gen->tone_slope.amplitudes[i] = y * (float) gen->volume_abs;

		} else if (gen->tone_slope.shape == CW_TONE_SLOPE_SHAPE_RECTANGULAR) {
			/* CW_TONE_SLOPE_SHAPE_RECTANGULAR is covered
			   before entering this "for" loop. */
			/* TODO: to avoid treating
			   CW_TONE_SLOPE_SHAPE_RECTANGULAR as special case,
			   add the calculation here. */
			cw_assert (0, MSG_PREFIX "we shouldn't be here, calculating rectangular slopes");

		} else {
			cw_assert (0, MSG_PREFIX "unsupported slope shape %d", gen->tone_slope.shape);
		}
	}

	return;
}




/**
   @brief Write tone to soundcard

   @internal
   @reviewed 2020-10-13
   @endinternal

   @param[in] gen
   @param[in] tone tone dequeued from queue (if dequeueing was successful); must always be non-NULL

   @return 0
*/
int cw_gen_write_to_soundcard_internal(cw_gen_t * gen, cw_tone_t * tone)
{
	cw_assert (NULL != tone, MSG_PREFIX "'tone' argument should always be non-NULL");

	/* Total number of samples to write in a loop below. */
	int64_t samples_to_write = tone->n_samples;

#define LIBCW_WRITE_LOOP_DEBUG_LEVEL 0
#if LIBCW_WRITE_LOOP_DEBUG_LEVEL > 0
	/* Debug code. */
	int n_loops = 0;
	const float n_loops_expected = floorf(1.0 * samples_to_write / gen->buffer_n_samples); /* In reality number of loops executed is sometimes n_loops_expected, but mostly it's n_loops_expected+1. */
	fprintf(stderr, MSG_PREFIX "entering loop (~%.1f iterations expected), tone->frequency = %d, buffer->n_samples = %d, samples_to_write = %"PRId64"\n",
		(double) n_loops_expected, tone->frequency, gen->buffer_n_samples, samples_to_write);
#endif

	// cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_DEBUG, MSG_PREFIX "%lld samples, %d us, %d Hz", tone->n_samples, tone->duration, gen->frequency);
	while (samples_to_write > 0) {

		const int64_t free_space = gen->buffer_n_samples - gen->buffer_sub_start;
		if (samples_to_write > free_space) {
			/* There will be some tone samples left for
			   next iteration of this loop.  But buffer in
			   this iteration will be ready to be pushed
			   to sound sink. */
			gen->buffer_sub_stop = gen->buffer_n_samples - 1;
		} else if (samples_to_write == free_space) {
			/* How nice, end of tone samples aligns with
			   end of buffer (last sample of tone will be
			   placed in last cell of buffer).

			   But the result is the same - a full buffer
			   ready to be pushed to sound sink. */
			gen->buffer_sub_stop = gen->buffer_n_samples - 1;
		} else {
			/* There will be too few samples to fill a
			   buffer. We can't send an under-filled buffer to
			   sound sink. We will have to get more
			   samples to fill the buffer completely. */
			gen->buffer_sub_stop = gen->buffer_sub_start + samples_to_write - 1;
		}

		/* How many samples of sound buffer's subarea will be
		   calculated in a given cycle of "calculate sine
		   wave" code, i.e. in current iteration of the 'while' loop? */
		const int buffer_sub_n_samples = gen->buffer_sub_stop - gen->buffer_sub_start + 1;


#if LIBCW_WRITE_LOOP_DEBUG_LEVEL > 0
		/* Debug code. */
		++n_loops;
#if LIBCW_WRITE_LOOP_DEBUG_LEVEL > 1
		/* Debug code. */
		fprintf(stderr, MSG_PREFIX "       loop #%d, buffer_sub_n_samples = %d\n", n_loops, buffer_sub_n_samples);
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_DEBUG,
			      MSG_PREFIX
			      "sub start: %d, sub stop: %d, sub size: %d / %"PRIu64,
			      gen->buffer_sub_start, gen->buffer_sub_stop, buffer_sub_n_samples, samples_to_write);
#endif
#endif


		const int calculated = cw_gen_calculate_sine_wave_internal(gen, tone);
		cw_assert (calculated == buffer_sub_n_samples, MSG_PREFIX "calculated wrong number of samples: %d != %d", calculated, buffer_sub_n_samples);

		if (gen->buffer_sub_stop == gen->buffer_n_samples - 1) {

			/* We have a buffer full of samples. The
			   buffer is ready to be pushed to sound
			   sink. */
			gen->write_buffer_to_sound_device(gen);
#ifdef ENABLE_DEV_PCM_SAMPLES_FILE
			cw_dev_debug_raw_sink_write_internal(gen);
#endif
			gen->buffer_sub_start = 0;
			gen->buffer_sub_stop = 0;
		} else {
			/* #needmoresamples
			   There is still some space left in the
			   buffer, go fetch new tone from tone
			   queue. */

			gen->buffer_sub_start = gen->buffer_sub_stop + 1;

			cw_assert (gen->buffer_sub_start <= gen->buffer_n_samples - 1,
				   MSG_PREFIX "sub start out of range: sub start = %d, buffer n samples = %d",
				   gen->buffer_sub_start, gen->buffer_n_samples);
		}

		samples_to_write -= buffer_sub_n_samples;

#if LIBCW_WRITE_LOOP_DEBUG_LEVEL > 0
		/* Debug code. */
		if (samples_to_write < 0) {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_DEBUG, MSG_PREFIX "samples left = %"PRId64, samples_to_write);
		}
#endif

	} /* while (samples_to_write > 0) { */

#if LIBCW_WRITE_LOOP_DEBUG_LEVEL > 0
	/* Debug code. */
	fprintf(stderr, MSG_PREFIX "left loop, %d iterations executed from %.1f iterations planned, samples left = %d\n",
		n_loops, (double) n_loops_expected, (int) samples_to_write);
#endif

	return 0;
}




/**
   @brief Construct empty tone with correct/needed values of samples count

   The function sets values tone->..._n_samples fields of empty @p
   tone based on information from @p gen (i.e. looking on how many
   samples of silence need to be "created").  The sample count values
   are set in a way that allows filling remainder of generator's
   buffer with silence.

   After this point tone duration should not be used - it's the samples count that is correct.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen
   @param[in] tone tone for which to calculate samples count.
*/
void cw_gen_empty_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone)
{
	/* All tones have been already dequeued from tone queue.

	   @p tone does not represent a valid tone to generate. At
	   first sight there is no need to write anything to
	   soundcard. But...

	   It may happen that during previous call to the function
	   there were too few samples in a tone to completely fill a
	   buffer (see #needmoresamples tag below).

	   We need to fill the buffer until it is full and ready to be
	   sent to sound sink.

	   Since there are no new tones for which we could generate
	   samples, we need to generate silence samples.

	   Padding the buffer with silence seems to be a good idea (it
	   will work regardless of value (Mark/Space) of last valid
	   tone). We just need to know how many samples of the silence
	   to produce.

	   Number of these samples will be stored in
	   samples_to_write. */

	/* We don't have a valid tone, so let's construct a fake one
	   for purposes of padding. */

	/* Required length of padding space is from end of last buffer
	   subarea to end of buffer. */
	tone->n_samples = gen->buffer_n_samples - (gen->buffer_sub_stop + 1);;

	tone->duration = 0;    /* This value matters no more, because now we only deal with samples. */
	tone->frequency = 0;   /* This fake tone is a piece of silence. */

	/* The silence tone used for padding doesn't require any
	   slopes. A slope falling to silence has been already
	   provided by last non-fake and non-silent tone. */
	tone->slope_mode = CW_SLOPE_MODE_NO_SLOPES;
	tone->rising_slope_n_samples = 0;
	tone->falling_slope_n_samples = 0;

	/* This is part of initialization of tone. Zero samples from the tone
	   have been calculated and put into generator's buffer. */
	tone->sample_iterator = 0;

	//fprintf(stderr, "++++ count of samples in empty tone '%c' = %d\n", tone->debug_id, tone->n_samples);

	return;
}




/**
   @brief Calculate tone suitable for silencing a generator

   Calculate a @p tone variable that, when passed to generator's write()
   function, will silence a generator.

   @internal
   @reviewed 2020-10-13
   @endinternal

   @param[in] gen
   @param[in] tone tone for which to calculate samples
*/
void cw_gen_silencing_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone)
{
	/* Make sure that we fill a buffer to the end (i.e. calculate as many
	   samples as there are from current position in the buffer to the
	   end of the buffer). Otherwise a buffer underrun may occur. */
	tone->n_samples = gen->buffer_n_samples - (gen->buffer_sub_stop + 1);

	/* Also make sure that a tone is long enough for it to be heard
	   (otherwise it may be perceived as click). */
	tone->n_samples += (5 * gen->buffer_n_samples);

	tone->duration = 0;    /* This value matters no more, because now we only deal with samples. */

	/* Length of a single slope. */
	cw_sample_iter_t slope_n_samples = gen->sample_rate / 100;
	slope_n_samples *= gen->tone_slope.duration;
	slope_n_samples /= 10000;

	switch (tone->slope_mode) {
	case CW_SLOPE_MODE_NO_SLOPES:
	case CW_SLOPE_MODE_RISING_SLOPE:
		/* Previous tone has ended with full level. Let the silencing
		   tone fall from the full level to zero. */
		tone->slope_mode = CW_SLOPE_MODE_FALLING_SLOPE;
		break;
	case CW_SLOPE_MODE_FALLING_SLOPE:
	case CW_SLOPE_MODE_STANDARD_SLOPES:
		/* Level of previous slope has already fallen to zero. Keep
		   the level at zero. */
	default:
		tone->slope_mode = CW_SLOPE_MODE_NO_SLOPES;
		tone->frequency = 0;
		break;
	}
	tone->rising_slope_n_samples = 0;
	tone->falling_slope_n_samples = slope_n_samples;

	/* This is part of initialization of tone. Zero samples from the tone
	   have been calculated and put into generator's buffer. */
	tone->sample_iterator = 0;

	//fprintf(stderr, "++++ count of samples in silencing tone '%c' = %d\n", tone->debug_id, tone->n_samples);

	return;
}




/**
   @brief Recalculate non-empty tone parameters from microseconds into samples

   The function sets tone->..._n_samples fields of non-empty @p tone
   based on other information from @p tone and from @p gen.

   After this point tone duration should not be used - it's the samples count that is correct.

   @internal
   @reviewed 2020-08-05
   @endinternal

   @param[in] gen
   @param[in] tone tone for which to calculate samples count.
*/
void cw_gen_tone_calculate_samples_size_internal(const cw_gen_t * gen, cw_tone_t * tone)
{
	/* 100 * 10000 = 1.000.000 usecs per second. */
	tone->n_samples = gen->sample_rate / 100;
	tone->n_samples *= tone->duration;
	tone->n_samples /= 10000;

	//fprintf(stderr, MSG_PREFIX "length of regular tone = %d [samples]\n", tone->n_samples);

	/* Length of a single slope (rising or falling). */
	cw_sample_iter_t slope_n_samples = gen->sample_rate / 100;
	slope_n_samples *= gen->tone_slope.duration;
	slope_n_samples /= 10000;

	if (tone->slope_mode == CW_SLOPE_MODE_RISING_SLOPE) {
		tone->rising_slope_n_samples = slope_n_samples;
		tone->falling_slope_n_samples = 0;

	} else if (tone->slope_mode == CW_SLOPE_MODE_FALLING_SLOPE) {
		tone->rising_slope_n_samples = 0;
		tone->falling_slope_n_samples = slope_n_samples;

	} else if (tone->slope_mode == CW_SLOPE_MODE_STANDARD_SLOPES) {
		tone->rising_slope_n_samples = slope_n_samples;
		tone->falling_slope_n_samples = slope_n_samples;

	} else if (tone->slope_mode == CW_SLOPE_MODE_NO_SLOPES) {
		tone->rising_slope_n_samples = 0;
		tone->falling_slope_n_samples = 0;

	} else {
		cw_assert (0, MSG_PREFIX "unknown tone slope mode %d", tone->slope_mode);
	}

	/* This is part of initialization of tone. Zero samples from the tone
	   have been calculated and put into generator's buffer. */
	tone->sample_iterator = 0;

#if 0
	/* Debug code. */
	if (tone->is_forever) {
		fprintf(stderr, "++++ count of samples in forever tone '%c' = %d\n", tone->debug_id, tone->n_samples);
	} else {
		fprintf(stderr, "++++ count of samples in regular tone '%c' = %d\n", tone->debug_id, tone->n_samples);
	}
#endif

	return;
}




cw_ret_t cw_gen_set_speed(cw_gen_t * gen, int new_value)
{
	if (new_value < CW_SPEED_MIN || new_value > CW_SPEED_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (new_value != gen->send_speed) {
		gen->send_speed = new_value;

		/* Changes of send speed require resynchronization. */
		gen->parameters_in_sync = false;
		cw_gen_sync_parameters_internal(gen);
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_set_frequency(cw_gen_t * gen, int new_value)
{
	if (new_value < CW_FREQUENCY_MIN || new_value > CW_FREQUENCY_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	} else {
		gen->frequency = new_value;
		return CW_SUCCESS;
	}
}




cw_ret_t cw_gen_set_volume(cw_gen_t * gen, int new_value)
{
	if (new_value < CW_VOLUME_MIN || new_value > CW_VOLUME_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	} else {
		gen->volume_percent = new_value;
		gen->volume_abs = (gen->volume_percent * CW_AUDIO_VOLUME_RANGE) / 100;

		cw_gen_set_tone_slope(gen, -1, -1);

		return CW_SUCCESS;
	}
}




cw_ret_t cw_gen_set_gap(cw_gen_t * gen, int new_value)
{
	if (new_value < CW_GAP_MIN || new_value > CW_GAP_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (new_value != gen->gap) {
		gen->gap = new_value;
		/* Changes of gap require resynchronization. */
		gen->parameters_in_sync = false;
		cw_gen_sync_parameters_internal(gen);
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_set_weighting(cw_gen_t * gen, int new_value)
{
	if (new_value < CW_WEIGHTING_MIN || new_value > CW_WEIGHTING_MAX) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	if (new_value != gen->weighting) {
		gen->weighting = new_value;

		/* Changes of weighting require resynchronization. */
		gen->parameters_in_sync = false;
		cw_gen_sync_parameters_internal(gen);
	}

	return CW_SUCCESS;
}




int cw_gen_get_speed(const cw_gen_t * gen)
{
	return gen->send_speed;
}




int cw_gen_get_frequency(const cw_gen_t * gen)
{
	return gen->frequency;
}




int cw_gen_get_volume(const cw_gen_t * gen)
{
	return gen->volume_percent;
}




int cw_gen_get_gap(const cw_gen_t * gen)
{
	return gen->gap;
}




int cw_gen_get_weighting(const cw_gen_t * gen)
{
	return gen->weighting;
}




void cw_gen_get_timing_parameters_internal(cw_gen_t * gen,
					   int * dot_duration,
					   int * dash_duration,
					   int * ims_duration,
					   int * ics_duration,
					   int * iws_duration,
					   int * additional_space_duration,
					   int * adjustment_space_duration)
{
	cw_gen_sync_parameters_internal(gen);

	if (dot_duration)  { *dot_duration  = gen->durations.dot_duration; }
	if (dash_duration) { *dash_duration = gen->durations.dash_duration; }
	if (ims_duration)  { *ims_duration  = gen->durations.ims_duration; }
	if (ics_duration)  { *ics_duration  = gen->durations.ics_duration; }
	if (iws_duration)  { *iws_duration  = gen->durations.iws_duration; }

	if (additional_space_duration) { *additional_space_duration = gen->durations.additional_space_duration; }
	if (adjustment_space_duration) { *adjustment_space_duration = gen->durations.adjustment_space_duration; }
}




void cw_gen_get_durations_internal(cw_gen_t * gen, cw_gen_durations_t * durations)
{
	/*
	  This function shall always and forever be a wrapper around
	  cw_gen_get_timing_parameters_internal(). Let the wrapped function do
	  some steps necessary before returning the parameters.
	*/

	cw_gen_get_timing_parameters_internal
		(gen,
		 &durations->dot_duration,
		 &durations->dash_duration,
		 &durations->ims_duration,
		 &durations->ics_duration,
		 &durations->iws_duration,
		 &durations->additional_space_duration,
		 &durations->adjustment_space_duration);
}




/**
   @brief Enqueue a mark (Dot or Dash)

   Low level primitive to enqueue a tone for mark of the given type, followed
   by the standard inter-mark-space.

   Function sets errno to EINVAL if an argument is invalid, and
   returns CW_FAILURE.

   Function also returns CW_FAILURE if adding the element to queue of
   tones failed.

   @reviewedon 2023-08-26

   @param[in] gen generator to be used to enqueue a mark and inter-mark-space
   @param[in] mark mark to send: Dot (CW_DOT_REPRESENTATION) or Dash (CW_DASH_REPRESENTATION)
   @param[in] is_first is it a first mark in a character?

   @return CW_FAILURE on failure
   @return CW_SUCCESS on success
*/
cw_ret_t cw_gen_enqueue_mark_internal(cw_gen_t * gen, char mark, bool is_first)
{
	cw_ret_t cwret = CW_FAILURE;

	/* Synchronize low-level timings if required. */
	cw_gen_sync_parameters_internal(gen);
	/* TODO: do we need to synchronize here receiver as well? */

	/* Send either a dot or a dash mark, depending on representation. */
	if (mark == CW_DOT_REPRESENTATION) {
		cw_tone_t tone;
		CW_TONE_INIT(&tone, gen->frequency, gen->durations.dot_duration, CW_SLOPE_MODE_STANDARD_SLOPES);
		tone.is_first = is_first;
		cwret = cw_tq_enqueue_internal(gen->tq, &tone);
		/* Enqueueing a mark means resetting of spaces counter. */
		gen->space_units_count = 0;
	} else if (mark == CW_DASH_REPRESENTATION) {
		cw_tone_t tone;
		CW_TONE_INIT(&tone, gen->frequency, gen->durations.dash_duration, CW_SLOPE_MODE_STANDARD_SLOPES);
		tone.is_first = is_first;
		cwret = cw_tq_enqueue_internal(gen->tq, &tone);
		/* Enqueueing a mark means resetting of spaces counter. */
		gen->space_units_count = 0;
	} else {
		errno = EINVAL;
		cwret = CW_FAILURE;
		/* Reset on error. */
		gen->space_units_count = 0;
	}

	if (CW_SUCCESS != cwret) {
		return CW_FAILURE;
	}

	/* Send the inter-mark-space. */
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, gen->durations.ims_duration, CW_SLOPE_MODE_NO_SLOPES);
	cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	/* Enqueueing an ims must be recorded in space units counter. */
	gen->space_units_count = UNITS_PER_IMS;
	return cwret;
}




/**
   @brief Enqueue inter-character-space

   The function enqueues enough space to form 3-Unit inter-character-space.

   The function can be called even when inter-mark-space has already been
   enqueued. In such situation standard inter-mark-space (one Unit) will be
   followed by just two Units to form a full standard inter-character-space
   (three Units).

   Inter-character adjustment space is added at the end.

   @reviewedon 2023-08-06

   @param[in] gen generator in which to enqueue the space

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_ics_internal(cw_gen_t * gen)
{
	/* Synchronize low-level timing parameters. */
	cw_gen_sync_parameters_internal(gen);

	/* The ics enqueued here should be shorter by already enqueued/played
	   spaces. Calculate the duration of shorter ics depending on what kind
	   of spaces were already enqueued before. */
	int ics_duration = 0;
	switch (gen->space_units_count) {
	case 0:
		/* It's possible that dot or dash was enqueued without ims with
		   cw_gen_enqueue_ik_symbol_no_ims_internal(), or maybe the count was
		   reset on error, so enqueue ics with its full duration. */
		ics_duration = gen->durations.ics_duration;
		break;
	case UNITS_PER_IMS:
		/* This ics is appended after already enqueued ims. Duration of the
		   tone that we enqueue here should be shorter by ims. The ims and
		   current shortened tone will together form 3-units ics. */
		ics_duration = gen->durations.ics_duration - gen->durations.ims_duration;
		break;
	case UNITS_PER_ICS:
	case UNITS_PER_IWS:
		/* libcw API provides functions for enqueueing ics or iws, so it's
		   possible that application will enqueue multiple ics spaces or mix
		   of ics and iws. So enqueue this ics in its full duration. */
		ics_duration = gen->durations.ics_duration;
		break;
	default:
		cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_ERROR,
		              MSG_PREFIX "Unexpected count of space units in 'enqueue ics': %d", gen->space_units_count);
		ics_duration = gen->durations.ics_duration;
		break;
	}

	/* Failsafe. */
	if (ics_duration < 0) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_ERROR,
		              MSG_PREFIX "Negative value of ics duration: %d", ics_duration);
		ics_duration = gen->durations.ics_duration;
	}

	/* Enqueue ics with calculated duration, plus any additional inter-character gap. */
	cw_tone_t tone;
	CW_TONE_INIT(&tone, 0, ics_duration + gen->durations.additional_space_duration, CW_SLOPE_MODE_NO_SLOPES);
	const cw_ret_t cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	gen->space_units_count = UNITS_PER_ICS;
	return cwret;
}




/**
   @brief Enqueue space character (' ') in generator, to be sent using Morse code

   The function should be used to enqueue a regular ' ' character.

   The function enqueues space of length 5 Units. The function is intended to
   be used after inter-mark-space and inter-character-space have already been
   enqueued.

   In such situation standard inter-mark-space (one Unit) and
   inter-character-space (two Units) and regular space (five units) form a
   full standard inter-word-space (seven Units).

   TODO: review this description again. This function alone doesn't send
   space character, it sends a part of what can be seen as inter-word-space.

   Inter-word adjustment space is added at the end.

   @reviewedon 2023-08-06

   @param[in] gen generator in which to enqueue the space

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_iws_internal(cw_gen_t * gen)
{
	/* Synchronize low-level timing parameters. */
	cw_gen_sync_parameters_internal(gen);

	/* The iws enqueued here should be shorter by already enqueued/played
	   spaces. Calculate the duration of shorter iws depending on what kind
	   of spaces were already enqueued before. */
	int iws_duration = 0;
	switch (gen->space_units_count) {
	case 0:
		/* We may get here when we only begin to play some string, or when
		   some reset of space_units_count was done, or when previous
		   element was an iws. This iws should have full duration. */
		iws_duration = gen->durations.iws_duration;
		break;
	case UNITS_PER_IMS:
		/* This iws is appended after already enqueued ims (e.g. at the end
		   of a word, when ' ' space character is played). Duration of the
		   tone that we enqueue here should be shorter by ims. The ims and
		   current shortened tone will together form 7-unit iws. */
		iws_duration = gen->durations.iws_duration - gen->durations.ims_duration;
		break;
	case UNITS_PER_ICS:
		/* This iws is appended after already enqueued ics. Duration of the
		   tone that we enqueue here should be shorter by ics. The ics and
		   current shortened tone will together form 7-unit iws. */
		iws_duration = gen->durations.iws_duration - gen->durations.ics_duration;
		break;
	case UNITS_PER_IWS:
		/* This is probably a situation when application wants to enqueue two
		   or more ' ' characters. Enqueue full duration of inter-word-space. */
		iws_duration = gen->durations.iws_duration;
		break;
	default:
		cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_ERROR,
		              MSG_PREFIX "Unexpected count of space units in 'enqueue iws': %d", gen->space_units_count);
		iws_duration = gen->durations.iws_duration;
		break;
	};

	/* Failsafe. */
	if (iws_duration < 0) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_ERROR,
		              MSG_PREFIX "Negative value of iws duration: %d", iws_duration);
		iws_duration = gen->durations.iws_duration;
	}

	/* Send silence for the word delay period, plus any adjustment
	   that may be needed at end of word. Make it in two tones,
	   and here is why.

	   Let's say that 'tone queue low watermark' is one element
	   (i.e. one tone).

	   In order for tone queue to recognize that a 'low tone
	   queue' callback needs to be called, the level in tq needs
	   to drop from 2 to 1.

	   Almost every queued character guarantees that there will be
	   at least two tones, e.g for 'E' it is dash + following
	   space. But what about a ' ' character?

	   If we enqueue ' ' character as single tone, there is only one
	   tone in tone queue, and the tone queue manager can't
	   recognize when the level drops from 2 to 1 (and thus the
	   'low level' callback won't be called).

	   If we enqueue ' ' character as two separate tones (as we do
	   this in this function), the tone queue manager can
	   recognize level dropping from 2 to 1. Then the passing of
	   critical level can be noticed, and "low level" callback can
	   be called.

	   BUT: Sometimes the first tone is dequeued before/during the
	   second one is enqueued, and we can't recognize 2->1 event.

	   So, to be super-sure that there is a recognizable event of
	   passing tone queue level from 2 to 1, we split the inter-word-space
	   into N parts and enqueue them. This way we have N + 1
	   tones per space, and client applications that rely on low
	   level threshold == 1 can correctly work when enqueueing
	   spaces.

	   At 60 wpm duration of gen->iws_duration is 100000 [us], so
	   it's large enough to safely divide it by small integer
	   value. */

	int enqueued = 0;
	cw_tone_t tone;
#if 0
	/* This section is incorrect. Enable this section only for
	   tests.  This section "implements" a bug that was present in
	   libcw until version 6.4.1 and that is now tested by
	   src/libcw/tests/libcw_test_tq_short_space.c */
	const int n = 1; /* No division. Old situation causing an error in
		      client applications. */
#else
	const int n = 2; /* "small integer value" - used to have more tones per inter-word-space. */
#endif
	CW_TONE_INIT(&tone, 0, iws_duration / n, CW_SLOPE_MODE_NO_SLOPES);
	for (int i = 0; i < n; i++) {
		if (CW_SUCCESS != cw_tq_enqueue_internal(gen->tq, &tone)) {
			/* Reset on error. */
			gen->space_units_count = 0;
			return CW_FAILURE;
		}
		enqueued++;
	}

	/* TODO acerion 2023.06.13: Don't enqueue the adjustment space as
	   separate tone. Add value of gen->adjustment_space_duration to
	   'duration' and enqueue resulting duration above. See how
	   'additional_space_duration' is added to ics duration in
	   cw_gen_enqueue_ics_internal(). */
	if (gen->durations.adjustment_space_duration > 0) {
		CW_TONE_INIT(&tone, 0, gen->durations.adjustment_space_duration, CW_SLOPE_MODE_NO_SLOPES);
		if (CW_SUCCESS != cw_tq_enqueue_internal(gen->tq, &tone)) {
			/* Reset on error. */
			gen->space_units_count = 0;
			return CW_FAILURE;
		}
		enqueued++;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_GENERATOR, CW_DEBUG_DEBUG,
		      MSG_PREFIX "enqueued %d tones per iws, tq len = %zu",
		      enqueued, cw_tq_length_internal(gen->tq));

	/* We don't really need to store the information that UNITS_PER_IWS units
	   have been enqueued. It's safe to reset the counter here and let the
	   next 'enqueue space' call start from zero. */
	gen->space_units_count = 0;
	return CW_SUCCESS;
}




cw_ret_t cw_gen_enqueue_representation(cw_gen_t * gen, const char * representation)
{
	if (!cw_representation_is_valid(representation)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Before we let this representation loose on tone generation,
	   we'd really like to know that all of its tones will get queued
	   up successfully.  The right way to do this is to calculate the
	   number of tones in our representation, then check that the space
	   exists in the tone queue. However, since the queue is comfortably
	   long, we can get away with just looking for a high water mark.  */
	if (cw_tq_length_internal(gen->tq) >= gen->tq->high_water_mark) {
		errno = EAGAIN;
		return CW_FAILURE;
	}

	/* Enqueue the marks. Every mark is followed by inter-mark-space. */
	for (int i = 0; representation[i] != '\0'; i++) {
		const bool is_first = i == 0;
		if (CW_SUCCESS != cw_gen_enqueue_mark_internal(gen, representation[i], is_first)) {
			return CW_FAILURE;
		}
	}

	/* This function will enqueue just a right amount of space after the last
	   inter-mark-space to form a 3-unit inter-character-space. */
	if (CW_SUCCESS != cw_gen_enqueue_ics_internal(gen)) {
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_enqueue_representation_no_ics(cw_gen_t * gen, const char * representation)
{
	if (!cw_representation_is_valid(representation)) {
		errno = EINVAL;
		return CW_FAILURE;
	}

	/* Before we let this representation loose on tone generation,
	   we'd really like to know that all of its tones will get queued
	   up successfully.  The right way to do this is to calculate the
	   number of tones in our representation, then check that the space
	   exists in the tone queue. However, since the queue is comfortably
	   long, we can get away with just looking for a high water mark.

	   TODO: do the check the proper way.
	   TODO: wrap this check into a function and reuse it in cw_gen_enqueue_representation()
	*/
	if (cw_tq_length_internal(gen->tq) >= gen->tq->high_water_mark) {
		errno = EAGAIN;
		return CW_FAILURE;
	}

	/* Enqueue the marks. Every mark is followed by inter-mark-space. */
	for (int i = 0; representation[i] != '\0'; i++) {
		const bool is_first = i == 0;
		if (CW_SUCCESS != cw_gen_enqueue_mark_internal(gen, representation[i], is_first)) {
			return CW_FAILURE;
		}
	}

	/* No inter-character-space added here. */

	return CW_SUCCESS;
}




/**
   @brief Enqueue a given valid ASCII character in generator, to be sent using Morse code

   _valid_character_ in function's name means that the function expects the
   character @p character to be valid (@p character should be validated by
   caller before passing it to the function).

   no_ics in function's name means that the inter-character-space is not
   appended at the end of Marks and Spaces enqueued in generator (but the
   last inter-mark-space is).

   @exception ENOENT @p character is not a valid character.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator to be used to enqueue character
   @param[in] character character to enqueue

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_valid_character_no_ics_internal(cw_gen_t * gen, char character)
{
	if (NULL == gen) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_GENERATOR, CW_DEBUG_ERROR,
			      MSG_PREFIX "no generator available");
		return CW_FAILURE;
	}

	/* ' ' character (i.e. inter-word-space) is a special case. */
	if (character == ' ') {
		return cw_gen_enqueue_iws_internal(gen);
	}

	const char * representation = cw_character_to_representation_internal(character);

	/* This shouldn't happen since we are in _valid_character_ function... */
	cw_assert (NULL != representation, MSG_PREFIX "failed to find representation for character '%c'/%hhx", character, character);

	/* ... but fail gracefully anyway. */
	if (NULL == representation) {
		errno = ENOENT;
		return CW_FAILURE;
	}

	if (CW_SUCCESS != cw_gen_enqueue_representation_no_ics(gen, representation)) {
		return CW_FAILURE;
	}

	/* No inter-character-space here. */

	return CW_SUCCESS;
}




/**
   @brief Enqueue a given valid ASCII character in generator, to be sent using Morse code

   After enqueueing last Mark (Dot or Dash) comprising a character, an
   inter-mark-space is enqueued. Inter-character-space is enqueued after that
   last inter-mark-space. The inter-character-space is not added if @p
   character is a ' ' character (i.e. inter-word-space).

   _valid_character_ in function's name means that the function expects the
   character @p character to be valid (@p character should be validated by
   caller before passing it to the function).

   @exception ENOENT @p character is not a valid character.

   @reviewed 2023-08-26

   @param[in] gen generator to be used to enqueue character
   @param[in] character character to enqueue

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_valid_character_internal(cw_gen_t * gen, char character)
{
	/* This function is adding 1 Unit of inter-mark-space at the end. */
	if (CW_SUCCESS != cw_gen_enqueue_valid_character_no_ics_internal(gen, character)) {
		return CW_FAILURE;
	}

	if (' ' == character) {
		/*
		  In the context of this function adding ics after freshly enqueued
		  iws would not be valid: iws should not be followed by ics.
		  Therefore don't call cw_gen_enqueue_ics_internal(). Just return
		  now.

		  Since cw_gen_enqueue_valid_character_no_ics_internal() called above
		  has succeeded, return the success here.
		*/
		return CW_SUCCESS;
	}

	/* This function will add enough units to form a full 3-Unit
	   inter-character-space. */
	if (CW_SUCCESS != cw_gen_enqueue_ics_internal(gen)) {
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_enqueue_character(cw_gen_t * gen, char character)
{
	if (!cw_character_is_valid(character)) {
		errno = ENOENT;
		return CW_FAILURE;
	}

	/* This function adds inter-character-space at the end of character. */
	if (CW_SUCCESS != cw_gen_enqueue_valid_character_internal(gen, character)) {
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_enqueue_character_no_ics(cw_gen_t * gen, char character)
{
	if (!cw_character_is_valid(character)) {
		errno = ENOENT;
		return CW_FAILURE;
	}

	/* This function doesn't add inter-character-space at the end of character. */
	if (CW_SUCCESS != cw_gen_enqueue_valid_character_no_ics_internal(gen, character)) {
		return CW_FAILURE;
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_enqueue_string(cw_gen_t * gen, const char * string)
{
	/* Check that the string is composed of valid characters. */
	if (!cw_string_is_valid(string)) {
		errno = ENOENT;
		return CW_FAILURE;
	}

	/* Send every character in the string. */
	for (int i = 0; string[i] != '\0'; i++) {
		/* This function adds inter-character-space at the end of character. */
		if (CW_SUCCESS != cw_gen_enqueue_valid_character_internal(gen, string[i])) {
			return CW_FAILURE;
		}
	}

	return CW_SUCCESS;
}




/**
   @brief Reset generator's essential parameters to their initial values

   You need to call cw_gen_sync_parameters_internal() after call to this function.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator for which to reset parameters
*/
void cw_gen_reset_parameters_internal(cw_gen_t * gen)
{
	cw_assert (NULL != gen, MSG_PREFIX "generator is NULL");

	gen->send_speed = CW_SPEED_INITIAL;
	gen->frequency = CW_FREQUENCY_INITIAL;
	gen->volume_percent = CW_VOLUME_INITIAL;
	gen->volume_abs = (gen->volume_percent * CW_AUDIO_VOLUME_RANGE) / 100;
	gen->gap = CW_GAP_INITIAL;
	gen->weighting = CW_WEIGHTING_INITIAL;

	gen->parameters_in_sync = false;

	return;

}




/**
   @brief Synchronize generator's low level timing parameters

   @reviewedon 2023-08-26

   @param[in] gen generator for which to synchronize parameters
*/
void cw_gen_sync_parameters_internal(cw_gen_t * gen)
{
	cw_assert (NULL != gen, MSG_PREFIX "generator is NULL");

	/* Do nothing if we are already synchronized. */
	if (gen->parameters_in_sync) {
		return;
	}

	/*
	  Set the length of a Dot to be a Unit with any weighting
	  adjustment, and the length of a Dash as three Dot lengths.
	  The weighting adjustment is by adding or subtracting a
	  length based on 50 % as a neutral weighting.
	*/
	gen->durations.unit_duration = CW_DOT_CALIBRATION / gen->send_speed;
	const int weighting_duration = (2 * (gen->weighting - 50) * gen->durations.unit_duration) / 100;
	gen->durations.dot_duration = gen->durations.unit_duration + weighting_duration;
	gen->durations.dash_duration = 3 * gen->durations.dot_duration;

	/*
	  The duration of inter-mark-space is adjusted by 28/22 times
	  weighting length to keep PARIS calibration correctly
	  timed (PARIS has 22 full units, and 28 empty ones).
	  Inter-mark-space and inter-character-space take
	  weightings into account.
	*/
	const int w = (28 * weighting_duration) / 22;

	gen->durations.ims_duration = UNITS_PER_IMS * gen->durations.unit_duration - w;
	gen->durations.ics_duration = UNITS_PER_ICS * gen->durations.unit_duration + w;
	gen->durations.iws_duration = UNITS_PER_IWS * gen->durations.unit_duration - w;
	gen->durations.additional_space_duration = gen->gap * gen->durations.unit_duration;

	/* For "Farnsworth", there also needs to be an adjustment
	   delay added to the end of words, otherwise the rhythm is
	   lost on word end.
	   I don't know if there is an "official" value for this,
	   but 2.33 or so times the gap is the correctly scaled
	   value, and seems to sound okay.

	   Thanks to Michael D. Ivey <ivey@gweezlebur.com> for
	   identifying this in earlier versions of libcw. */
	gen->durations.adjustment_space_duration = (7 * gen->durations.additional_space_duration) / 3;

	cw_debug_msg (&cw_debug_object, CW_DEBUG_PARAMETERS, CW_DEBUG_INFO,
		      MSG_PREFIX "'%s': gen durations [us] at speed %d [wpm]:\n"
		      "[II] " MSG_PREFIX "    dot: %11d\n"
		      "[II] " MSG_PREFIX "   dash: %11d\n"
		      "[II] " MSG_PREFIX "    ims: %11d\n"
		      "[II] " MSG_PREFIX "    ics: %11d\n"
		      "[II] " MSG_PREFIX "    iws: %11d\n"
		      "[II] " MSG_PREFIX "   adsd: %11d\n"
		      "[II] " MSG_PREFIX "   ajsd: %11d",

		      gen->label,
		      gen->send_speed,
		      gen->durations.dot_duration,
		      gen->durations.dash_duration,
		      gen->durations.ims_duration,
		      gen->durations.ics_duration,
		      gen->durations.iws_duration,
		      gen->durations.additional_space_duration,
		      gen->durations.adjustment_space_duration);

	/* Generator parameters are now in sync. */
	gen->parameters_in_sync = true;

	return;
}




/**
   @brief Enqueue beginning of mark from straight key

   Helper function intended to hide from keying module the details of tone
   queue and of enqueueing a tone.

   Call this function from straight key code only (see "sk" in function's
   name). The function should be called only on "key down" (begin mark) event
   from hardware straight key.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_sk_begin_mark_internal(cw_gen_t * gen)
{
	/* In case of straight key we don't know at all how long the
	   tone should be (we don't know for how long the straight key
	   will be closed).

	   Let's enqueue a beginning of mark (rising slope) +
	   "forever" (constant) tone. The constant tone will be generated
	   until key goes into CW_KEY_VALUE_OPEN state. */


	/* Enqueue rising slope */

	cw_tone_t tone;
	CW_TONE_INIT(&tone, gen->frequency, gen->tone_slope.duration, CW_SLOPE_MODE_RISING_SLOPE);
	cw_ret_t cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	/* Enqueueing a mark, so reset counter of enqueued space units. */
	gen->space_units_count = 0;
	if (cwret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_TONE_QUEUE, CW_DEBUG_ERROR,
			      MSG_PREFIX "enqueue begin mark: failed to enqueue rising slope: '%s'", strerror(errno));
		/* TODO: what do we do with this error now? The cwret
		   variable will be overwritten below. */
	}


	/* Enqueue plateau - forever tone. */

	/* If there was an error during enqueue of rising slope of
	   mark, assume that it was a transient error, and proceed to
	   enqueueing forever tone. Only after we fail to enqueue the
	   "main" tone, we are allowed to return failure to caller. */
	CW_TONE_INIT(&tone, gen->frequency, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
	tone.is_forever = true;
	cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	/* Enqueueing a mark, so reset counter of enqueued space units. */
	gen->space_units_count = 0;
	if (cwret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_TONE_QUEUE, CW_DEBUG_ERROR,
			      MSG_PREFIX "enqueue begin mark: failed to enqueue forever tone: '%s'", strerror(errno));
	}


	cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_TONE_QUEUE, CW_DEBUG_DEBUG,
		      MSG_PREFIX "enqueue begin mark: tone queue len = %zu", cw_tq_length_internal(gen->tq));

	return cwret;
}




/**
   @brief Enqueue beginning of space from straight key

   Helper function intended to hide from keying module the details of tone
   queue and of enqueueing a tone.

   Call this function from straight key code only (see "sk" in function's
   name). The function should be called only on "key up" (begin space) event
   from hardware straight key.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_sk_begin_space_internal(cw_gen_t * gen)
{
	cw_ret_t cwret = CW_FAILURE;

	if (gen->sound_system == CW_AUDIO_CONSOLE) {
		/* FIXME: I think that enqueueing tone is not just a
		   matter of generating it using generator, but also a
		   matter of timing events using generator. Enqueueing
		   tone here and dequeueing it later will be used to
		   control state of a key. How does enqueueing a
		   quantum tone influences the key state? */


		/* Generate just a bit of silence, just to switch
		   buzzer from generating a sound to being silent. */
		cw_tone_t tone;
		CW_TONE_INIT(&tone, 0, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
		cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	} else {
		/* For soundcards a falling slope with volume from max
		   to zero should be enough, but... */
		cw_tone_t tone;
		CW_TONE_INIT(&tone, gen->frequency, gen->tone_slope.duration, CW_SLOPE_MODE_FALLING_SLOPE);
		cwret = cw_tq_enqueue_internal(gen->tq, &tone);
		if (CW_SUCCESS == cwret) {
			/* ... but on some occasions, on some
			   platforms, some sound systems may need to
			   constantly generate "silent" tone. These four
			   lines of code are just for them.

			   FIXME: what occasions? what platforms? what sound systems?

			   It would be better to avoid queueing silent
			   "forever" tone because this increases CPU
			   usage. It would be better to simply not to
			   queue any new tones after "falling slope"
			   tone. Silence after the last falling slope
			   would simply last on itself until there is
			   new tone in queue to dequeue. */
			CW_TONE_INIT(&tone, 0, gen->quantum_duration, CW_SLOPE_MODE_NO_SLOPES);
			tone.is_forever = true;
			cwret = cw_tq_enqueue_internal(gen->tq, &tone);
		}
	}

	/* The function enqueues 'space', this 'space' means just 'silence'.
	   Reset space unit counts because no meaningful 'space units' have been
	   enqueued. */
	gen->space_units_count = 0;

	return cwret;
}




/**
   @brief Enqueue an element (dot/dash/ims) from iambic keyer

   Helper function intended to hide from keying module the details of tone
   queue and of enqueueing a tone.

   Call this function from iambic keyer code only (see "ik" in function's
   name). The function should be called on hardware key events only. Since we
   enqueue symbols, we know that they have limited, specified
   length. This means that the function should be called for events
   from iambic keyer.

   @p symbol may be an ims, and the ims will be enqueued. But if the symbol
   is dot or dash, the function won't append ims after the dot or dash -
   hence "no_ims" in function's name.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator
   @param[in] symbol symbol to enqueue (Space/Dot/Dash)

   @return CW_SUCCESS on success
   @return CW_FAILURE on failure
*/
cw_ret_t cw_gen_enqueue_ik_symbol_no_ims_internal(cw_gen_t * gen, char symbol)
{
	cw_tone_t tone = { 0 };

	/* In all other places the assignment to gen->space_units_count happens
	   after call to cw_tq_enqueue_internal(). To keep this convention I need
	   this temporary var. */
	int units_count = 0;

	switch (symbol) {
	case CW_DOT_REPRESENTATION:
		CW_TONE_INIT(&tone, gen->frequency, gen->durations.dot_duration, CW_SLOPE_MODE_STANDARD_SLOPES);
		/* Enqueueing a mark means resetting of spaces counter. */
		units_count = 0;
		break;

	case CW_DASH_REPRESENTATION:
		CW_TONE_INIT(&tone, gen->frequency, gen->durations.dash_duration, CW_SLOPE_MODE_STANDARD_SLOPES);
		/* Enqueueing a mark means resetting of spaces counter. */
		units_count = 0;
		break;

	case CW_SYMBOL_IMS:
		CW_TONE_INIT(&tone, 0, gen->durations.ims_duration, CW_SLOPE_MODE_NO_SLOPES);
		/* Enqueueing an ims. Record this fact in space units counter. */
		units_count = UNITS_PER_IMS;
		break;
	default:
		cw_assert (0, MSG_PREFIX "unknown iambic keyer symbol '%d'", symbol);
		/* Reset on error. */
		units_count = 0;
		break;
	}

	const cw_ret_t cwret = cw_tq_enqueue_internal(gen->tq, &tone);
	gen->space_units_count = units_count;
	return cwret;
}




cw_ret_t cw_gen_wait_for_queue_level(cw_gen_t * gen, size_t level)
{
	return cw_tq_wait_for_level_internal(gen->tq, level);
}




void cw_gen_flush_queue(cw_gen_t * gen)
{
	/* This function locks and unlocks mutex. */
	cw_tq_flush_internal(gen->tq);

	/* TODO: we probably want to have these two functions
	   separated. Function called cw_gen_flush_queue() probably shouldn't
	   also silence generator. */

	/* Force silence on the speaker anyway, and stop any background
	   soundcard tone generation. */
	cw_gen_silence_internal(gen);

	return;
}




cw_ret_t cw_gen_remove_last_character(cw_gen_t * gen)
{
	return cw_tq_remove_last_character_internal(gen->tq);
}




cw_ret_t cw_gen_get_sound_device(cw_gen_t const * gen, char * buffer, size_t size)
{
	cw_assert (NULL != gen, MSG_PREFIX "generator is NULL");
	snprintf(buffer, size, "%s", gen->picked_device_name);
	return CW_SUCCESS;
}




int cw_gen_get_sound_system(cw_gen_t const * gen)
{
	cw_assert (NULL != gen, MSG_PREFIX "generator is NULL");
	return gen->sound_system;
}




size_t cw_gen_get_queue_length(cw_gen_t const * gen)
{
	return cw_tq_length_internal(gen->tq);
}




cw_ret_t cw_gen_register_low_level_callback(cw_gen_t * gen, cw_queue_low_callback_t callback_func, void * callback_arg, size_t level)
{
	return cw_tq_register_low_level_callback_internal(gen->tq, callback_func, callback_arg, level);
}




cw_ret_t cw_gen_wait_for_end_of_current_tone(cw_gen_t * gen)
{
	return cw_tq_wait_for_end_of_current_tone_internal(gen->tq);
}




bool cw_gen_is_queue_full(cw_gen_t const * gen)
{
	return cw_tq_is_full_internal(gen->tq);
}




cw_ret_t cw_gen_set_label(cw_gen_t * gen, const char * label)
{
	if (NULL == gen) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'gen' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", gen->label);
		return CW_FAILURE;
	}
	if (strlen(label) > (LIBCW_OBJECT_INSTANCE_LABEL_SIZE - 1)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_WARNING,
			      MSG_PREFIX "'%s': new label '%s' too long, truncating", gen->label, label);
		/* Not an error, just log warning. New label will be truncated. */
	}

	/* Notice that empty label is acceptable. In such case we will
	   erase old label. */

	snprintf(gen->label, sizeof (gen->label), "%s", label);

	/* Generator's tone queue is not publicly available, but we
	   have to set some label for the queue as well. */
	if (NULL == gen->tq) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_INTERNAL, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': tq is NULL, not setting label for it", gen->label);
		/* This is not a good place to investigate why tq is
		   NULL, so simply continue. */
	} else {
		snprintf(gen->tq->label, sizeof (gen->tq->label), "%s", label);
	}

	return CW_SUCCESS;
}




cw_ret_t cw_gen_get_label(const cw_gen_t * gen, char * label, size_t size)
{
	if (NULL == gen) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'gen' argument is NULL");
		return CW_FAILURE;
	}
	if (NULL == label) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_CLIENT_CODE, CW_DEBUG_ERROR,
			      MSG_PREFIX "'%s': 'label' argument is NULL", gen->label);
		return CW_FAILURE;
	}

	/* Notice that we don't care if size is zero. */

	snprintf(label, size, "%s", gen->label);

	return CW_SUCCESS;
}




/**
   @brief Function used to set and track value of generator

   @internal
   @reviewed 2020-10-03
   @endinternal

   @param[in] gen generator
   @param[in] tone tone dequeued from generator's tone queue
   @param[in] queue_state state of queue after dequeueing current tone

   @return CW_SUCCESS
*/
static cw_ret_t cw_gen_value_tracking_internal(cw_gen_t * gen, const cw_tone_t * tone, cw_queue_state_t queue_state)
{
	cw_key_value_t value = CW_KEY_VALUE_OPEN;

	switch (queue_state) {
	case CW_TQ_JUST_EMPTIED:
	case CW_TQ_NONEMPTY:
		/* A valid tone has been dequeued just now. */
		value = tone->frequency ? CW_KEY_VALUE_CLOSED : CW_KEY_VALUE_OPEN;
		break;

	case CW_TQ_EMPTY:
		/* Tone queue remains empty. No new tone == no sound. */
		value = CW_KEY_VALUE_OPEN;
		break;
	default:
		cw_assert (0, MSG_PREFIX "unexpected state of tone queue: %d", queue_state);
		break;
	}
	cw_gen_value_tracking_set_value_internal(gen, gen->key, value);

	return CW_SUCCESS;
}




/**
   @brief Set new value of generator

   Filter successive calls with identical value of @p value single action
   (successive calls with the same value of @p value don't change internally
   registered value of generator).

   If and only if the function registers change of generator value, an
   external callback function (if configured) is called.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator for which to set new value
   @param[in] key TODO: document
   @param[in] value value of generator to be set
*/
void cw_gen_value_tracking_set_value_internal(cw_gen_t * gen, __attribute__((unused)) volatile cw_key_t * key, cw_key_value_t value)
{
	//cw_assert (NULL != key, MSG_PREFIX "gen track value: key is NULL");

	if (gen->value_tracking.value == value) {
		/* This is not an error. This may happen when
		   dequeueing 'forever' tone multiple times in a
		   row. */
		/*
		  TODO: uncomment this fprintf() and see how often it's
		  called for straight key actions in xcwcp.
		  2020-10-17: the frequency may be now decreased after we now
		  call pthread_cond_broadcast() in
		  cw_gen_dequeue_and_generate_internal() only conditionally,
		  if two consecutive tones aren't 'forever'.
		*/
		// fprintf(stderr, "gen: dropping the same value %d -> %d\n", gen->value_tracking.value, value);
		return;
	}

	cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_INFO,
		      MSG_PREFIX "set gen value: %d->%d", gen->value_tracking.value, value);

	/* Remember the new generator value. */
	gen->value_tracking.value = value;

	/*
	  In theory client code should register either a receiver (so
	  events from key are passed to receiver directly), or a
	  callback (so events from key are passed to receiver through
	  callback).

	  See comment for cw_key_register_receiver() in libcw_key.c:

	  "Receiver should somehow receive key events from physical or
	  logical key. This can be done in one of two ways:"

	  These two ways are represented by the two 'if' blocks
	  below. So *in theory* only one of these "if" blocks will be
	  executed.
	*/

#if 0
	if (false && key->rec) {
		if (CW_KEY_VALUE_CLOSED == gen->value_tracking.value) {
			/* Key down. */
			cw_rec_mark_begin(key->rec,
#ifdef IAMBIC_KEY_HAS_TIMER
					  key->ik.ik_timer
#else
					  NULL
#endif
					  );
		} else {
			/* Key up. */
			cw_rec_mark_end(key->rec,
#ifdef IAMBIC_KEY_HAS_TIMER
					key->ik.ik_timer
#else
					NULL
#endif
					);
		}
	}
#endif
#if 1
	if (gen->value_tracking.value_tracking_callback_func) {
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_KEYING, CW_DEBUG_INFO,
			      MSG_PREFIX "set gen value: about to call value tracking callback, generator value = %d\n", gen->value_tracking.value);

		(*gen->value_tracking.value_tracking_callback_func)(gen->value_tracking.value_tracking_callback_arg, gen->value_tracking.value);
	}
#endif
	return;
}




/**
   @brief Register external callback function for tracking state of generator

   Register a @p callback_func function that should be called when a state of
   a @p gen changes of value from CW_KEY_VALUE_OPEN to CW_KEY_VALUE_CLOSED or
   vice-versa.

   The first argument passed to the registered callback function is
   the supplied @p callback_arg, if any.

   The second argument passed to registered callback function is the
   generator's value: CW_KEY_VALUE_OPEN or CW_KEY_VALUE_CLOSED.

   Calling this routine with a NULL function address removes
   previously registered callback.

   @internal
   @reviewed 2020-08-06
   @endinternal

   @param[in] gen generator for which to register a callback
   @param[in] callback_func callback function to be called on generator state changes
   @param[in] callback_arg first argument to callback_func
*/
void cw_gen_register_value_tracking_callback_internal(cw_gen_t * gen, cw_gen_value_tracking_callback_t callback_func, void * callback_arg)
{
	gen->value_tracking.value_tracking_callback_func = callback_func;
	gen->value_tracking.value_tracking_callback_arg = callback_arg;

	return;
}




/**
   @brief Pick a device name for given sound system

   Don't call this function for CW_AUDIO_SOUNDCARD because this function
   doesn't implement logic for selecting a current sound system out of a set
   of systems that are able to use a soundcard (PulseAudio, ALSA, OSS).

   For ALSA sound system the value returned through @p picked_device_name is
   guaranteed to be non-empty.

   For PulseAudio the value returned through @p picked_device_name will be
   empty if default device is to be used. PA API accepts NULL pointer as
   indication to use default device, so the empty value of @p
   picked_device_name should be somehow "converted" to NULL pointer passed to
   PulseAudio API.

   TODO: selection of device names in libcw is a mess full of repetitions.
   Re-think and re-implement it. Keep in mind that for PulseAudio sound
   system a NULL passed to the sound system's API is allowed value,
   indicating "use default". So we want to be able to pass NULL to the API.

   @reviewed 2020-11-14

   @param[in] alternative_device_name device name provided by library's client code (may be NULL or empty)
   @param[in] sound_system sound system for which the device name is being picked
   @param[out] picked_device_name output buffer
   @param[in] size size of @p picked_device_name buffer

   @return CW_SUCCESS if the device name has been picked successfully
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_gen_pick_device_name_internal(const char * alternative_device_name, enum cw_audio_systems sound_system, char * picked_device_name, size_t size)
{
	snprintf(picked_device_name, size, "%s", "");
	cw_ret_t cwret = CW_FAILURE;

	switch (sound_system) {
	case CW_AUDIO_NULL:
		/* This is an internal type of sound system (and device name
		   isn't really used), and I decided that for simplicity the
		   returned pointer will never be NULL. So behaviour is the
		   same as for ALSA. */

	case CW_AUDIO_CONSOLE:
	case CW_AUDIO_OSS:
		/* For above 2 sound systems the Unix open() function doesn't
		   do any special interpretation of NULL pointer argument or
		   empty string argument. We have to provide explicit device
		   name or path. So behaviour is the same as for ALSA. */

	case CW_AUDIO_ALSA:
		/* When you want to tell ALSA to use ALSA's default device,
		   don't pass NULL or empty string, because ALSA's
		   snd_pcm_open() doesn't interpret NULL or empty string in
		   any special way. Use explicit value of 'device name'
		   argument. */
		if (NULL == alternative_device_name
		    || '\0' == alternative_device_name[0]
		    || 0 == strcmp(alternative_device_name, default_sound_devices[sound_system])) {

			/* No alternative device provided, use libcw's
			   default. */
			snprintf(picked_device_name, size, "%s", default_sound_devices[sound_system]);
		} else {
			/* Use non-default device provided by client code. */
			snprintf(picked_device_name, size, "%s", alternative_device_name);
		}
		cwret = CW_SUCCESS;
		break;

	case CW_AUDIO_PA:
		if (NULL == alternative_device_name
		    || '\0' == alternative_device_name[0]
		    || 0 == strcmp(alternative_device_name, default_sound_devices[sound_system])) {

			/* Empty: code that will call PulseAudio API will
			   have to recognize empty string and pass NULL to
			   PulseAudio API. The API will recognize the NULL as
			   'select PulseAudio's default device'. */
			snprintf(picked_device_name, size, "%s", "");
		} else {
			/* Use non-default device name provided by client
			   code. */
			snprintf(picked_device_name, size, "%s", alternative_device_name);
		}
		cwret = CW_SUCCESS;
		break;

	case CW_AUDIO_SOUNDCARD:
		/* This function should never be called for SOUNDCARD sound
		   device. It should be called for specific sound systems
		   only. */
		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_ERROR,
			      MSG_PREFIX "%s:%d unexpected sound system %d\n",
			      __func__, __LINE__, sound_system);
		cwret = CW_FAILURE;
		break;

	case CW_AUDIO_NONE:
	default:
		cw_debug_msg (&cw_debug_object, CW_DEBUG_KEYING, CW_DEBUG_ERROR,
			      MSG_PREFIX "%s:%d invalid sound system %d\n",
			      __func__, __LINE__, sound_system);
		cwret = CW_FAILURE;
		break;
	}

	return cwret;
}




int cw_gen_get_shortest_dot_duration_internal(void)
{
	const int speed = CW_SPEED_MAX;
	const int weighting = CW_WEIGHTING_MIN;

	const int unit_duration = CW_DOT_CALIBRATION / speed;
	const int weighting_duration = (2 * (weighting - 50) * unit_duration) / 100;
	const int dot_duration = unit_duration + weighting_duration;

	return dot_duration;
}



