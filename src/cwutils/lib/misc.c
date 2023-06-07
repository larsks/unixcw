#include <stdio.h>


#include "misc.h"

/**
   Get ideal (expected) duration of given element (dot, dash, spaces)
*/
int ideal_duration_of_element(cw_element_type_t type, cw_durations_t * durations)
{
	switch (type) {
	case dot:
		return durations->dot_usecs;
	case dash:
		return durations->dash_usecs;
	case ims:
		return durations->ims_usecs;
	case ics:
		return durations->ics_usecs;
	case iws:
		return durations->iws_usecs;
	default:
		fprintf(stderr, "[ERROR] Unexpected element type '%c'\n", type);
		return 1;
	}
}





void calculate_divergences_from_stats(const cw_element_stats_t * stats, cw_duration_divergence_t * divergences, int duration_expected)
{
	divergences->min = 100.0 * (stats->duration_min - duration_expected) / (1.0 * duration_expected);
	divergences->avg = 100.0 * (stats->duration_avg - duration_expected) / (1.0 * duration_expected);
	divergences->max = 100.0 * (stats->duration_max - duration_expected) / (1.0 * duration_expected);
}




void cw_durations_print(FILE * file, cw_durations_t * durations)
{
	fprintf(file, "[INFO ] dot duration        = %7d us\n", durations->dot_usecs);
	fprintf(file, "[INFO ] dash duration       = %7d us\n", durations->dash_usecs);
	fprintf(file, "[INFO ] ims duration        = %7d us\n", durations->ims_usecs);
	fprintf(file, "[INFO ] ics duration        = %7d us\n", durations->ics_usecs);
	fprintf(file, "[INFO ] iws duration        = %7d us\n", durations->iws_usecs);
	fprintf(file, "[INFO ] additional duration = %7d us\n", durations->additional_usecs);
	fprintf(file, "[INFO ] adjustment duration = %7d us\n", durations->adjustment_usecs);
}

