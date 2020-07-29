/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_TQ
#define H_LIBCW_TQ




#include "config.h"




#include <pthread.h>    /* pthread_mutex_t */
#include <stdbool.h>    /* bool */
#include <stdint.h>     /* uint32_t */




#include "libcw2.h"




#if defined(__cplusplus)
extern "C"
{
#endif





/* Right now there is no function that would calculate number of tones
   representing given character or string, so there is no easy way to
   present exact relationship between capacity of tone queue and
   number of characters that it can hold.  TODO: perhaps we could
   write utility functions to do that calculation? */

/* TODO: create tests that validate correctness of handling of tone
   queue capacity. See if we really handle the capacity correctly. */

enum {
	/* Default and maximum values of two basic parameters of tone
	   queue: capacity and high water mark. The parameters can be
	   modified using suitable function. */

	/* Tone queue will accept at most "capacity" tones. */
	CW_TONE_QUEUE_CAPACITY_MAX = 3000,        /* ~= 5 minutes at 12 WPM */

	/* Tone queue will refuse to accept new tones (TODO: tones or
	   characters?) if number of tones in queue (queue length) is already
	   equal or larger than queue's high water mark. */
	CW_TONE_QUEUE_HIGH_WATER_MARK_MAX = 2900
};





/* Tone queue states (with totally random non-false values). */
typedef enum {
	CW_TQ_EMPTY    = 45,
	CW_TQ_NONEMPTY = 74
} cw_queue_state_t;


/* Return values from dequeue function. */
enum {
	CW_TQ_DEQUEUED        = 10,
	CW_TQ_NDEQUEUED_EMPTY = 11,
	CW_TQ_NDEQUEUED_IDLE  = 12
};





typedef struct {
	/* Frequency of a tone, in Hz. */
	int frequency;

	/* Duration of a tone, in microseconds. */
	int duration;

	/* Is this "forever" tone? See libcw_tq.c for more info about
	   "forever" tones. */
	bool is_forever;

	/* Is this the first tone of a character?
	   Used to backspace in the queue. */
	bool is_first;

	/* Type of slope. */
	int slope_mode;

	/* Duration of a tone, in samples.
	   This is a derived value, a function of duration and sample rate. */

	/* TODO: come up with thought-out, consistent type system for
	   samples count and tone duration. The type system should take into
	   consideration very long duration of tones in QRSS. */
	int64_t n_samples;

	/* Counter of samples in whole tone. */
	int sample_iterator;

	/* a tone can start and/or end abruptly (which may result in
	   audible clicks), or its beginning and/or end can have form
	   of slopes (ramps), where amplitude increases/decreases less
	   abruptly than if there were no slopes;

	   using slopes reduces audible clicks at the beginning/end of
	   tone, and can be used to shape spectrum of a tone;

	   AFAIK most desired shape of a slope looks like sine wave;
	   most simple one is just a linear slope;

	   slope area should be integral part of a tone, i.e. it shouldn't
	   make the tone longer than duration/n_samples;

	   a tone with rising and falling slope should have this length
	   (in samples):
	   rising_slope_n_samples   +   (n_samples - 2 * slope_n_samples)   +   falling_slope_n_samples

	   libcw allows following slope area scenarios (modes):
	   1. no slopes: tone shouldn't have any slope areas (i.e. tone
	      with constant amplitude);
	   1.a. a special case of this mode is silent tone - amplitude
	        of a tone is zero for whole duration of the tone;
	   2. tone has nothing more than a single slope area (rising or
	      falling); there is no area with constant amplitude;
	   3. a regular tone, with area of rising slope, then area with
	   constant amplitude, and then falling slope;

	   currently, if a tone has both slopes (rising and falling), both
	   slope areas have to have the same length; */

	int rising_slope_n_samples;     /* Number of samples on rising slope. */
	int falling_slope_n_samples;    /* Number of samples on falling slope. */
} cw_tone_t;





/* Set values of tone's fields. Some field are set with values given
   as arguments to the macro. Other are initialized with default
   values. The macro should be used like this:
   cw_tone_t my_tone;
   CW_TONE_INIT(&tone, 200, 5000, CW_SLOPE_MODE_STANDARD_SLOPES);
 */
#define CW_TONE_INIT(m_tone, m_frequency, m_duration, m_slope_mode) {	\
		(m_tone)->frequency               = m_frequency;	\
		(m_tone)->duration                = m_duration;		\
		(m_tone)->slope_mode              = m_slope_mode;	\
		(m_tone)->is_forever              = false;		\
		(m_tone)->is_first                = false;		\
		(m_tone)->n_samples               = 0;			\
		(m_tone)->sample_iterator         = 0;			\
		(m_tone)->rising_slope_n_samples  = 0;			\
		(m_tone)->falling_slope_n_samples = 0;			\
	}


