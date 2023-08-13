#ifndef UNIXCW_CWUTILS_LIB_WAV_H
#define UNIXCW_CWUTILS_LIB_WAV_H




#include <stdbool.h>
#include <stdint.h>




/**
   @brief Header of wav file

   See https://ccrma.stanford.edu/courses/422-winter-2014/projects/WaveFormat/ for more info.
*/
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
   @brief Read header of WAV file

   Function doesn't validate the header's fields, caller should use
   wav_validate_header() for this.

   @reviewedon 2023.08.12

   @param[in] fd Opened file handle of WAV file
   @param[out] header Preallocated header structure into which to read the header

   @return 0 on success
   @return -1 on failure
*/
int wav_read_header(int fd, wav_header_t * header);




/**
   @brief Do a basic validation of WAV header

   Return of validation is returned through @p valid.

   The function is not very smart, but it does it's best and gives its caller
   a chance to find out about some obvious problems with the header (and thus
   with the file).

   @reviewedon 2023.08.12

   @param[in] header Header of wav file to validate
   @param[out] valid Result of validation (true = valid; false = invalid)
*/
void wav_validate_header(const wav_header_t * header, bool * valid);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_WAV_H */

