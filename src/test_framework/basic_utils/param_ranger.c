/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)

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




/**
   @file param_ranger.c

   Generate numeric values of some parameter that are within specified range.

   A range of values may be useful when trying to test an object that can be
   configured with different integer values of some parameter.

   Example: frequency (tone) of generated sound: generator of sound may
   support frequencies in range Fmin to Fmax. Test code may configure the
   tested generator to work with a series of frequencies within the range.
*/




#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libcw_utils.h>

#include <tests/test_framework.h>
#include "param_ranger.h"




void cwtest_param_ranger_init(cwtest_param_ranger_t * ranger, int min, int max, int step, int initial_value)
{
	ranger->range_min = min;
	ranger->range_max = max;
	ranger->step = step;
	ranger->previous_value = initial_value;

	ranger->plateau_length = 0;

	if (initial_value == ranger->range_max) {
		/* Value in a ranger is already at max, so values generated by ranger
		   can only go 'down' from here. */
		ranger->direction = cwtest_param_ranger_direction_down;
	} else {
		ranger->direction = cwtest_param_ranger_direction_up;
	}
}




bool cwtest_param_ranger_get_next(cwtest_param_ranger_t * ranger, int * new_value)
{
	FILE * file = stderr;

	if (ranger->interval_sec) {
		/* Generate new value only after specific time interval has passed
		   since last value was returned. */
		const time_t now_timestamp = time(NULL);
		if (now_timestamp < ranger->previous_timestamp + ranger->interval_sec) {
			/* Don't generate new value yet. */
			return false;
		}
		ranger->previous_timestamp = now_timestamp;
		/* Go to code that will calculate new value. */
	}


	int val = 0;
	if (ranger->direction == cwtest_param_ranger_direction_up) {
		val = ranger->previous_value + ranger->step;
		if (val >= ranger->range_max) {
			val = ranger->range_max;
			ranger->direction = cwtest_param_ranger_direction_down; /* Starting with next call, start returning decreasing values. */

			if (0 != ranger->plateau_length) {
				fprintf(file, "[DEBUG] Entering 'maximum' plateau, value = %d\n", ranger->range_max);
				ranger->direction |= cwtest_param_ranger_direction_plateau;
				ranger->plateau_remaining = ranger->plateau_length;
			}
		}

	} else if (ranger->direction == cwtest_param_ranger_direction_down) {
		val = ranger->previous_value - ranger->step;
		if (val <= ranger->range_min) {
			val = ranger->range_min;
			ranger->direction = cwtest_param_ranger_direction_up; /* Starting with next call, start returning increasing values. */

			if (0 != ranger->plateau_length) {
				fprintf(file, "[DEBUG] Entering 'minimum' plateau, value = %d\n", ranger->range_min);
				ranger->direction |= cwtest_param_ranger_direction_plateau;
				ranger->plateau_remaining = ranger->plateau_length;
			}
		}

	} else if (ranger->direction & cwtest_param_ranger_direction_plateau) {
		/* Will return the same value as previously. */
		val = ranger->previous_value;

		if (ranger->plateau_remaining > 0) {
			fprintf(file, "[DEBUG] On plateau, remaining %d\n", ranger->plateau_remaining);
			ranger->plateau_remaining--;
		} else {
			/* Leave the plateau. Bit indicating direction
			   up or direction down will be read and used
			   in next function call. */
			fprintf(file, "[DEBUG] Leaving plateau\n");
			ranger->direction &= (~cwtest_param_ranger_direction_plateau);
		}

	} else {
		fprintf(file, "[ERROR] Unhandled direction %02x\n", ranger->direction);
		return false;
	}


	ranger->previous_value = val;

	fprintf(file, "[DEBUG] Returning new parameter value %d\n", val);
	*new_value = val;

	return true;
}




void cwtest_param_ranger_set_interval_sec(cwtest_param_ranger_t * ranger, time_t interval_sec)
{
	if (interval_sec) {
		ranger->previous_timestamp = time(NULL);
		ranger->interval_sec = interval_sec;
	} else {
		ranger->previous_timestamp = 0;
		ranger->interval_sec = 0;
	}
}




void cwtest_param_ranger_set_plateau_length(cwtest_param_ranger_t * ranger, int plateau_length)
{
	ranger->plateau_length = plateau_length;
}



