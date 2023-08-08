#ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H
#define UNIXCW_CWUTILS_LIB_ELEMENTS_H




#include <stdio.h>




#include <libcw/libcw_gen.h>




/*
  Type for dots, dashes and spaces of different types.

  Use cw_element_type_get_representation() to get printable representation of
  the type, or write your own code for getting representation of the type if
  you need to (or just print plain integers).
*/
typedef enum {
	cw_element_type_none = 0,
	cw_element_type_dot,
	cw_element_type_dash,
	cw_element_type_ims,
	cw_element_type_ics,
	cw_element_type_iws
} cw_element_type_t;




/* Yet another type for representing marks and spaces (and closed/open). */
typedef enum cw_state_t {
	cw_state_space,
	cw_state_mark
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




struct cw_elements_t {
	cw_element_t * array;
	size_t max_count;
	size_t curr_count;
};




typedef struct cw_elements_t cw_elements_t;




/**
   @brief Append new element to structure of elements

   Create new element that has given @p state and given @p duration. Add it
   to end of @p elements.

   Function may fail if there is not enough space in @p elements for new
   element.

   @param[in/out] elements Elements structure to which to append new element
   @param[in] state State of the new element
   @param[in] duration Duration of the new element

   @return 0 on success
   @return -1 on failure
*/
int cw_elements_append_element(cw_elements_t * elements, cw_state_t state, cw_element_time_t duration);




/**
   @brief Constructor of new elements structure

   The structure will have pre-allocated space for @p count elements.

   Use cw_elements_delete() to de-allocate the structure returned by this
   function.

   @param count Count of elements that the allocated structure will be able
   to hold.

   @return Newly allocated elements structure on success
   @return NULL on failure
*/
cw_elements_t * cw_elements_new(size_t count);




/**
   @brief Destructor of elements structure

   This function deallocates structure allocated with cw_elements_new().

   @param elements Pointer to elements structure that is to be deallocated
*/
void cw_elements_delete(cw_elements_t ** elements);




/**
   @brief Print elements to file

   @param[out] file File to write to
   @param[in] elements Elements structure to write to @p file
*/
void cw_elements_print_to_file(FILE * file, cw_elements_t * elements);




/**
   @brief Recognize elements in given string

   Function walks through given @p string, recognizes marks and spaces of
   different kind, builds element items from this information, sets 'type'
   and 'state' in the element items, and appends the element items to @p
   elements.

   Function doesn't set duration member of element items because the input
   string doesn't provide information necessary to get durations of elements.

   @param[in] string string to be used as input of tests
   @param[out] elements Structure with element items (each element has its type and state set)

   @return 0 on success
   @return -1 on failure
*/
int cw_elements_from_string(const char * string, cw_elements_t * elements);




/**
   @brief Get printable representation of element's type

   @param element Element for which to get the representation

   @return Single character with printable representation of given @p type
*/
char cw_element_type_get_representation(cw_element_type_t type);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H */

