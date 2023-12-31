/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_GEN
#define H_LIBCW_GEN



#include "config.h"

#include "libcw.h"
#include "libcw2.h"

#include <cwutils/cw_config.h>
#include "libcw_alsa.h"
#include "libcw_console.h"
#include "libcw_key.h"
#include "libcw_oss.h"
#include "libcw_pa.h"
#include "libcw_tq.h"




/*
  Duration of a single slope (rising or falling) in standard tone. [us]

  TODO: check what is the duration of the shortest possible tone (at highest
  speed) and how much of that tone would be "spent" in slopes.
*/
#define CW_AUDIO_SLOPE_DURATION       5000



/* Symbolic name for inter-mark-space. TODO: this should not be a space
   character. Space character is reserved for inter-character-space.*/
enum { CW_SYMBOL_IMS = ' ' };




/**
   @brief Parameters describing timing of different elements (marks and
   spaces) produced by a generator
*/
typedef struct {
	int unit_duration;  /**< Duration of basic timing unit, derived from generator's sending speed. [microseconds] */
	int dot_duration;   /**< Duration of a dot Mark. [microseconds] */
	int dash_duration;  /**< Duration of a dash Mark. [microseconds] */
	int ims_duration;   /**< Duration of inter-mark-space (i.e. the Space). [microseconds] */
	int ics_duration;   /**< Duration of inter-character-space. [microseconds] */
	int iws_duration;   /**< Duration of inter-word-space. [microseconds] */
	int additional_space_duration; /**< Duration of additional space at the end of a character. [microseconds] */
	int adjustment_space_duration; /**< Duration of adjustment space at the end of a word. [microseconds] */
} cw_gen_durations_t;




struct cw_gen_struct {

	/* Tone queue. */

	/* Generator can only generate tones that were first put into
	   queue, and then dequeued. Here is a tone queue associated
	   with a generator. One tone queue per generator. One
	   generator per tone queue.

	   The tone queue should be created in generator's
	   constructor, and deleted in generator's destructor using
	   tone queue's own constructor and destructor functions - see
	   cw_tq module for declarations of these functions.

	   TODO: maybe this doesn't have to be a pointer, perhaps this can be
	   a regular variable. We don't need to allocate a tone queue. The tq
	   should always exist, and there should be always just one tone
	   queue. So why allocate it?
	*/
	cw_tone_queue_t * tq;




	/* Parameters. */

	/* Generator's basic parameters. */
	int send_speed;     /* [wpm] */
	int frequency;      /* The frequency of generated sound. [Hz] */
	int volume_percent; /* Level of sound in percents of maximum allowable level. */
	int volume_abs;     /* Level of sound in absolute terms; height of PCM samples. */
	int gap;            /* Inter-mark-space. [number of dot durations]. */
	int weighting;      /* Dot/dash weighting. */




	/* Generator's timing parameters. */
	/* These are basic timing parameters which should be
	   recalculated each time client code demands changing some
	   higher-level parameter of generator (e.g. changing of
	   sending speed).

	   Be sure to read comment in cw_gen_sync_parameters_internal() on
	   calculation of values of these parameters. */
	cw_gen_durations_t durations;




	/* Generator's misc parameters. */
	/* Shortest duration of tone. Right now it's initialized in
	   constructor and never changed afterwards, but in future it
	   may change - the value may be dynamically modified for
	   performance reasons. The longer the quantum duration, the
	   less often some functions need to be called. */
	int quantum_duration;




	/* After changing sending speed, gap or weighting, some
	   generator's internal parameters need to be recalculated.
	   This is a flag that shows when this needs to be done. */
	bool parameters_in_sync;




	/* Misc fields. */

	/* Sound buffer and related items. */
	/* Buffer storing sine wave that is calculated in "calculate sine
	   wave" cycles and sent to sound system (OSS, ALSA, PulseAudio).

	   The buffer should be always filled with valid data before sending
	   it to sound system (to avoid hearing garbage).

	   We should also send exactly buffer_n_samples samples to sound
	   system, in order to avoid situation when sound system waits for
	   filling its buffer too long - this would result in errors and
	   probably audible clicks. */
	cw_sample_t * buffer;

