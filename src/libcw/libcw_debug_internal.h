/*
 * Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
 * Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef H_LIBCW_DEBUG_INTERNAL
#define H_LIBCW_DEBUG_INTERNAL




#if defined(__cplusplus)
extern "C"
{
#endif




#include "config.h"




#if LIBCW_WITH_DEV
#include "libcw_gen.h"

/**
   @brief Open debug sink file for generator

   In addition to writing to sound sink, the generator will also write samples to the file.

   The file is a disc file in /tmp/ dir.

   The file should be closed with cw_dev_debug_raw_sink_close_internal().

   @param[in/out] gen generator for which to open the sink file
*/
void cw_dev_debug_raw_sink_open_internal(cw_gen_t * gen);

/**
   @brief Close debug sink file for generator

   Close a debug sink file that was opened with cw_dev_debug_raw_sink_open_internal().
   The file itself is not removed.

   @param[in/out] gen generator for which to close the sink file
*/
void cw_dev_debug_raw_sink_close_internal(cw_gen_t * gen);

#endif




#if defined(__cplusplus)
}
#endif




#endif  /* H_LIBCW_DEBUG_INTERNAL */

