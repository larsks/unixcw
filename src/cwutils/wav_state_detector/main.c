#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>




#include "libcw/libcw.h"

#include <cwutils/lib/elements.h>
#include <cwutils/lib/elements_detect.h>
#include <cwutils/lib/wav.h>




/**
   \file main.c

   @brief Detect states (mark/space) in given wav file

   Small debug program that:
   - uses some 'elements' functions that are also used by libcw generator
     tests,
   - uses the functions to detect mark/space states in given wav file,
   - saves the mark/space states into 'elements' data structure,
   - gives possibility to confirm that the 'elements' functions work
     correctly.


  The input file for this program is a wav file because it's possible to get
  parameters of file from the wav header. I could achieve the same by passing
  information about a raw file through command-line arguments to this
  program, but that would be cumbersome.


  Usage:

  1. Compile unixcw package with enabled feature of saving samples to debug
     file:

         ./configure --enable-dev
         make

  2. Generate debug file in one of unixcw programs: just play some string in
     either cw, cwcp or xcwcp. The debug file (with raw samples) will be
     generated automatically. Its path will be printed by the program to
     console.

     Count of marks and spaces that this program can detect and record is
     limited, so don't make the string too long.

  3. Convert the raw file into wav file:

         sox -e signed-integer -b 16 -c 1 -r 44100 /tmp/cw_file_PulseAudio_44100Hz_mono_signed_16bit_pcm.raw /tmp/cw_file_PulseAudio_44100Hz_mono_signed_16bit_pcm.wav

     Adjust sample rate (44100 in above example) as necessary, depending
     on parameters used by libcw and printed by unixcw program to console.

  4. Call the program on the wav file like this:

         ./wav_state_detector /path/to/file.wav

    The program will analyse the wav file, detect states (mark/space) and
    their durations, and will generate another raw file, this time with
    square wave that corresponds to the mark/space states detected in wav
    file.

    If input wav file is /path/to/file.wav, then the output raw file will be
    /path/to/file.wav_states.raw.

  5. Open the input wav file and the output raw file in e.g. Audacity and
     compare both files.

     Confirm that square wave saved to raw file corresponds with what is
     present in input wav file.
*/




// parecord --channels=1 --format=s32le /tmp/audio_s32le.wav --file-format=wav




/**
   @brief Write given @p elements as a series of samples into raw file

   Function writes a square wave into a file. The high/low states of the wave
   correspond with mark/space states of @p elements. The function also looks
   at elements' durations, and writes as many samples as it's necessary to
   create the high/low state of proper duration. @p sample_spacing is used to
   determine how many samples per each state need to be written to file.

   Ideally there should be as many samples in the output raw file as there
   were samples in input wav file used to create @p elements.

   @reviewedon 2023.08.12

   @param[out] fd File into which to write the samples
   @param[in] elements Elements structure to write to file
   @param[in] sample_spacing Time span between samples
*/
static void write_elements_to_file(int fd, cw_elements_t * elements, cw_element_time_t sample_spacing)
{
	/* Values selected to make the high and low levels of square wave look
	   good in Audacity, compared to audio wave generated with libcw's
	   default volume of 70%. */
	const cw_sample_t high = 30000;
	const cw_sample_t low = -30000;

	for (size_t e = 0; e < elements->curr_count; e++) {
		cw_element_time_t this_element_span = 0.0;
		while (this_element_span < elements->array[e].timespan) {
			if (elements->array[e].state == cw_state_mark) {
				write(fd, &high, sizeof (high));
			} else {
				write(fd, &low, sizeof (low));
			}
			this_element_span += sample_spacing;
		}
	}
}




/**
   @reviewedon 2023.08.12
*/
int main(int argc, char * argv[])
{
	if (argc != 2) {
		fprintf(stderr, "[ERROR] Missing argument with path to input wav audio file\n");
		fprintf(stderr, "[INFO ] Run this program like this: '%s path_to_file.wav'\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	const char * wav_path = argv[1];
	int input_fd = open(wav_path, O_RDONLY);
	if (-1 == input_fd) {
		fprintf(stderr, "[ERROR] Can't open input wav file '%s': %s\n", wav_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	wav_header_t header = { 0 };
	if (0 != wav_read_header(input_fd, &header)) {
		fprintf(stderr, "[ERROR] Failed to read header of input wav file '%s'\n", wav_path);
		close(input_fd);
		return -1;
	}

	bool valid = false;
	wav_validate_header(&header, &valid);
	if (!valid) {
		fprintf(stderr, "[ERROR] Header of wav file doesn't seem to be valid\n");
		close(input_fd);
		exit(EXIT_FAILURE);
	}

	const cw_element_time_t sample_spacing = (1000.0 * 1000.0) / header.sample_rate; // [us]
	fprintf(stderr, "[INFO ] Sample rate    = %u Hz\n", header.sample_rate);
	fprintf(stderr, "[INFO ] Sample spacing = %.4f us\n", sample_spacing);

	cw_elements_t * wav_elements = cw_elements_new(1000);
	const int retval = cw_elements_detect_from_wav(input_fd, wav_elements, sample_spacing);
	close(input_fd);
	if (0 != retval) {
		fprintf(stderr, "[ERROR] Failed to detect elements in wav\n");
		cw_elements_delete(&wav_elements);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "[INFO ] Detected %zu elements in wav file\n", wav_elements->curr_count);
	/* Debug. */
	cw_elements_print_to_file(stderr, wav_elements);


	/* Write square wave representing states into new output file. The file
	   can be imported into e.g. Audacity and the square wave can be compared
	   with input file. The visual comparison done by human is a kind of
	   verification that cw_elements_detect_from_wav() works correctly. */
	char states_path[1024] = { 0 };
	snprintf(states_path, sizeof (states_path), "%s_states.raw", wav_path);
	int states_fd = open(states_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (-1 == states_fd) {
		fprintf(stderr, "[ERROR] Failed to open output raw file '%s': %s\n", states_path, strerror(errno));
		cw_elements_delete(&wav_elements);
		exit(EXIT_FAILURE);
	}
	write_elements_to_file(states_fd, wav_elements, sample_spacing);
	close(states_fd);

	cw_elements_delete(&wav_elements);
	exit(EXIT_SUCCESS);
}


