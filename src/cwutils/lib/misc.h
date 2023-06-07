#ifndef UNIXCW_CWUTILS_LIB_MISC_H
#define UNIXCW_CWUTILS_LIB_MISC_H


#include <stdio.h>


#include "libcw_gen.h"
#include "elements.h"




/* How much durations of real dots, dashes and spaces diverge from ideal
   ones? */
typedef struct cw_duration_divergence_t {
	double min; /* [percentages] */
	double avg; /* [percentages] */
	double max; /* [percentages] */
} cw_duration_divergence_t;




int ideal_duration_of_element(cw_element_type_t type, cw_durations_t * durations);
void calculate_divergences_from_stats(const cw_element_stats_t * stats, cw_duration_divergence_t * divergences, int duration_expected);
void cw_durations_print(FILE * file, cw_durations_t * durations);



#endif /* #ifndef UNIXCW_CWUTILS_LIB_MISC_H */

