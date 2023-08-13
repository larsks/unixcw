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



#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include <libcw.h>

#include "wav.h"




#define FILE_HEADER_SIZE 44 /* Size of header of wav file: 44 bytes per spec. */




/**
   \file wav.c

   Code related to wav sound files.

   libcw may be generating raw sample files on some occasions, but for test
   code and test tools it's easier to work with files that have a header with
   meta-information.

   unixcw's test tools and test code may use functions from this file to
   operate on the wav sound files.

*/




int wav_read_header(int fd, wav_header_t * header)
{
	fprintf(stderr, "[INFO ] Reading %zd bytes of header\n", sizeof (wav_header_t));
	ssize_t n = read(fd, header, sizeof (wav_header_t));

	if (FILE_HEADER_SIZE != sizeof (wav_header_t)) {
		fprintf(stderr, "[ERROR] Header struct has wrong size %zd\n", sizeof (wav_header_t));
		return -1;
	}

	if (n != (ssize_t) sizeof (wav_header_t)) {
		fprintf(stderr, "[ERROR] Wrong read size for header: %zd != %zd\n", n, sizeof (wav_header_t));
		return -1;
	}

	if (1) {
		fprintf(stderr, "[INFO ] Audio file header:\n");
		fprintf(stderr, "[INFO ]     audio format:       %8u\n", header->audio_format);
		fprintf(stderr, "[INFO ]     number of channels: %8u\n", header->number_of_channels);
		fprintf(stderr, "[INFO ]     sample rate:        %8u\n", header->sample_rate);
		fprintf(stderr, "[INFO ]     bits per sample:    %8u\n", header->bits_per_sample);

		if (0) {
			fprintf(stderr, "[INFO ]     RIFF = '%.4s'\n", header->chunk_id);
			fprintf(stderr, "[INFO ]     fmt  = '%.4s'\n", header->subchunk_1_id);
			fprintf(stderr, "[INFO ]     data = '%.4s'\n", header->subchunk_2_id);
		}
	}

	return 0;
}




void wav_validate_header(const wav_header_t * header, bool * valid)
{
	/*
	  libcw produces wav files with one channel, and the other code from
	  cwutils/lib also expects one channel.
	*/
	if (CW_AUDIO_CHANNELS != header->number_of_channels) {
		fprintf(stderr, "[ERROR] wav header contains unexpected count of channels: %d (expected %d)\n",
		        header->number_of_channels, CW_AUDIO_CHANNELS);
		*valid = false;
		return;
	}

	/*
	  libcw uses unit16_t as type of sample values.

	  TODO (acerion) 2023.08.09: Calculation with sizeof and CHAR_BIT may not
	  be true if int16_t integer is represented by e.g. 32-bit integer on
	  some platform.
	*/
	const uint16_t expected_bps = sizeof (cw_sample_t) * CHAR_BIT;
	if (expected_bps != header->bits_per_sample) {
		fprintf(stderr, "[ERROR] wav header contains unexpected value of bits per sample: count of channels: %d (expected 16 / %hu)\n",
		        header->bits_per_sample, expected_bps);
		*valid = false;
		return;
	}

	*valid = true;
}


