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




#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <libcw.h>
#include <libcw_data.h>

#include "elements.h"
#include "elements_detect.h"




/**
   @file elements_detect.c

   Detect elements (their duration and their mark/space state) in a wav file.
*/




/*
  How many consecutive samples are used to recognize mark or space?

  N consecutive zero samples mean space. N consecutive non-zero samples mean
  mark (a sine wave crossing a zero, with zero value of a sample is a bad
  luck). Anything in between is "can't decide".
*/
#define STATE_MEMORY_SIZE 4
typedef struct state_memory_t {
	/* int32_t allows me to use INT32_MAX as "sample not initialized yet"
	   value. Samples are int16_t, so INT32_MAX can't occur as a valid value
	   of sample. */
	int32_t samples[STATE_MEMORY_SIZE];

	/* How many samples to consider when detecting state? This value could be
	   dynamically changed in the future: could be larger at the beginning,
	   and later decreased. */
	int compare_count;
} state_memory_t;




static bool state_detect(state_memory_t * memory, cw_sample_t sample, cw_state_t * state);
static void state_init_memory(state_memory_t * memory);





int cw_elements_detect_from_wav(int input_fd, cw_elements_t * elements, cw_element_time_t sample_spacing)
{
	/* Time stamp of start of previous element. Zero time stamp is at the
	   beginning of pcm file. */
	cw_element_time_t prev_element_start_ts = 0.0;

	cw_state_t prev_state = cw_state_space;
	cw_state_t current_state = cw_state_space;

	size_t sample_i = 0;
	cw_sample_t sample = 0;
	bool beginning_of_file = true;

	state_memory_t state_memory;
	state_init_memory(&state_memory);

	while (0 != read(input_fd, &sample, sizeof (sample))) {
		bool detected = state_detect(&state_memory, sample, &current_state);
		if (!detected) {
			/* Current state of wave in samples can't be detected. Either we
			   are at the beginning of file, or wave is in transition between
			   mark/space. */
			sample_i++;
			continue;
		}

		if (beginning_of_file) {
			/* Special case for beginning of file. */
			fprintf(stderr, "[DEBUG] Detected initial state %s\n", current_state == cw_state_mark ? "mark" : "space");

			beginning_of_file = false;
			const cw_element_time_t current_timestamp = sample_i * sample_spacing;

			prev_element_start_ts = current_timestamp;
			prev_state = current_state;
		} else {
			if (current_state != prev_state) {
				fprintf(stderr, "[DEBUG] Detected transition to %s\n", current_state == cw_state_mark ? "mark" : "space");

				/* We have just detected change of state. We now know how
				   long the previous state lasted, and we need to save
				   the duration of the previous state, and the previous
				   state itself. Therefore we pass 'prev_state' to
				   cw_elements_append_element() below. */
				const cw_element_time_t current_timestamp = sample_i * sample_spacing;
				const cw_element_time_t prev_duration = current_timestamp - prev_element_start_ts;
				if (0 != cw_elements_append_element(elements, prev_state, prev_duration)) {
					fprintf(stderr, "[ERROR] Failed to append element from wav\n");
					return -1;
				}

				prev_element_start_ts = current_timestamp;
				prev_state = current_state;
			}
		}
		sample_i++;
	}

	/* Special case for end of file. Current state and its duration was never
	   saved (because in the loop we always saved previous state). Now we
	   have to save the last element found in file - the current state and
	   its duration. The file has just ended, and so the current element
	   ends. This ending of current element must be reflected in
	   'elements'. */
	const cw_element_time_t current_timestamp = sample_i * sample_spacing; /* TODO: "sample_i" or "sample_i - 1"? */
	const cw_element_time_t current_duration = current_timestamp - prev_element_start_ts;
	if (0 != cw_elements_append_element(elements, current_state, current_duration)) {
		fprintf(stderr, "[ERROR] Failed to append last element from wav\n");
		return -1;
	}

	return 0;
}




int cw_elements_detect_from_string(const char * string, cw_elements_t * elements)
{
	size_t e = 0;

	int s = 0;
	while (string[s] != '\0') {

		if (string[s] == ' ') {
			/* ' ' character is represented by iws. This is a special case
			   because this character doesn't have its "natural"
			   representation in form of dots and dashes. */
			if (e > 0 && (elements->array[e - 1].type == cw_element_type_ims || elements->array[e - 1].type == cw_element_type_ics)) {
				/* Overwrite last end-of-element: space changes its type to
				   iws */
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
	for (size_t i = 0; i < elements->curr_count; i++) {
		fprintf(stderr, "[DEBUG] Initialized element %3zd with type '%c'\n", i, cw_element_type_get_representation(elements->array[i].type));
	}
#endif

	return 0;
}




/**
   @brief Detect current state of sound in set of samples

   Look at current @p sample, look at past samples saved in @p memory, and
   decide if there is no sound (@p state will be set cw_state_space) or a
   sound (@p will be set to CW_SPACE_MARK).

   Function may be unable to detect either mark or space - this can happen if
   @p memory didn't accumulate enough data, or if @p memory together with @p
   sample indicate that a samples are transitioning between silence and
   sound.

   @reviewedon 2023.08.12

   @param[in/out] memory memory of past samples, updated by this function
   @param[in] sample current sample to be used together with @p memory in evaluation of sound in samples
   @param[out] state state of sound, will be set if function will detect a mark or space.

   @return true if some state has been detected (@p state is updated accordingly)
   @return false otherwise (@p state is not updated)
*/
static bool state_detect(state_memory_t * memory, cw_sample_t sample, cw_state_t * state)
{
	/* Update set of samples used for detection. Put latest sample on the
	   front (from the left).

	   Always update all STATE_MEMORY_SIZE samples, regardless of value of
	   state_memory_t::compare_count. */
	int i = 0;
	for (i = STATE_MEMORY_SIZE - 1; i > 0; i--) {
		memory->samples[i] = memory->samples[i - 1];
	}
	memory->samples[i] = sample;


	/* See if we have enough data to detect state. */
	for (i = 0; i < memory->compare_count; i++) {
		if (INT32_MAX == memory->samples[i]) {
			/* We are still in the initial state. There is not enough valid
			   accumulated samples to decide if state is 'mark' or
			   'space'. */
			return false;
		}
	}

	int zeros = 0;
	int non_zeros = 0;
	for (i = 0; i <memory->compare_count; i++) {
		if (0 == memory->samples[i]) {
			zeros++;
		} else {
			non_zeros++;
		}
	}

	if (memory->compare_count == zeros) {
		*state = cw_state_space; /* N consecutive zeros mean a space. */
		return true;
	}
	if (memory->compare_count == non_zeros) {
		*state = cw_state_mark; /* N consecutive non-zeros mean a mark. */
		return true;
	}

	return false; /* mark/space state was not recognized. */
}




/**
   @brief Initialize 'state memory' data structure

   Initialize the structure that will be later passed to
   state_detect() in a loop.

   @param[in/out] memory data structure to initialize
*/
static void state_init_memory(state_memory_t * memory)
{
	for (int i = 0; i < STATE_MEMORY_SIZE; i++) {
		memory->samples[i] = INT32_MAX;
	}

	memory->compare_count = STATE_MEMORY_SIZE;
}