/* Copy values of all fields from one variable of cw_tone_t type to
   the other. The macro accepts pointers to cw_tone_t variables as
   arguments. */
#define CW_TONE_COPY(m_dest, m_source) {				\
		(m_dest)->frequency               = (m_source)->frequency; \
		(m_dest)->duration                = (m_source)->duration; \
		(m_dest)->slope_mode              = (m_source)->slope_mode; \
		(m_dest)->is_forever              = (m_source)->is_forever; \
		(m_dest)->is_first                = (m_source)->is_first; \
		(m_dest)->n_samples               = (m_source)->n_samples; \
		(m_dest)->sample_iterator         = (m_source)->sample_iterator;	\
		(m_dest)->rising_slope_n_samples  = (m_source)->rising_slope_n_samples; \
		(m_dest)->falling_slope_n_samples = (m_source)->falling_slope_n_samples; \
	};





struct cw_gen_struct;

typedef struct {
	volatile cw_tone_t queue[CW_TONE_QUEUE_CAPACITY_MAX];

	/* Tail index of tone queue. Index of last (newest) inserted
	   tone, index of tone to be dequeued from the list as a last
	   one.

	   The index is incremented *after* adding a tone to queue. */
	volatile size_t tail;

	/* Head index of tone queue. Index of first (oldest) tone
	   inserted to the queue. Index of the tone to be dequeued
	   from the queue as a first one. */
	volatile size_t head;

	cw_queue_state_t state; /* TODO: replace this with 'bool is_empty' flag. */

	size_t capacity;
	size_t high_water_mark;
	size_t len;

	/* It's useful to have the tone queue dequeue function call
	   a client-supplied callback routine when the amount of data
	   in the queue drops below a defined low water mark.
	   This routine can then refill the buffer, as required. */
	volatile size_t low_water_mark;
	void     (* low_water_callback)(void *);
	void     * low_water_callback_arg;
	/* Set to true when conditions for calling low water callback
	   are true. The flag is set in cw_tq module, but the callback
	   itself may be called outside of the module, e.g. by cw_gen
	   code. */
	bool         call_callback;


	/* IPC */
	/* Used to broadcast queue events to waiting functions. */
	pthread_cond_t wait_var;
	pthread_mutex_t wait_mutex;

	/* Used to communicate between enqueueing and dequeueing
	   mechanism. */
	pthread_cond_t dequeue_var;
	pthread_mutex_t dequeue_mutex;


	pthread_mutex_t mutex;

	/* Generator associated with a tone queue. */
	struct cw_gen_struct * gen;

	char label[LIBCW_OBJECT_INSTANCE_LABEL_SIZE];
} cw_tone_queue_t;



cw_tone_queue_t * cw_tq_new_internal(void);
void              cw_tq_delete_internal(cw_tone_queue_t ** tq);
void              cw_tq_flush_internal(cw_tone_queue_t * tq);

size_t cw_tq_capacity_internal(const cw_tone_queue_t * tq);
size_t cw_tq_length_internal(cw_tone_queue_t * tq);
cw_ret_t cw_tq_enqueue_internal(cw_tone_queue_t * tq, const cw_tone_t * tone);
cw_ret_t cw_tq_dequeue_internal(cw_tone_queue_t * tq, cw_tone_t * tone);

cw_ret_t cw_tq_wait_for_level_internal(cw_tone_queue_t * tq, size_t level);
cw_ret_t cw_tq_register_low_level_callback_internal(cw_tone_queue_t * tq, cw_queue_low_callback_t callback_func, void * callback_arg, size_t level);
bool cw_tq_is_nonempty_internal(const cw_tone_queue_t * tq);
cw_ret_t cw_tq_wait_for_end_of_current_tone_internal(cw_tone_queue_t * tq);
void cw_tq_reset_internal(cw_tone_queue_t * tq);
bool cw_tq_is_full_internal(const cw_tone_queue_t * tq);

void cw_tq_handle_backspace_internal(cw_tone_queue_t * tq);




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef H_LIBCW_TQ */
