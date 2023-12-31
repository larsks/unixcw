/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef _LIBCW_GEN_INTERNAL_H_
#define _LIBCW_GEN_INTERNAL_H_




#include <stdbool.h>



#include "libcw_gen.h"
#include "libcw_tq.h"
#include "libcw_utils.h"




/* Internal definitions of this module, exposed to unit tests code. */




/*
  In proper Morse code timing the following three rules are given:
  1. Duration of inter-mark-space is one Unit, perhaps adjusted.
  2. Duration of inter-character-space is three Units total.
  3. Duration of inter-word-space is seven Units total.
*/
#define UNITS_PER_IMS 1
#define UNITS_PER_ICS 3
#define UNITS_PER_IWS 7




CW_STATIC_FUNC cw_ret_t cw_gen_new_open_internal(cw_gen_t * gen, const cw_gen_config_t * gen_conf);
CW_STATIC_FUNC void * cw_gen_dequeue_and_generate_internal(void * arg);
CW_STATIC_FUNC int    cw_gen_calculate_sine_wave_internal(cw_gen_t * gen, cw_tone_t * tone);
CW_STATIC_FUNC int    cw_gen_calculate_sample_amplitude_internal(cw_gen_t * gen, const cw_tone_t * tone);
CW_STATIC_FUNC int    cw_gen_write_to_soundcard_internal(cw_gen_t * gen, cw_tone_t * tone);
CW_STATIC_FUNC cw_ret_t cw_gen_enqueue_valid_character_no_ics_internal(cw_gen_t * gen, char character);
CW_STATIC_FUNC void   cw_gen_recalculate_slope_amplitudes_internal(cw_gen_t * gen);
CW_STATIC_FUNC cw_ret_t cw_gen_join_thread_internal(cw_gen_t * gen);




#endif /* #ifndef _LIBCW_GEN_INTERNAL_H_ */
