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




#include <stdio.h>

#include "misc.h"




int cw_element_type_to_duration(cw_element_type_t type, const cw_gen_durations_t * durations, int * duration)
{
	switch (type) {
	case cw_element_type_dot:
		*duration = durations->dot_duration;
		return 0;

	case cw_element_type_dash:
		*duration = durations->dash_duration;
		return 0;

	case cw_element_type_ims:
		*duration = durations->ims_duration;
		return 0;

	case cw_element_type_ics:
		*duration = durations->ics_duration;
		return 0;

	case cw_element_type_iws:
		*duration = durations->iws_duration;
		return 0;

	case cw_element_type_none:
	default:
		fprintf(stderr, "[ERROR] Unexpected element type '%c'\n", type);
		return -1;
	}
}




void cw_gen_durations_print(FILE * file, const cw_gen_durations_t * durations)
{
#define CWPRIduration "7d"
	fprintf(file, "[INFO ] dot duration        = %" CWPRIduration " us\n", durations->dot_duration);
	fprintf(file, "[INFO ] dash duration       = %" CWPRIduration " us\n", durations->dash_duration);
	fprintf(file, "[INFO ] ims duration        = %" CWPRIduration " us\n", durations->ims_duration);
	fprintf(file, "[INFO ] ics duration        = %" CWPRIduration " us\n", durations->ics_duration);
	fprintf(file, "[INFO ] iws duration        = %" CWPRIduration " us\n", durations->iws_duration);
	fprintf(file, "[INFO ] additional duration = %" CWPRIduration " us\n", durations->additional_space_duration);
	fprintf(file, "[INFO ] adjustment duration = %" CWPRIduration " us\n", durations->adjustment_space_duration);
}

