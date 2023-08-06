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




/**
   Get ideal (expected) duration of given element (dot, dash, spaces)
*/
int ideal_duration_of_element(cw_element_type_t type, cw_gen_durations_t * durations)
{
	switch (type) {
	case cw_element_type_dot:
		return durations->dot_duration;
	case cw_element_type_dash:
		return durations->dash_duration;
	case cw_element_type_ims:
		return durations->ims_duration;
	case cw_element_type_ics:
		return durations->ics_duration;
	case cw_element_type_iws:
		return durations->iws_duration;
	case cw_element_type_none:
	default:
		fprintf(stderr, "[ERROR] Unexpected element type '%c'\n", type);
		return 3210; /* Some recognizable integer. TODO: use -1*/
	}
}




void cw_durations_print(FILE * file, cw_gen_durations_t * durations)
{
	fprintf(file, "[INFO ] dot duration        = %7d us\n", durations->dot_duration);
	fprintf(file, "[INFO ] dash duration       = %7d us\n", durations->dash_duration);
	fprintf(file, "[INFO ] ims duration        = %7d us\n", durations->ims_duration);
	fprintf(file, "[INFO ] ics duration        = %7d us\n", durations->ics_duration);
	fprintf(file, "[INFO ] iws duration        = %7d us\n", durations->iws_duration);
	fprintf(file, "[INFO ] additional duration = %7d us\n", durations->additional_space_duration);
	fprintf(file, "[INFO ] adjustment duration = %7d us\n", durations->adjustment_space_duration);
}

