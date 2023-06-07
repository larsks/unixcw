#ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H
#define UNIXCW_CWUTILS_LIB_ELEMENTS_H




#include <stdio.h>




/*
  Custom type for dots, dashes and spaces just for the purpose of this test.
  The enum members are also visual representations of the dots, dashes and
  spaces.
*/
typedef enum {
	dot  = '.',
	dash = '-',
	ims  = 'M',
	ics  = 'C',
	iws  = 'W'
} cw_element_type_t;




typedef enum cw_state_t {
	CW_STATE_SPACE,
	CW_STATE_MARK
} cw_state_t;




typedef struct cw_element_t {
	int duration;   /* microseconds */
	cw_element_type_t type;
	cw_state_t state;
} cw_element_t;



typedef struct cw_element_stats_t {
	int duration_min;
	int duration_avg;
	int duration_max;

	int duration_total;
	int count;
} cw_element_stats_t;





void elements_append_new(cw_element_t * elements, int * elements_iter, cw_state_t state, int duration);
void elements_clear_durations(cw_element_t * elements, int count);
void elements_print_to_file(FILE * file, cw_element_t * elements, int count);
void element_stats_update(cw_element_stats_t * stats, int element_duration);
int elements_from_string(const char * string, cw_element_t * elements, int array_size);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_ELEMENTS_H */

