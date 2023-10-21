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
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */




/**
   @file random.c

   @brief Code for getting random numbers

   In unixcw the quality of the source of seed doesn't have security
   implications. In situations where just want to have *some* uniqueness of
   seed, time(0) seems to be good enough on a system with up-to-date wall
   clock.

   However two factors come into play:

   1. SEI CERT MSC32-C rule says that time(0) is not good enough.

   2. If unixcw programs or libcw tests will be ever executed on embedded
   devices that perhaps won't have an NTP client, or NTP server will be
   unavailable, or the device or won't have battery-sustained real-time
   clock, the call to time(0) may be returning values from a small pool. The
   values would frequently be somewhere from first hour of first day of
   January 1970. That would mean a decreased uniqueness of seed.

   Therefore we need something better than seed(time(0)).

   The xor operation on timespec members follows recommendation from
   https://wiki.sei.cmu.edu/confluence/display/c/MSC32-C.+Properly+seed+pseudorandom+number+generators

   Further pointers:
   1. getauxval + AT_RANDOM (https://man7.org/linux/man-pages/man3/getauxval.3.html)
*/




#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>




#include "random.h"




static uint32_t cw_random_get_seed(void);




/**
   @brief Get a good value of seed for seeding a random number generator

   Depending on context, you may pass to cw_random_srand() either a
   specific value (to initialize specific sequence of pseudo-random numbers),
   or you may pass a random-ish seed. This function returns a random-ish
   seed.

   @return Random value of seed that can be passed to cw_random_srand().
*/
static uint32_t cw_random_get_seed(void)
{
	/*
	  Reasoning for nanoseconds, regardless of clock id:

	  999_999_999 nanoseconds is represented as 0x3b9ac9ff. This means that
	  we can apply mask 0x1fffffff and take 29 bits out of tv_nsec, giving
	  us 2^29 unique combinations.
	*/
	/*
	  Reasoning for seconds from BOOTTIME clock:

	  We have to assume that in worst-case scenario a program (either user
	  program or test program) is started shortly after boot time, e.g.
	  within first 32 seconds from power on. 32 seconds is encoded in first 5
	  bits. Given that we only need 32-29=3 bits, these 5 bits from tv_sec
	  are more than enough.
	*/
	/*
	  Reasoning for seconds from REALTIME clock:

	  We have to consider worst-case scenario where the wall clock of a
	  machine is stuck in year 1970 (either no NTP or no battery-sustained
	  RTC). Also as a part of worst-case scenario the program is started
	  shortly after boot up (e.g. within first 32 seconds)

	  32 seconds is encoded in first 5 bits. Given that we only need 32-29=3
	  bits, these 5 bits from tv_sec are more than enough.

	  If on the other hand it appears that we are on machine with correct
	  wall clock, we can use more bits from tv_sec.
	*/

	struct timespec boottime = { 0 };
	clock_gettime(CLOCK_BOOTTIME, &boottime);
	const uint32_t from_boottime = ((boottime.tv_nsec & 0x1fffffff) << 3) | (boottime.tv_sec & 0x7);

	uint32_t from_realtime = 0;
	struct timespec realtime = { 0 };
	clock_gettime(CLOCK_REALTIME, &realtime); /* CLOCK_REALTIME is wall-clock time. */
	if (realtime.tv_sec > 365 * 24 * 60 * 60) {
		/*
		  We are not in year 1970, so we can assume either working NTP, or
		  working RTC. Let's get more bits from seconds (more than 2).

		  365 * 24 * 60 * 60 = 31536000 = 0x1e13380 -> 0xffffff mask (24
		  bits). Don't get too wild with those seconds, shorten the mask to
		  12 bits. Let the tv_nsec still be a larger ingredient of output value.

		  Shifting tv_nsec by 12 bits loses some MSBs, but that's ok: LSBs
		  change more rapidly than MSBs.
		*/
		from_realtime = ((realtime.tv_nsec & 0x1fffffff) << 12) | (realtime.tv_sec & 0xfff);
	} else {
		/*
		  We are in year 1970. It's likely that realtime will be
		  approximately equal to boottime, so flip the ingredients compared
		  to boottime's calculation.
		*/
		from_realtime = ((realtime.tv_sec & 0x7) << 29) | (realtime.tv_nsec & 0x1fffffff);
	}
	uint32_t seed = from_boottime ^ from_realtime;

	/* Make sure that seed is never zero. See comment to cw_random_srand()
	   for reason for this. */
	if (0 == seed) {
		seed = 1;
	}

	return seed;
}




