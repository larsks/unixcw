#ifndef UNIXCW_CWUTILS_LIB_MISC_H
#define UNIXCW_CWUTILS_LIB_MISC_H




#include <stdio.h>

#include <libcw_gen.h>

#include "elements.h"




/**
   @brief Get duration of element of given type

   Look up duration of given @p type in provided @p durations.

   Use @p durations as source of information on what is the duration of
   element of given @p type.

   @reviewedon 2023.08.12

   @param[in] type Type of element for which to get duration
   @param[in] durations Information on durations taken from a generator
   @param[out] duration Duration of element of given type

   @return 0 on success
   @return -1 on failure
*/
int cw_element_type_to_duration(cw_element_type_t type, const cw_gen_durations_t * durations, int * duration);




/**
   @brief Print @p durations structure to given @p file

   This is just a debug function.

   @reviewedon 2023.08.12

   @param[out] file File to which to print info on @p durations
   @param[in] durations Durations to print
*/
void cw_gen_durations_print(FILE * file, const cw_gen_durations_t * durations);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_MISC_H */

