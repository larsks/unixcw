#ifndef UNIXCW_CWUTILS_LIB_WAV_H
#define UNIXCW_CWUTILS_LIB_WAV_H




#include <stdint.h>




// https://ccrma.stanford.edu/courses/422-winter-2014/projects/WaveFormat/
typedef struct __attribute__((packed)) wav_header_t {
	char chunk_id[4]; /* "RIFF" */
	uint32_t chunk_size;
	char format[4]; /* "WAVE" */

	char subchunk_1_id[4]; /* "fmt " */
	uint32_t subchunk_1_size;
	uint16_t audio_format;
	uint16_t number_of_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;

	char subchunk_2_id[4]; /* "data" */
	uint32_t subchunk_2_size;
} wav_header_t;
#define FILE_HEADER_SIZE 44 /* 44 bytes per spec. */




int read_wav_header(int fd, wav_header_t * header);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_WAV_H */