uint32_t cw_random_srand(uint32_t seed)
{
	uint32_t value = 0;
	if (0 == seed) {
		value = cw_random_get_seed();
	} else {
		value = seed;
	}

#ifdef __OpenBSD__
	/*
	  OpenBSD's implementation of srand48() ignores seed, which doesn't
	  allow for *deterministic* pseudorandom sequences. See
	  https://man.openbsd.org/srand48 for more info.

	  I need to have deterministic sequences to be able to repeat a bug that
	  happened when a test was executed with specific seed.

	  unixcw doesn't do cryptography, so non-deterministic sequences are not
	  required.
	*/
	srand48_deterministic(((long int) value);
#else
	srand48((long int) value);
#endif

	return value;
}




int cw_random_get_int(int lower, int upper, int * result)
{
	if (lower >= upper) {
		fprintf(stderr, "[ERROR] %s:%d: Invalid bounds: %d:%d\n", __func__, __LINE__, lower, upper);
		return -1;
	}
	if (lower < 0) {
		fprintf(stderr, "[ERROR] %s:%d: Negative bounds are not (yet) supported\n", __func__, __LINE__);
		return -1;
	}
	if (NULL == result) {
		fprintf(stderr, "[ERROR] %s:%d: Invalid pointer arg\n", __func__, __LINE__);
		return -1;
	}
	/* FIXME acerion 2023.10.01: lrand48() returns value in range 0-MAX, so
	   think twice about this casting before implementing support for
	   negative 'upper' and/or 'lower' values. */
	const int initial = (int) lrand48();
	/* FIXME acerion 2023.10.01: the arithmetic operations are not tested if
	   'upper' and/or 'lower' is negative. */
	const int value = lower + (initial % (upper + 1 - lower));
	*result = value;
	return 0;
}




int cw_random_get_uint32(uint32_t lower, uint32_t upper, uint32_t * result)
{
	if (lower >= upper) {
		fprintf(stderr, "[ERROR] %s:%d: Invalid bounds: %u:%u\n", __func__, __LINE__, lower, upper);
		return -1;
	}
	if (NULL == result) {
		fprintf(stderr, "[ERROR] %s:%d: Invalid pointer arg\n", __func__, __LINE__);
		return -1;
	}

	/*
	  The lrand48() function returns 'long' value, but man page says that
	  generated values are in range "0 to 2^31 - 1". So if on my machine the
	  'long' type has 8 bytes, then not all bits in long values would be
	  random.

	  To avoid confusion about how many random values are actually possible
	  to pull from this function, I can't return long, and I have to
	  return uint32_t.
	*/
	const uint32_t initial = (uint32_t) lrand48();
	const uint32_t value = lower + (initial % (upper + 1 - lower));
	*result = value;
	return 0;
}




int cw_random_get_bool(bool * result)
{
	if (NULL == result) {
		fprintf(stderr, "[ERROR] %s:%d: Invalid pointer arg\n", __func__, __LINE__);
		return -1;
	}

	const uint32_t value = (uint32_t) lrand48();
	const uint32_t val = value / 10;
	*result = val % 2;
#if 0 /* Debug. */
	fprintf(stderr, "%s:%d: bool = %-5s (from 0x%08x / %12u / %12u integer)\n",
	        __func__, __LINE__,
	        *result ? "true" : "false", value, value, val);
#endif
	return 0;
}




