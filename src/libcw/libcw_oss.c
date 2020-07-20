/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2020  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/




/**
   \file libcw_oss.c

   \brief OSS sound system.
*/




#include <stdbool.h>




#include "config.h"
#include "libcw_debug.h"
#include "libcw_oss.h"




#define MSG_PREFIX "libcw/oss: "




extern cw_debug_t cw_debug_object;
extern cw_debug_t cw_debug_object_ev;
extern cw_debug_t cw_debug_object_dev;




#ifdef LIBCW_WITH_OSS




#include <assert.h>
#include <ctype.h>
#include <dlfcn.h> /* dlopen() and related symbols */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if   defined(HAVE_SYS_SOUNDCARD_H)
#       include <sys/soundcard.h>
#elif defined(HAVE_SOUNDCARD_H)
#       include <soundcard.h>
#else
#
#endif




#include "libcw_gen.h"




extern const unsigned int cw_supported_sample_rates[];




/* Conditional compilation flags. */
#define CW_OSS_SET_FRAGMENT       1  /* ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &param) */
#define CW_OSS_SET_POLICY         0  /* ioctl(fd, SNDCTL_DSP_POLICY, &param) */

/* Constants specific to OSS sound system configuration. */
static const int CW_OSS_SETFRAGMENT = 7;              /* Sound fragment size, 2^7 samples. */
static const int CW_OSS_SAMPLE_FORMAT = AFMT_S16_NE;  /* Sound format AFMT_S16_NE = signed 16 bit, native endianess; LE = Little endianess. */

static cw_ret_t cw_oss_open_device_ioctls_internal(int fd, int * sample_rate);
static cw_ret_t cw_oss_get_version_internal(int fd, int * x, int * y, int * z);
static cw_ret_t cw_oss_write_buffer_to_sound_device_internal(cw_gen_t * gen);
static cw_ret_t cw_oss_open_and_configure_sound_device_internal(cw_gen_t * gen);
static void cw_oss_close_sound_device_internal(cw_gen_t * gen);




/**
   @brief Check if it is possible to open OSS output with given device name

   Function does a test opening and test configuration of OSS output,
   but it closes it before returning.

   @reviewed on 2020-07-19

   @param device_name[in] name of OSS device to be used; if NULL then the
   function will use library-default device name.

   @return true if opening OSS output succeeded
   @return false if opening OSS output failed
*/
bool cw_is_oss_possible(const char * device_name)
{
	const char * dev = device_name ? device_name : CW_DEFAULT_OSS_DEVICE;
	/* Open the given soundcard device file, for write only. */
	int soundcard = open(dev, O_WRONLY);
	if (soundcard == -1) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "is possible: open(%s): '%s'", dev, strerror(errno));
		return false;
        }

	{
		int x = 0, y = 0, z = 0;
		cw_ret_t cw_ret = cw_oss_get_version_internal(soundcard, &x, &y, &z);
		if (cw_ret == CW_FAILURE) {
			cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
				      MSG_PREFIX "is possible: can't get OSS version '%s'", strerror(errno));
			close(soundcard);
			return false;
		} else {
			cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
				      MSG_PREFIX "is possible: OSS version %d.%d.%d", x, y, z);
		}
	}

	/*
	  http://manuals.opensound.com/developer/OSS_GETVERSION.html:
	  about OSS_GETVERSION ioctl:
	  "This ioctl call returns the version number OSS API used in
	  the current system. Applications can use this information to
	  find out if the OSS version is new enough to support the
	  features required by the application. However this methods
	  should be used with great care. Usually it's recommended
	  that applications check availability of each ioctl() by
	  calling it and by checking if the call returned errno=EINVAL."

	  So, we call all necessary ioctls to be 100% sure that all
	  needed features are available. cw_oss_open_device_ioctls_internal()
	  doesn't specifically look for EINVAL, it only checks return
	  values from ioctl() and returns CW_FAILURE if one of ioctls()
	  returns -1. */
	int dummy;
	cw_ret_t cw_ret = cw_oss_open_device_ioctls_internal(soundcard, &dummy);
	close(soundcard);
	if (cw_ret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "is possible: one or more OSS ioctl() calls failed");
		return false;
	} else {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "is possible: OSS is possible");
		return true;
	}
}




