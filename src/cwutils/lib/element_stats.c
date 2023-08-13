/*
  Copyright (C) 2023  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program. If not, see <https://www.gnu.org/licenses/>.
*/




#include "element_stats.h"




void cw_element_stats_update(cw_element_stats_t * stats, int element_duration)
{
	/* TODO (acerion) 2023.08.12: check for possible overflow. */

	stats->duration_total += element_duration;
	stats->count++;
	stats->duration_avg = stats->duration_total / stats->count;

	if (element_duration > stats->duration_max) {
		stats->duration_max = element_duration;
	}
	if (element_duration < stats->duration_min) {
		stats->duration_min = element_duration;
	}
}




void cw_element_stats_calculate_divergences(const cw_element_stats_t * stats, cw_element_stats_divergences_t * divergences, int duration_expected)
{
	divergences->min = 100.0 * (stats->duration_min - duration_expected) / (1.0 * duration_expected);
	divergences->avg = 100.0 * (stats->duration_avg - duration_expected) / (1.0 * duration_expected);
	divergences->max = 100.0 * (stats->duration_max - duration_expected) / (1.0 * duration_expected);
}




void cw_element_stats_init(cw_element_stats_t * stats)
{
	stats->duration_min = INT_MAX;
	stats->duration_avg = 0;
	stats->duration_max = INT_MIN;
	stats->duration_total = 0;
	stats->count = 0;
}

