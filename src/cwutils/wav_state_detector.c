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



#include <libcw.h>

#include "lib/elements.h"
#include "lib/elements_detect.h"
#include "lib/wav.h"





/**
   @brief Detect states (mark/space) in given wav file

   Small debug program that:
   - uses some 'elements' functions that are also used by libcw generator
     tests,
   - uses the functions to detect mark/space states in given wav file,
   - saves the mark/space states into 'elements' data structures,
   - gives possibility to confirm that the 'elements' functions work
     correctly.


  The input file for this program is a wav file because it's possible to get
  parameters of file from the wav header. I could achieve the same by passing
  command-line arguments to this program, but...


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

     Adjust sample rate (44100 in above example) as necessary.

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
   @brief Print elements' states and durations to file

   @param[out] file File to write to
   @param[in] elements Elements to write to @p file
   @param[int] count Count of elements to write
*/
static void print_elements_to_file(FILE * file, cw_element_t * elements, int count)
{
	for (int i = 0; i < count; i++) {
		if (CW_STATE_MARK == elements[i].state) {
			fprintf(file, "mark:  %8d\n", elements[i].duration);
		} else {
			fprintf(file, "space: %8d\n", elements[i].duration);
		}
	}
}




/**
   @brief Write given @p elements as a series of samples into raw file

   Function writes a square wave into a file. The high/low states of the wave
   correspond with mark/space states of @p elements. The function also looks
   at elements' durations, and writes as many samples as it's necessary to
   create the high/low state of proper duration. @p sample_spacing is used to
   determine how many samples per each state need to be written to file.

   Ideally there should be as many samples in the output raw file as there
   were samples in input wav file used to create @p elements.

   @param[out] fd File into which to write the samples
   @param[in] elements Elements to write to file
   @param[in] elements_count Count of elements to write to file
   @param[in] sample_spacing Time span between samples
*/
static void write_elements_to_file(int fd, cw_element_t * elements, int elements_count, float sample_spacing)
{
	const cw_sample_t high = 30000;
	const cw_sample_t low = -30000;
	for (int e = 0; e < elements_count; e++) {
		/*
		  For 44100 sample rate the sample spacing is 22.6757 microseconds.
		  If we were using integer type for increment, we would lose a lot of
		  time over N samples, and the data in input wav and in output raw
		  files would diverge over time. Use float for better results.
		*/
		float d = 0.0F;
		while (d < (float) elements[e].duration) {
			if (elements[e].state == CW_STATE_MARK) {
				write(fd, &high, sizeof (high));
			} else {
				write(fd, &low, sizeof (low));
			}
			d += sample_spacing;
		}
	}
}




int main(int argc, char * argv[])
{
	if (argc != 2) {
		fprintf(stderr, "[ERROR] Missing path to audio file\n");
		exit(EXIT_FAILURE);
	}

	const char * path = argv[1];
	int input_fd = open(path, O_RDONLY);
	if (-1 == input_fd) {
		fprintf(stderr, "[ERROR] Can't open input file '%s'\n", path);
		exit(EXIT_FAILURE);
	}

	wav_header_t header = { 0 };
	read_wav_header(input_fd, &header);

	const float sample_spacing = (1000.0F * 1000.0F) / header.sample_rate; // [us]
	fprintf(stderr, "[INFO ] Sample rate    = %d Hz\n", header.sample_rate);
	fprintf(stderr, "[INFO ] Sample spacing = %.4f us\n", (double) sample_spacing);

	cw_element_t wav_elements[1000] = { 0 };
	const int wav_elements_count = elements_detect_from_wav(input_fd, wav_elements, sample_spacing);
	close(input_fd);
	fprintf(stderr, "[INFO ] Detected %d elements in wav file\n", wav_elements_count);
	/* Debug. */
	print_elements_to_file(stderr, wav_elements, wav_elements_count);


	/* Write square wave representing states into new output file. The file
	   can be imported into e.g. Audacity and the square wave can be compared
	   with input file. The visual comparison done by human is a kind of
	   verification that elements_detect_from_wav() works correctly. */
	char states_path[1024] = { 0 };
	snprintf(states_path, sizeof (states_path), "%s_states.raw", path);
	int states_fd = open(states_path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (-1 == states_fd) {
		fprintf(stderr, "[ERROR] Failed to open output raw file '%s': %s\n", states_path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	write_elements_to_file(states_fd, wav_elements, wav_elements_count, sample_spacing);
	close(states_fd);

	exit(EXIT_SUCCESS);
}


