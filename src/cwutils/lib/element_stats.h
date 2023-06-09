#ifndef UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H
#define UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H




#include "elements.h"




/**
   @brief A store of statistics of elements

   Stores the shortest and longest duration of all registered elements. Also
   stores average duration of all registered elements.

   Elements are registered in variables of this type with
   element_stats_update().
*/
typedef struct cw_element_stats_t {
	int duration_min; /**< The shortest duration or all registered elements. */
	int duration_avg; /**< The average duration or all registered elements. */
	int duration_max; /**< The longest duration or all registered elements. */

	/* These two fields are needed for calculating of average. */
	int duration_total; /**< Accumulated duration of all elements added to a statistics variable. */
	int count;          /**< Count of all elements added to a statistics variable. */
} cw_element_stats_t;





/**
   @brief How much the durations of real dots, dashes and spaces diverge from
   ideal ones? */
typedef struct cw_element_stats_divergences_t {
	double min; /**< By how many percents does the shortest duration diverges from expected value? [percentages] */
	double avg; /**< By how many percents does the average duration diverges from expected value? [percentages] */
	double max; /**< By how many percents does the longest duration diverges from expected value? [percentages] */
} cw_element_stats_divergences_t;




/**
   @brief Update given stats variable with given duration

   @param stats Statistics variable, storing information about durations
   @param element_duration New duration to use to update @p stats
*/
void element_stats_update(cw_element_stats_t * stats, int element_duration);
void element_stats_calculate_divergences(const cw_element_stats_t * stats, cw_element_stats_divergences_t * divergences, int duration_expected);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENT_STATS_H */

