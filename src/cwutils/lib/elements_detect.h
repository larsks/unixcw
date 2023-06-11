#ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H
#define UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H




#include "elements.h"




/**
   @brief Detect elements in a wav sample

   Function reads samples from @p input_fd, decides whether samples indicate
   'mark' or 'space' state, and sets values of cw_element_t::duration and
   cw_element_t::state in @p elements.

   Each new element is set on each detected change between mark and space. As
   a result, returned value will indicate how many marks or spaces are put
   into @p elements.

   @p elements is a preallocated array of elements. Size of the array must be
   somehow guessed in advance, before this function is called (it's ok to
   over-estimate the size).

   @param[in] input_fd File descriptor from which to read samples
   @param[out] elements Pre-allocated elements structure into which to save elements
   @param[in] sample_spacing Time span between consecutive samples

   @return 0 on success
   @return -1 on failure
*/
int elements_detect_from_wav(int input_fd, cw_elements_t * elements, cw_element_time_t sample_spacing);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H */

