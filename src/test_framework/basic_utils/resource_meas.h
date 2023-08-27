#ifndef _TEST_FRAMEWORK_BASIC_UTILS_RESOURCE_MEAS_H_
#define _TEST_FRAMEWORK_BASIC_UTILS_RESOURCE_MEAS_H_




#include <pthread.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>




typedef struct {

	/* At what intervals the measurement should be taken. */
	int meas_interval_msecs;

	struct rusage rusage_prev;
	struct rusage rusage_curr;

	struct timeval timestamp_prev;
	struct timeval timestamp_curr;

	struct timeval user_cpu_diff; /* User CPU time used. */
	struct timeval sys_cpu_diff;  /* System CPU time used. */
	struct timeval summary_cpu_usage; /* User + System CPU time used. */

	struct timeval timestamp_diff;

	suseconds_t resource_usage;
	/* At what interval the last two measurements were really taken. */
	suseconds_t meas_duration;

	pthread_mutex_t mutex;
	pthread_attr_t thread_attr;
	pthread_t thread_id;

	float current_cpu_usage; /* Last calculated value of CPU usage. */
	float maximal_cpu_usage; /* Maximum detected during measurements run. */

} resource_meas;




/* Format to be used when printing CPU usage with printf(). */
#define MEAS_CPU_FMT "%05.1f%%"





/**
   @brief Start taking measurements of system resources usage

   This function also resets to zero 'max resource usage' field in
   @param meas, so that a new value can be calculated during new
   measurement.

   @reviewedon 2023.08.27

   @param[in/out] meas Resource measurement variable
   @param[in] meas_interval_msecs Value indicating at what intervals the measurements should be taken [milliseconds]

   @return 0 on successful start
   @return -1 otherwise
*/
int resource_meas_start(resource_meas * meas, int meas_interval_msecs);




/**
   @brief Stop taking measurements of system resources usage

   This function doesn't erase any measurements stored in @p meas

   @reviewedon 2023.08.21

   @param[in/out] meas Resource measurement variable
*/
void resource_meas_stop(resource_meas * meas);




/**
   @brief Get current CPU usage

   The value may change from one measurement to another - may rise and
   fall.

   @reviewedon 2023.08.21

   @param[in] meas Resource measurement variable
*/
float resource_meas_get_current_cpu_usage(resource_meas * meas);




/**
   @brief Get maximal CPU usage calculated since measurement has been started

   This function returns the highest value detected since measurement
   was started with resource_meas_start(). The value may be steady or
   may go up. The value is reset to zero each time a
   resource_meas_start() function is called.

   @reviewedon 2023.08.21

   @param[in] meas Resource measurement variable
*/
float resource_meas_get_maximal_cpu_usage(resource_meas * meas);




#endif /* #ifndef _TEST_FRAMEWORK_BASIC_UTILS_RESOURCE_MEAS_H_ */

