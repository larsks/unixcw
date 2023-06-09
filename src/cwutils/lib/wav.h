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




/**
   @Brief Read header of WAV file

   Function doesn't validate the header's fields.

   @param[in] fd File handle of WAV file
   @param[out] header Preallocated header structure into which to read the header

   @return 0 on success
   @return -1 on failure
*/
int read_wav_header(int fd, wav_header_t * header);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_WAV_H */

