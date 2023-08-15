#ifndef H_CW_EASY_LEGACY_RECEIVER_INTERNAL
#define H_CW_EASY_LEGACY_RECEIVER_INTERNAL




#include <stdbool.h>
#include <sys/time.h>




#if defined(__cplusplus)
extern "C"
{
#endif




/**
   @brief Easy receiver to be used in programs that use legacy libcw API
*/
struct cw_easy_legacy_receiver_t {
	/* Timer for measuring length of dots and dashes.

	   Initial value of the timestamp is created by xcwcp's receiver on
	   first "paddle down" event in a character. The timestamp is then
	   updated by libcw on specific time intervals. The intervals are a
	   function of keyboard key presses or mouse button presses recorded
	   by xcwcp. */
	struct timeval main_timer;

	/* Safety flag to ensure that we keep the library in sync with keyer
	   events. Without, there's a chance that of a on-off event, one half
	   will go to one application instance, and the other to another
	   instance. */
	volatile bool tracked_key_state;

	/* Flag indicating if receive polling has received a character, and
	   may need to augment it with a word space on a later poll. */
	volatile bool is_pending_iws;

	/* Flag indicating possible receive errno detected in signal handler
	   context and needing to be passed to the foreground. */
	volatile int libcw_receive_errno;

	/* State of left and right paddle of iambic keyer. The
	   flags are common for keying with keyboard keys and
	   with mouse buttons.

	   A timestamp for libcw needs to be generated only in
	   situations when one of the paddles comes down and
	   the other is up. This is why we observe state of
	   both paddles separately. */
	bool is_left_down;
	bool is_right_down;

	/* Whether to get a representation or a character from receiver's
	   internals with libcw low-level API. */
	bool get_representation;

	void * rec_tester;
};




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef H_CW_EASY_LEGACY_RECEIVER_INTERNAL */

