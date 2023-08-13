#ifndef UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H
#define UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H




#include "elements.h"




/**
   @brief A store of statistics of elements

   Stores the shortest and longest duration of all registered elements. Also
   stores average duration of all registered elements.

   Elements are registered in variables of this type with
   cw_element_stats_update().
*/
typedef struct cw_element_stats_t {
	int duration_min; /**< The shortest duration or all registered elements. */
	int duration_avg; /**< The average duration or all registered elements. */
	int duration_max; /**< The longest duration or all registered elements. */

	/* These two fields are needed for calculating of average. */
	int duration_total;  /**< Accumulated duration of all elements added to a statistics variable. */
	int count;           /**< Count of all elements added to a statistics variable. */
} cw_element_stats_t;




/**
   @brief How much the durations of real dots, dashes and spaces diverge from
   ideal ones?
*/
typedef struct cw_element_stats_divergences_t {
	double min; /**< By how many percents does the shortest duration diverges from expected value? [percentages] */
	double avg; /**< By how many percents does the average duration diverges from expected value? [percentages] */
	double max; /**< By how many percents does the longest duration diverges from expected value? [percentages] */
} cw_element_stats_divergences_t;




/**
   @brief Initialize element stats value

   Initialize members of @p stats before it can be updated with calls to cw_element_stats_update().

   @reviewedon 2023.08.12

   @param[in] stats stats value to be initialized
*/
void cw_element_stats_init(cw_element_stats_t * stats);




/**
   @brief Update given stats variable with given duration

   Call this function for a specific element (e.g. inter-mark-space) each
   time you encounter a new element and want to collect/update its
   statistics.

   @reviewedon 2023.08.12

   @param[in/out] stats Statistics variable, storing information about durations
   @param[in] element_duration New duration to use to update @p stats
*/
void cw_element_stats_update(cw_element_stats_t * stats, int element_duration);




/**
   @brief Calculate how much durations of elements diverge from expected value

   Given statistics of some elements (min/avg/max durations of e.g. an
   inter-mark-space), and given the expected (ideal) duration of a dot,
   calculate how much the min/avg/max duration diverges from expected
   duration.

   @reviewedon 2023.08.12

   @param[in] stats Statistics of an element
   @param[out] divergences Divergences to calculate
   @param[in] duration_expected Expected duration of an element
*/
void cw_element_stats_calculate_divergences(const cw_element_stats_t * stats, cw_element_stats_divergences_t * divergences, int duration_expected);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H */

