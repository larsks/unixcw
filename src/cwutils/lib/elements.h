#ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H
#define UNIXCW_CWUTILS_LIB_ELEMENTS_H




#include <stdio.h>




/*
  Custom type for dots, dashes and spaces. The enum members are also visual
  representations of the dots, dashes and spaces.

  TODO: decouple representations of values from the definitions of values.
*/
typedef enum {
	dot  = '.',   /**< Dot mark. */
	dash = '-',   /**< Dash mark. */
	ims  = 'M',   /**< Inter-mark-space. */
	ics  = 'C',   /**< Inter-character-space. */
	iws  = 'W'    /**< Inter-word-space. */
} cw_element_type_t;




/* Yet another type for representing marks and spaces (and closed/open). */
typedef enum cw_state_t {
	CW_STATE_SPACE,
	CW_STATE_MARK
} cw_state_t;




/**
   @brief Data type for times, timestamps and durations of elements

   Sample spacing that is derived from sample rate is a floating point value.
   In order to keep accuracy of various timings and durations in code using
   cw_element_t type, I need to use a floating point precision type.
   Otherwise fractions of seconds lost in different places will accumulate,
   especially for data sets longer than 2-3 seconds.

   'float' data type was not good enough, so I'm using 'double', at least for
   now.
*/
typedef double cw_element_time_t;




typedef struct cw_element_t {
	cw_element_time_t duration;   /* microseconds */
	cw_element_type_t type;
	cw_state_t state;
} cw_element_t;





void elements_append_new(cw_element_t * elements, int * elements_iter, cw_state_t state, cw_element_time_t duration);
void elements_clear_durations(cw_element_t * elements, int count);




/**
   @brief Print elements to file

   @param[out] file File to write to
   @param[in] elements Elements to write to @p file
   @param[int] count Count of elements to write
*/
void elements_print_to_file(FILE * file, cw_element_t * elements, int count);




int elements_from_string(const char * string, cw_element_t * elements, int array_size);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H */

