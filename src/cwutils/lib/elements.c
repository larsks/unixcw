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

#include <libcw_data.h>

#include "elements.h"




void cw_elements_print_to_file(FILE * file, cw_elements_t * elements)
{
	for (size_t i = 0; i < elements->curr_count; i++) {
		if (cw_state_mark == elements->array[i].state) {
			fprintf(file, "mark:   %11.2fus, '%c'\n", (double) elements->array[i].duration, elements->array[i].type);
		} else {
			fprintf(file, "space:  %11.2fus, '%c'\n", (double) elements->array[i].duration, elements->array[i].type);
		}
	}
}




int cw_elements_append_element(cw_elements_t * elements, cw_state_t state, cw_element_time_t duration)
{
	if (elements->curr_count >= elements->max_count) {
		fprintf(stderr, "[ERROR] Reached limit of items in elements, can't add another item\n");
		return -1;
	}

	if (state == cw_state_mark) {
		elements->array[elements->curr_count].state = cw_state_mark;
		elements->array[elements->curr_count].duration = duration;
		elements->curr_count++;
		return 0;
	} else if (state == cw_state_space) {
		elements->array[elements->curr_count].state = cw_state_space;
		elements->array[elements->curr_count].duration = duration;
		elements->curr_count++;
		return 0;
	} else {
		return 0; /* Not strictly an error, so return success. */
	}
}




int cw_elements_from_string(const char * string, cw_elements_t * elements)
{
	size_t e = 0;

	int s = 0;
	while (string[s] != '\0') {

		if (string[s] == ' ') {
			/* ' ' character is represented by iws. This is a special case
			   because this character doesn't have its "natural"
			   representation in form of dots and dashes. */
			if (e > 0 && (elements->array[e - 1].type == cw_element_type_ims || elements->array[e - 1].type == cw_element_type_ics)) {
				/* Overwrite last end-of-element. */
				elements->array[e - 1].type = cw_element_type_iws;
				elements->array[e - 1].state = cw_state_space;
				/* No need to increment 'e' as we are not adding new element. */
			} else {
				elements->array[e].type = cw_element_type_iws;
				elements->array[e].state = cw_state_space;
				e++;
			}
		} else {
			/* Regular (non-space) character has its Morse representation.
			   Get the representation, and copy each dot/dash into
			   'elements'. Add ims after each dot/dash. */
			const char * representation = cw_character_to_representation_internal(string[s]);
			int r = 0;
			while (representation[r] != '\0') {
				switch (representation[r]) {
				case '.':
					elements->array[e].type = cw_element_type_dot;
					elements->array[e].state = cw_state_mark;
					break;
				case '-':
					elements->array[e].type = cw_element_type_dash;
					elements->array[e].state = cw_state_mark;
					break;
				default:
					break;
				};
				r++;
				e++;
				elements->array[e].type = cw_element_type_ims;
				elements->array[e].state = cw_state_space;
				e++;

			}
			/* Turn ims after last mark (the last mark in character) into ics. */
			elements->array[e - 1].type = cw_element_type_ics;
			elements->array[e - 1].state = cw_state_space;
		}
		s++;
	}

	if (e > elements->max_count) {
		fprintf(stderr, "[ERROR] Count of elements (%zd) exceeds available space (%zd)\n", e, elements->max_count);
		return -1;
	}
	elements->curr_count = e;

#if 0 /* For debugging only. */
	for (int i = 0; i < elements->curr_count; i++) {
		fprintf(stderr, "[DEBUG] Initialized element %3d with type '%c'\n", i, elements->array[i].type);
	}
#endif

	return 0;
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