	/* Size of data buffer, in samples.

	   The size may be restricted (min,max) by current sound system
	   (OSS, ALSA, PulseAudio); the sound system may also accept only
	   specific values of the size.

	   Sound system libraries may provide functions that can be
	   used to query for allowed sound buffer sizes.

	   The smaller the buffer, the more often you have to call function
	   writing data to sound system, which increases CPU usage.

	   The larger the buffer, the less responsive an application may
	   be to changes of sound data parameters (depending on application
	   type). */
	int buffer_n_samples;


	/* We need two indices to gen->buffer, indicating beginning
	   and end of a subarea in the buffer.  The subarea is not
	   exactly the same as gen->buffer for variety of reasons:
	   - buffer length is almost always smaller than length of a
	   Mark or Space (in samples) that we want to produce;
	   - moreover, length of a Mark/Space (in samples) is almost
	   never an exact multiple of length of a buffer;
	   - as a result, a sound representing a Mark/Space may start
	   and end anywhere between beginning and end of the buffer;

	   A workable solution is to have a subarea of the buffer, a
	   window, into which we will write a series of fragments of
	   calculated sound.

	   "start" and "stop" mark beginning and end of the subarea
	   (inclusive: samples indexed by "start" and "stop" are part
	   of the subarea).

	   The subarea shall not wrap around boundaries of the buffer:
	   "stop" shall be no larger than "gen->buffer_n_samples - 1",
	   and it shall never be smaller than "start". "start" will be
	   somewhere between zero and "stop", inclusive.

	   "start" == "stop" is a valid situation - length of subarea
	   is then one.

	   Very often (in the middle of a long tone), "start" will be
	   zero, and "stop" will be "gen->buffer_n_samples - 1".

	   Sine wave (sometimes with amplitude = 0) will be calculated
	   for subarea's cells (samples) ranging from cell "start"
	   to cell "stop", inclusive. */
	int buffer_sub_start;
	int buffer_sub_stop;

	/* Set to the same value of sample rate as configured on sound
	   sink. */
	unsigned int sample_rate;

	/* Used to calculate sine wave.
	   Phase offset needs to be stored between consecutive calls to
	   function calculating consecutive fragments of sine wave. */
	float phase_offset;



	/* Tone parameters. */
	/* Some parameters of tones (and of tones' slopes) are common
	   for all tones generated in given time by a generator.
	   Therefore the generator should contain this struct.

	   Other parameters, such as tone's length or frequency, are
	   strictly related to tones - you won't find them here. */
	struct {
		/* Depending on sample rate, sending speed, and user
		   preferences, duration of slope of tones generated by
		   generator may vary, but once set, it is constant
		   for all generated tones (until next change of
		   sample rate, sending speed, etc.).

		   This is why we have the slope duration in generator.

		   n_amplitudes declared a bit below in this struct is
		   a secondary parameter, derived from ->duration. */
		int duration; /* [us] */

		/* Linear/raised cosine/sine/rectangle. */
		int shape;

		/* Table of amplitudes of every PCM sample of tone's
		   slope.

		   The values in amplitudes[] change from zero to max
		   (at least for any sane slope shape), so naturally
		   they can be used in forming rising slope. However
		   they can be used in forming falling slope as well -
		   just iterate the table from end to beginning.

		   TODO: it seems that amplitudes can be
		   integers. Investigate it. */
		float * amplitudes;

		/* This is a secondary parameter, derived from
		   ->duration. n_amplitudes is useful when iterating over
		   ->amplitudes[] or reallocing the ->amplitudes[]. */
		int n_amplitudes;
	} tone_slope;



	/* Library's client (client code using library). */
	struct library_client {
		/* Main thread, existing from beginning to end of main process run.
		   The variable is used to send signals to main app thread. */
// #define GENERATOR_CLIENT_THREAD // Disabled on 2020-08-04
#ifdef GENERATOR_CLIENT_THREAD
		pthread_t thread_id;
#endif
		char * name;
	} library_client;



	/* cw key associated with this generator. */
	/* Key that has a generator associated with it.

	   Standalone generator will have this set to NULL. But
	   generator that is used by a key will have this set to
	   non-NULL value with cw_key_register_generator().

	   Remember that the key needs to have a generator, not the
	   other way around. TODO: explain why key requires a
	   generator. */
	volatile struct cw_key_struct * key;




