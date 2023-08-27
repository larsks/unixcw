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




#include <stdlib.h>

#include "elements.h"




/**
   \file elements.c

   A linear collection of CW elements (dots and dashes).

   The elements may have been detected in a wav file or in a string - you can
   use functions from elements_detect.c file for that purpose.
*/




void cw_elements_print_to_file(FILE * file, const cw_elements_t * elements)
{
	for (size_t i = 0; i < elements->curr_count; i++) {
		if (cw_state_mark == elements->array[i].state) {
			fprintf(file, "mark:   %11.2fus, '%c'\n", elements->array[i].timespan, cw_element_type_get_representation(elements->array[i].type));
		} else {
			fprintf(file, "space:  %11.2fus, '%c'\n", elements->array[i].timespan, cw_element_type_get_representation(elements->array[i].type));
		}
	}
}




int cw_elements_append_element(cw_elements_t * elements, cw_state_t state, cw_element_time_t timespan)
{
	if (elements->curr_count >= elements->max_count) {
		fprintf(stderr, "[ERROR] Reached limit of items in elements, can't add another item\n");
		return -1;
	}

	if (state == cw_state_mark) {
		elements->array[elements->curr_count].state = cw_state_mark;
		elements->array[elements->curr_count].timespan = timespan;
		elements->curr_count++;
		return 0;
	} else if (state == cw_state_space) {
		elements->array[elements->curr_count].state = cw_state_space;
		elements->array[elements->curr_count].timespan = timespan;
		elements->curr_count++;
		return 0;
	} else {
		return 0; /* Not strictly an error, so return success. */
	}
}




cw_elements_t * cw_elements_new(size_t count)
{
	cw_elements_t * elements = (cw_elements_t *) calloc(1, sizeof (cw_elements_t));
	if (NULL == elements) {
		fprintf(stderr, "[ERROR] Failed to allocate elements structure\n");
		return NULL;
	}

	elements->array = (cw_element_t *) calloc(count, sizeof (cw_element_t));
	if (NULL == elements->array) {
		free(elements);
		fprintf(stderr, "[ERROR] Failed to allocate elements array\n");
		return NULL;
	}

	elements->max_count = count;
	return elements;
}




void cw_elements_delete(cw_elements_t ** elements)
{
	if (NULL == elements || NULL == *elements) {
		return;
	}
	if ((*elements)->array) {
		free((*elements)->array);
		(*elements)->array = NULL;
	}
	free(*elements);
	*elements = NULL;
}




char cw_element_type_get_representation(cw_element_type_t type)
{
	switch (type) {
	case cw_element_type_dot:
		return '.';
	case cw_element_type_dash:
		return '-';
	case cw_element_type_ims:
		return 'M';
	case cw_element_type_ics:
		return 'C';
	case cw_element_type_iws:
		return 'W';
	case cw_element_type_none:
	default:
		return '?';
	}
}


