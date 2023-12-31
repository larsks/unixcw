#ifndef _TEST_FRAMEWORK_BASIC_UTILS_PARAM_RANGER_H_
#define _TEST_FRAMEWORK_BASIC_UTILS_PARAM_RANGER_H_




#include <stdbool.h>
#include <sys/time.h>
#include <sys/types.h>




/**
   Direction in which values returned by calls to _get_next() will go:
   will they increase, will they decrease or will they stay on
   constant level (plateau) for few calls to _get_next().

   Using bits to mark direction because once we reach plateau, we have to
   remember in which direction to go once we get off from the plateau.

   TODO acerion 2023.08.21: using bits to encode the direction after leaving
   a plateau may be unnecessary: we know where to go after leaving a plateau
   because we can check current value (ranger::previous_value) and from that
   decide if we can/go up or down from that value.
*/
typedef enum cwtest_param_ranger_direction {
	cwtest_param_ranger_direction_up      = 0x01,
	cwtest_param_ranger_direction_down    = 0x02,
	cwtest_param_ranger_direction_plateau = 0x04
} cwtest_param_ranger_direction;




/**
   Object for obtaining varying values of some integer parameter from
   specified range on each call to _get_next() function.

   Currently the returned values can change linearly up and down
   between min and max. Possibly in the future the ranger will support
   modes other than linear: random, sine, or other.
*/
typedef struct cwtest_param_ranger_t {
	/* Minimal value of parameter values generated. */
	int range_min;

	/* Maximal value of parameter values generated. */
	int range_max;

	/* By how much the returned value changes on each successful
	   call to _get_next(). */
	int step;

	/* Internal helper variable. Value returned by previous call
	   to _get_next(), used in calculating new value returned by
	   _get_next(). */
	int previous_value;

	/* Internal helper variable. In linear generation method: flag
	   that dictates if values returned by _get_next() are
	   linearly increasing or decreasing. */
	cwtest_param_ranger_direction direction;

	/* Internal helper variable. Timestamp at which previous new
	   value was returned by _get_next() */
	time_t previous_timestamp;

	/* Time interval at which new values are calculated and
	   returned by _get_next(). If time between previous_timestamp
	   and current time is less than interval_sec, _get_next()
	   will return false. */
	time_t interval_sec;

	/* When parameter reaches minimum or maximum value, how many
	   next calls to _get_next() should return the same min/max
	   value (how many calls to _get_next() should stay on the
	   plateau)?

	   Set to zero to disable this feature.

	   Value is zero by default.

	   This is a bit fuzzy parameter, with possible off-by-one
	   error. Don't set it to 1 or 2 or such small value and
	   expect _get_next() to return min/max values exactly 1 or 2
	   times. Set it to 10 or 20, and expect approximately 10 or
	   approximately 20 calls to _get_next() to return min/max.

	   Unit: times (successful calls to _get_next() that return
	   min/max).
	*/
	int plateau_length;

	/* Internal helper variable. If plateau_length is non-zero,
	   how many next calls to _get_next() should return previous
	   (minimal or maximal) value? */
	int plateau_remaining;
} cwtest_param_ranger_t;




/**
   @brief Initialize @p ranger

   @reviewedon 2023.08.23

   @param[out] ranger Ranger to initialize
   @param[in] min Minimal value of range of values returned by _get_next()
   @param[in] max Maximal value of range of values returned by _get_next()
   @param[in] step By how much the returned value changes on each successful call to _get_next()
   @param[in] initial_value Initial parameter value stored by @p ranger and used to calculate first value returned by _get_next()
*/
void cwtest_param_ranger_init(cwtest_param_ranger_t * ranger, int min, int max, int step, int initial_value);




/**
   @brief Configure ranger to generate new value only if specific
   interval has passed since previous successful call to _get_next()

   If @p interval_sec is non-zero, calling _get_next() on @p ranger will
   return new value only if at least @p interval_sec seconds passed
   since last successful call.

   If you have some control loop executed every 100ms, and you want to
   be able to operate on @p ranger in this loop, but want to get
   values less frequently than every 100ms, you can configure desired
   time interval for @p ranger with this function. The calls to
   _get_next() will then return success and new value each @p
   interval_sec.

   Pass zero value of @p interval_sec to disable this feature for @p
   ranger.

   TODO acerion 2023.08.21: in theory we could want to have sub-second ranger
   intervals.

   @reviewedon 2023.08.23

   @param[in/out] ranger Ranger to configure
   @param[in] interval_sec At what intervals the call to _get_next() will return true and will return next value of parameter
*/
void cwtest_param_ranger_set_interval_sec(cwtest_param_ranger_t * ranger, time_t interval_sec);




/**
   @brief Configure plateau length for ranger

   When value calculated by ranger reaches min or max, then next
   (approximately) N calls to _get_next() will return the same value equal to
   min or max. N is equal to @p plateau_length. 'approximately' because I
   didn't make sure that the plateau is exactly equal to N. It may be N+1, or
   it may be N-1.

   The values returned by _get_next() will stay on that 'plateau' for
   approximately @p plateau_length calls. After approximately @p
   plateau_length calls, the values returned by _get_next() will leave the
   plateau and will start to change again.

   Pass zero value of @p plateau_length to disable this feature for @p
   ranger.

   @reviewedon 2023.08.23

   @param[in/out] ranger Ranger to configure
   @param[in] plateau_length How many calls to _get_next() will return non-changing min or max value, when the min/max is reached
*/
void cwtest_param_ranger_set_plateau_length(cwtest_param_ranger_t * ranger, int plateau_length);




/**
   @brief Get next value from @p ranger

   On successful call function returns true, and new value is returned
   through @p new_value. Otherwise false is returned.

   If @param ranger is configured to use intervals (with
   cwtest_param_ranger_set_interval_sec()), only calls that are
   separated by at least given time interval will return
   true.

   @if @param ranger is not configured to use intervals, then each call to
   this function will be successful.

   @reviewedon 2023.08.23

   @param[in/out] ranger Ranger from which to get next value
   @param[out] new_value New value returned by ranger

   @return true if ranger has returned new value through @p new_value
   @return false otherwise
*/
bool cwtest_param_ranger_get_next(cwtest_param_ranger_t * ranger, int * new_value);




#endif /* #ifndef _TEST_FRAMEWORK_BASIC_UTILS_PARAM_RANGER_H_ */