	/* pthread */

	/* Properties of generator's thread function is that is used
	   to generate sine wave and write the wave to sound sink
	   (cw_gen_dequeue_and_generate_internal()). */
	struct {
		pthread_t      id;
		pthread_attr_t attr;

		/* Call to pthread_create(&id, ...) executed and
		   succeeded? If not, don't call pthread_kill(). I
		   can't check value of thread.id, because pthread_t
		   data type is opaque.

		   This flag is a bit different than
		   cw_gen_t->do_dequeue_and_generate.  Setting
		   ->do_dequeue_and_generate signals intent to run a loop
		   deqeueing tones in
		   cw_gen_dequeue_and_generate_internal().  Setting
		   ->thread.running means that thread function
		   cw_gen_dequeue_and_generate_internal() was launched
		   successfully. */
		bool running;
	} thread;

	/* start/stop flag.
	   Set to true before running dequeue_and_play thread
	   function.
	   Set to false to stop generator and return from
	   dequeue_and_generate thread function. */
	bool do_dequeue_and_generate;

	bool silencing_initialized;



	/* Sound system. */

	/* Path to console file, or path to OSS soundcard file, or ALSA sound
	   device name, or PulseAudio device name. */
	char picked_device_name[LIBCW_SOUND_DEVICE_NAME_SIZE];

	/* none/null/console/OSS/ALSA/PulseAudio */
	int sound_system;

	bool sound_device_is_open;

#ifdef ENABLE_DEV_PCM_SAMPLES_FILE
	/* Output file descriptor for debug data (console, OSS, ALSA,
	   PulseAudio). */
	int dev_raw_sink;

	/* Test code would like to know the path to the sink file, therefore the
	   path is a member of generator type. The value is set by generator. */
	char dev_raw_sink_path[256];
#endif

	/* Open and configure sound system handle stored in given generator. */
	cw_ret_t (* open_and_configure_sound_device)(cw_gen_t * gen, const cw_gen_config_t * gen_conf);

	/* Close sound system device stored in given generator. */
	void (* close_sound_device)(cw_gen_t * gen);

	/* Two types of write functions:
	   - one that writes to device samples from generator's buffer (the
             contents is calculated earlier),
	   - one that writes to device an on/off state from provided tone
             argument.

	   The first function is implemented by real sound systems that
	   support real sound cards: OSS, ALSA and PulseAudio.

	   The second function is for pseudo sound systems that don't access
	   a sound card: Null and Console. */
	cw_ret_t (* write_buffer_to_sound_device)(cw_gen_t * gen);
	cw_ret_t (* write_tone_to_sound_device)(cw_gen_t * gen, const cw_tone_t * tone);

	/**
	   @brief Do some housekeeping of sound sink when tone queue goes completely empty

	   A sound system may not set this function pointer.

	   @param[in/out] gen generator with opened sound sink

	   @return CW_SUCCESS on success
	   @return CW_FAILURE on failure
	*/
	cw_ret_t (* on_empty_queue)(cw_gen_t * gen);

	/*
	  Current value of generator, as dictated by value of the tone
	  that has been most recently dequeued. Value tracking
	  mechanism filters out consecutive tones with the same on/off
	  value, providing a consistent 'current' value.

	  The value is down/up; closed/open; generating/silent;
	  on/off; mark/space; sound/no-sound.

	  We also have a callback pointer. Callback can be registered
	  by client code, and generator will call it each time the
	  value changes.
	*/
	struct value_tracking {
		cw_key_value_t value;

		cw_gen_value_tracking_callback_t value_tracking_callback_func;
		void * value_tracking_callback_arg;
	} value_tracking;


	char label[LIBCW_OBJECT_INSTANCE_LABEL_SIZE];


	cw_console_data_t console;

#ifdef LIBCW_WITH_OSS
	/* Data used by OSS. */
	cw_oss_data_t oss_data;
#endif

#ifdef LIBCW_WITH_ALSA
	/* Data used by ALSA. */
	cw_alsa_data_t alsa_data;
#endif

#ifdef LIBCW_WITH_PULSEAUDIO
	/* Data used by PulseAudio. */
	cw_pa_data_t pa_data;
#endif