/**
   @brief Configure given @p gen variable to work with OSS sound system

   This function only sets some fields of @p gen (variables and function
   pointers). It doesn't interact with OSS sound system.

   @reviewed 2020-07-19

   @param gen[in] generator structure in which to fill some fields
   @param device_name[in] name of OSS device to use

   @return CW_SUCCESS
*/
cw_ret_t cw_oss_fill_gen_internal(cw_gen_t * gen, const char * device_name)
{
	assert (gen);

	gen->sound_system = CW_AUDIO_OSS;
	cw_gen_set_sound_device_internal(gen, device_name);

	gen->open_and_configure_sound_device = cw_oss_open_and_configure_sound_device_internal;
	gen->close_sound_device              = cw_oss_close_sound_device_internal;
	gen->write_buffer_to_sound_device    = cw_oss_write_buffer_to_sound_device_internal;

	return CW_SUCCESS;
}




/**
   @brief Write generated samples to OSS sound system device configured and opened for generator

   @reviewed 2020-07-19

   @param gen[in] generator that will write to sound device

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_oss_write_buffer_to_sound_device_internal(cw_gen_t * gen)
{
	assert (gen);
	assert (gen->sound_system == CW_AUDIO_OSS);

	int n_bytes = sizeof (gen->buffer[0]) * gen->buffer_n_samples;
	int rv = write(gen->sound_sink_fd, gen->buffer, n_bytes);
	if (rv != n_bytes) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "write: %s", strerror(errno));
		return CW_FAILURE;
	}
	// cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO, MSG_PREFIX "written %d samples", gen->buffer_n_samples);

	return CW_SUCCESS;
}




/**
   @brief Open and configure OSS handle stored in given generator

   You must use cw_gen_set_sound_device_internal() before calling
   this function. Otherwise generator \p gen won't know which device to open.

   @reviewed on 2020-07-19

   @param gen[in] generator for which to open and configure sound system handle

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
cw_ret_t cw_oss_open_and_configure_sound_device_internal(cw_gen_t * gen)
{
	/* TODO: there seems to be some redundancy between
	   cw_oss_open_and_configure_sound_device_internal() and is_possible() function. */

	/* Open the given soundcard device file, for write only. */
        gen->sound_sink_fd = open(gen->sound_device, O_WRONLY);
	if (-1 == gen->sound_sink_fd) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: open(%s): '%s'", gen->sound_device, strerror(errno));
		return CW_FAILURE;
	}

	cw_ret_t cw_ret = cw_oss_open_device_ioctls_internal(gen->sound_sink_fd, &gen->sample_rate);
	if (cw_ret != CW_SUCCESS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: one or more OSS ioctl() calls failed");
		close(gen->sound_sink_fd);
		return CW_FAILURE;
	}


	int size = 0;
	/* Get fragment size in bytes, may be different than requested
	   with ioctl(..., SNDCTL_DSP_SETFRAGMENT), and, in particular,
	   can be different than 2^N. */
	int rv = rv = ioctl(gen->sound_sink_fd, (int) SNDCTL_DSP_GETBLKSIZE, &size);
	if (-1 == rv) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: ioctl(SNDCTL_DSP_GETBLKSIZE): '%s'", strerror(errno));
		close(gen->sound_sink_fd);
		return CW_FAILURE;
	}

	if ((size & 0x0000ffff) != (1 << CW_OSS_SETFRAGMENT)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "open: OSS fragment size not set, %d", size);
		close(gen->sound_sink_fd);
		/* FIXME */
		return CW_FAILURE;
	} else {
		/* TODO: are we reporting size correctly? Shouldn't it be
		   "(size & 0x0000ffff)" or maybe even "2^(size & 0x0000ffff)"? */
		cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
			      MSG_PREFIX "open: OSS fragment size = %d", size);
	}
	gen->buffer_n_samples = size;


	cw_oss_get_version_internal(gen->sound_sink_fd, &gen->oss_version.x, &gen->oss_version.y, &gen->oss_version.z);

	/* Mark sound sink as now open for business. */
	gen->sound_device_is_open = true;

