#ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H
#define UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H




#include "elements.h"




/**
   @brief Detect elements in a wav sample

   Function reads samples from @p input_fd, decides whether samples indicate
   'mark' or 'space' state, and sets values of cw_element_t::duration and
   cw_element_t::state in @p elements.

   Each new element is set on each detected change between mark and space.

   @p elements is a preallocated array of elements. Size of the array must be
   somehow guessed in advance, before this function is called (it's ok to
   over-estimate the size).

   @reviewedon 2023.08.12

   @param[in] input_fd File descriptor from which to read samples
   @param[out] elements Pre-allocated elements structure into which to save elements
   @param[in] sample_spacing Time span between consecutive samples

   @return 0 on success
   @return -1 on failure
*/
int cw_elements_detect_from_wav(int input_fd, cw_elements_t * elements, cw_element_time_t sample_spacing);




/**
   @brief Detect elements in given string

   Function walks through given @p string, recognizes marks and spaces of
   different kind in each character of the string (including space
   character), builds element items from this information, sets 'type' and
   'state' in the element items, and appends the element items to @p
   elements.

   Function doesn't set 'duration' member of element items because the input
   string doesn't provide information necessary to get durations of elements.

   @reviewedon 2023.08.12

   @param[in] string string to be used as input of tests
   @param[out] elements Structure with element items (each element has its type and state set)

   @return 0 on success
   @return -1 on failure
*/
int cw_elements_detect_from_string(const char * string, cw_elements_t * elements);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_DETECT_H */