	/*
	  Count of units of space enqueued in generator.
	  When enqueueing ims, ics or iws, increase the counter accordingly.
	  When enqueueing a mark (dot or dash), reset the counter.
	  On errors reset the counter.
	*/
	int space_units_count;
};




/**
   @brief Get generator's duration parameters

   Return the low-level timing parameters calculated from generator's speed,
   gap, tolerance, and weighting. Units of returned parameter values are
   microseconds.

   Use NULL for the pointer argument to any parameter value not required.

   @p gen's parameters are synced with cw_gen_sync_parameters_internal()
   within this function before they are returned.

   This function does the same what cw_gen_get_durations_internal() does, but
   here the duration parameters are returned individually by a set of 'int*'
   arguments.

   TODO acerion 2023.08.26 Give better description of last two function args.

   @reviewedon 2023.08.26

   @param[in] gen generator from which to get durations
   @param[out] dot_duration Duration of a dot
   @param[out] dash_duration Duration of a dash
   @param[out] ims_duration Duration of inter-mark-space
   @param[out] ics_duration Duration of inter-character-space
   @param[out] iws_duration Duration of inter-word-space
   @param[out] additional_space_duration Additional duration
   @param[out] adjustment_space_duration Adjustment duration
*/
void cw_gen_get_timing_parameters_internal(cw_gen_t * gen,
					   int * dot_duration,
					   int * dash_duration,
					   int * ims_duration,
					   int * ics_duration,
					   int * iws_duration,
					   int * additional_space_duration,
					   int * adjustment_space_duration);




/**
   @brief Get generator's duration parameters

   Return the low-level timing parameters calculated from generator's speed,
   gap, tolerance, and weighting. Units of returned parameter values are
   microseconds.

   @p gen's parameters are synced with cw_gen_sync_parameters_internal()
   within this function before they are returned.

   This function does the same what cw_gen_get_timing_parameters_internal()
   does, but here the duration parameters are returned collectively in a @p
   durations structure.

   @reviewedon 2023.08.26

   @param[in] gen generator from which to get durations
   @param[out] durations structure with generator's durations
*/
void cw_gen_get_durations_internal(cw_gen_t * gen, cw_gen_durations_t * durations);




/* Generator's 'enqueue' primitives. */
cw_ret_t cw_gen_enqueue_mark_internal(cw_gen_t * gen, char mark, bool is_first);
cw_ret_t cw_gen_enqueue_ics_internal(cw_gen_t * gen);
cw_ret_t cw_gen_enqueue_iws_internal(cw_gen_t * gen);
cw_ret_t cw_gen_enqueue_valid_character_internal(cw_gen_t * gen, char character);

/* These are also 'enqueue' primitives, but are intended to be used on
   hardware keying events from straight key (sk) and iambic keyer (ik). */
cw_ret_t cw_gen_enqueue_sk_begin_mark_internal(cw_gen_t * gen);
cw_ret_t cw_gen_enqueue_sk_begin_space_internal(cw_gen_t * gen);
cw_ret_t cw_gen_enqueue_ik_symbol_no_ims_internal(cw_gen_t * gen, char symbol);

cw_ret_t cw_gen_silence_internal(cw_gen_t * gen);
char * cw_gen_get_sound_system_label_internal(const cw_gen_t * gen, char * buffer, size_t size);

void cw_generator_delete_internal(void);

void cw_gen_reset_parameters_internal(cw_gen_t * gen);
void cw_gen_sync_parameters_internal(cw_gen_t * gen);

/**
   @brief Get the duration of the shortest dot mark that can be generated by libcw

   This function is used by ALSA when configuring ALSA's parameters.

   TODO (acerion) 2023.08.06 This function duplicates calculations done in
   cw_gen_sync_parameters_internal(). Try somehow to decrease the
   duplication.

   @reviewedon 2023.08.26

   @return The duration of the shortest dot mark
*/
int cw_gen_get_shortest_dot_duration_internal(void);

cw_ret_t cw_gen_pick_device_name_internal(const char * alternative_device_name, enum cw_audio_systems sound_system, char * picked_device_name, size_t size);

int cw_generator_new_internal(const cw_gen_config_t * gen_conf);



#endif /* #ifndef H_LIBCW_GEN */