#if CW_DEV_RAW_SINK
	gen->dev_raw_sink = open("/tmp/cw_file.oss.raw", O_WRONLY | O_TRUNC | O_NONBLOCK);
#endif

	return CW_SUCCESS;
}




/**
   @brief Perform all necessary ioctl calls on opened OSS file descriptor

   Wrapper function for ioctl calls that need to be done when configuring
   file descriptor @p fd for OSS playback.

   @reviewed on 2020-07-19

   @param fd[in] file descriptor of open OSS file;
   @param sample_rate[out] sample rate configured by ioctl calls

   @return CW_FAILURE on errors
   @return CW_SUCCESS on success
*/
cw_ret_t cw_oss_open_device_ioctls_internal(int fd, int * sample_rate)
{
	int parameter = 0; /* Ignored. */
	if (-1 == ioctl(fd, SNDCTL_DSP_SYNC, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SYNC): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#if 0
	/*
	   This ioctl call failed on FreeBSD 10, which resulted in
	   libcw failing to open OSS device. A bit of digging on the
	   web revealed this:

	   OSS4:
	   http://manuals.opensound.com/developer/SNDCTL_DSP_POST.html:
	   "This ioctl call is provided for compatibility with older
	   applications. It has no practical purpose and should in no
	   case be used in new applications."
	*/

	parameter = 0; /* Ignored. */
	if (-1 == ioctl(fd, SNDCTL_DSP_POST, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_POST): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#endif
	/* Set the sample format. */
	parameter = CW_OSS_SAMPLE_FORMAT;
	if (-1 == ioctl(fd, (int) SNDCTL_DSP_SETFMT, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SETFMT): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	if (parameter != CW_OSS_SAMPLE_FORMAT) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: sample format not supported");
		/* TODO: can't we try some other sample format? */
		return CW_FAILURE;
	}

	/* Set up mono/stereo mode. */
	parameter = CW_AUDIO_CHANNELS;
	if (-1 == ioctl(fd, (int) SNDCTL_DSP_CHANNELS, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_CHANNELS): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	if (parameter != CW_AUDIO_CHANNELS) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: number of channels not supported");
		return CW_FAILURE;
	}

	/* Set up a standard sampling rate based on the notional correct
	   value, and retain the one we actually get. */
	unsigned int rate = 0;
	bool success = false;
	for (int i = 0; cw_supported_sample_rates[i]; i++) {
		rate = cw_supported_sample_rates[i];
		if (0 == ioctl(fd, (int) SNDCTL_DSP_SPEED, &rate)) {
			if (rate != cw_supported_sample_rates[i]) {
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: imprecise sample rate:");
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: asked for: %u", cw_supported_sample_rates[i]);
				cw_debug_msg (&cw_debug_object_dev, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_WARNING, MSG_PREFIX "ioctls: got:       %u", rate);
			}
			success = true;
			break;
		}
	}

	if (!success) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SPEED): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		*sample_rate = rate;
	}


	audio_buf_info buff;
	if (-1 == ioctl(fd, (int) SNDCTL_DSP_GETOSPACE, &buff)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_GETOSPACE): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		/*
		fprintf(stderr, "before:\n");
		fprintf(stderr, "buff.fragments = %d\n", buff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", buff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", buff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", buff.fragstotal);
		*/
	}


