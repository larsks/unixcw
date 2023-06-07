#include <stdio.h>
#include <unistd.h>

#include "wav.h"




int read_wav_header(int fd, wav_header_t * header)
{
	fprintf(stderr, "[INFO ] Reading %zd bytes of header\n", sizeof (wav_header_t));
	int n = read(fd, header, sizeof (wav_header_t));

	if (FILE_HEADER_SIZE != sizeof (wav_header_t)) {
		fprintf(stderr, "[ERROR] Header struct has wrong size %zd\n", sizeof (wav_header_t));
		return -1;
	}

	if (n != sizeof (wav_header_t)) {
		fprintf(stderr, "[ERROR] Wrong read size for header: %d != %zd\n", n, sizeof (wav_header_t));
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







