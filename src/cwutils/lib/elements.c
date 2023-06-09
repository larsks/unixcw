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




/**
   Clear data accumulated in current test run. The function should be used as
   a preparation for next test run.
*/
void elements_clear_durations(cw_element_t * elements, int count)
{
	/* Clear durations calculated in current test before next call of
	   this test function. */
	for (int e = 0; e < count; e++) {
		elements[e].duration = 0.0F;
	}
}





void elements_print_to_file(FILE * file, cw_element_t * elements, int count)
{
	for (int i = 0; i < count; i++) {
		if (CW_STATE_MARK == elements[i].state) {
			fprintf(file, "mark:   %11.2fus, '%c'\n", (double) elements[i].duration, elements[i].type);
		} else {
			fprintf(file, "space:  %11.2fus, '%c'\n", (double) elements[i].duration, elements[i].type);
		}
	}
}




void elements_append_new(cw_element_t * elements, int * elements_iter, cw_state_t state, cw_element_time_t duration)
{
	if (state == CW_STATE_MARK) {
		elements[*elements_iter].state = CW_STATE_MARK;
		elements[*elements_iter].duration = duration;
		(*elements_iter)++;
	} else if (state == CW_STATE_SPACE) {
		elements[*elements_iter].state = CW_STATE_SPACE;
		elements[*elements_iter].duration = duration;
		(*elements_iter)++;
	} else {
		;
	}
}




/**
   Convert given string into elements

   Function sets 'type' and 'state' in @p elements' items

   @param[in] string string to be used as input of tests
   @param[out] elements array of marks, spaces, their types and their timings
   @param[in] array_size total count of slots in @p elements array
*/
int elements_from_string(const char * string, cw_element_t * elements, int array_size)
{
	int e = 0;

	int s = 0;
	while (string[s] != '\0') {

		if (string[s] == ' ') {
			/* ' ' character is represented by iws. This is a special case
			   because this character doesn't have its "natural"
			   representation in form of dots and dashes. */
			if (e > 0 && (elements[e - 1].type == ims || elements[e - 1].type == ics)) {
				/* Overwrite last end-of-element. */
				elements[e - 1].type = iws;
				elements[e - 1].state = CW_STATE_SPACE;
				/* No need to increment 'e' as we are not adding new element. */
			} else {
				elements[e].type = iws;
				elements[e].state = CW_STATE_SPACE;
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
					elements[e].type = dot;
					elements[e].state = CW_STATE_MARK;
					break;
				case '-':
					elements[e].type = dash;
					elements[e].state = CW_STATE_MARK;
					break;
				default:
					break;
				};
				r++;
				e++;
				elements[e].type = ims;
				elements[e].state = CW_STATE_SPACE;
				e++;

			}
			/* Turn ims after last mark (the last mark in character) into ics. */
			elements[e - 1].type = ics;
			elements[e - 1].state = CW_STATE_SPACE;
		}
		s++;
	}

	if (e > array_size) {
		fprintf(stderr, "[ERROR] Count of elements (%d) exceeds available space (%d)\n", e, array_size);
		exit(EXIT_FAILURE);
	}
	const int elements_count = e;

#if 0 /* For debugging only. */
	for (int i = 0; i < elements_count; i++) {
		fprintf(stderr, "[DEBUG] Initialized element %3d with type '%c'\n", i, elements[i].type);
	}
#endif

	return elements_count;
}