#if CW_OSS_SET_FRAGMENT
	/*
	 * Live a little dangerously, by trying to set the fragment size of the
	 * card.  We'll try for a relatively short fragment of 128 bytes.  This
	 * gives us a little better granularity over the amounts of audio data
	 * we write periodically to the soundcard output buffer.  We may not get
	 * the requested fragment size, and may be stuck with the default.  The
	 * argument has the format 0xMMMMSSSS - fragment size is 2^SSSS, and
	 * setting 0x7fff for MMMM allows as many fragments as the driver can
	 * support.
	 */
	/* parameter = 0x7fff << 16 | CW_OSS_SETFRAGMENT; */
	parameter = 0x0032 << 16 | CW_OSS_SETFRAGMENT;

	if (-1 == ioctl(fd, (int) SNDCTL_DSP_SETFRAGMENT, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_SETFRAGMENT): '%s'", strerror(errno));
		return CW_FAILURE;
	}
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "ioctls: fragment size is 2^%d = %d", parameter & 0x0000ffff, 2 << ((parameter & 0x0000ffff) - 1));

	/* Query fragment size just to get the driver buffers set. */
	if (-1 == ioctl(fd, (int) SNDCTL_DSP_GETBLKSIZE, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_GETBLKSIZE): '%s'", strerror(errno));
		return CW_FAILURE;
	}

	if (parameter != (1 << CW_OSS_SETFRAGMENT)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: OSS fragment size not set, %d", parameter);
	}

#endif
#if CW_OSS_SET_POLICY
	parameter = 5; /* TODO: what does this value mean? */
	if (-1 == ioctl(fd, SNDCTL_DSP_POLICY, &parameter)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_DSP_POLICY): '%s'", strerror(errno));
		return CW_FAILURE;
	}
#endif

	if (-1 == ioctl(fd, (int) SNDCTL_DSP_GETOSPACE, &buff)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "ioctls: ioctl(SNDCTL_GETOSPACE): '%s'", strerror(errno));
		return CW_FAILURE;
	} else {
		/*
		fprintf(stderr, "after:\n");
		fprintf(stderr, "buff.fragments = %d\n", buff.fragments);
		fprintf(stderr, "buff.fragsize = %d\n", buff.fragsize);
		fprintf(stderr, "buff.bytes = %d\n", buff.bytes);
		fprintf(stderr, "buff.fragstotal = %d\n", buff;3R.fragstotal);
		*/
	}

	return CW_SUCCESS;
}




/**
   @brief Close OSS device stored in given generator

   @reviewed on 2020-07-19

   @param gen[in] generator for which to close its sound device
*/
void cw_oss_close_sound_device_internal(cw_gen_t * gen)
{
	close(gen->sound_sink_fd);
	gen->sound_sink_fd = -1;
	gen->sound_device_is_open = false;

#if CW_DEV_RAW_SINK
	if (gen->dev_raw_sink != -1) {
		close(gen->dev_raw_sink);
		gen->dev_raw_sink = -1;
	}
#endif
	return;
}




/**
   @brief Get version number of OSS API

   @reviewed on 2020-07-19

   @param fd[in] opened file descriptor for OSS device
   @param x[out] first digit of OSS version
   @param y[out] second digit of OSS version
   @param z[out] third digit of OSS version

   @return CW_SUCCESS on success
   @return CW_FAILURE otherwise
*/
cw_ret_t cw_oss_get_version_internal(int fd, int * x, int * y, int * z)
{
	assert (fd != -1);

	int version = 0;
	if (-1 == ioctl(fd, (int) OSS_GETVERSION, &version)) {
		cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
			      MSG_PREFIX "get version: ioctl OSS_GETVERSION");
		return CW_FAILURE;
	} else {
		*x = (version & 0xFF0000) >> 16;
		*y = (version & 0x00FF00) >> 8;
		*z = (version & 0x0000FF) >> 0;
		return CW_SUCCESS;
	}
}




#else /* #ifdef LIBCW_WITH_OSS */




bool cw_is_oss_possible(__attribute__((unused)) const char * device)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return false;
}




cw_ret_t cw_oss_fill_gen_internal(__attribute__((unused)) cw_gen_t * gen, __attribute__((unused)) const char * device_name)
{
	cw_debug_msg (&cw_debug_object, CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_INFO,
		      MSG_PREFIX "This sound system has been disabled during compilation");
	return CW_FAILURE;
}




#endif /* #ifdef LIBCW_WITH_OSS */
