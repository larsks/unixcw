/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/




#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <libcw.h>
#include <libcw_rec.h>
#include "libcw/libcw_rec.h"

#include "cw_easy_rec.h"




/**
   \file Receiving of cw made easy

   A set of wrappers around receiver API from libcw2.h that hides some
   complexities, and makes using the receiver much easier.

   This file is different from cw_easy_legacy_receiver.c in that it is a wrapper around
   modern (non-legacy) receiver API. With this API we can have more than one
   easy receiver at a time, in a single process.
*/





struct cw_easy_rec_t {

	cw_rec_t * rec;

	/* Timer for measuring length of dots and dashes.

	   Initial value of the timestamp is created by xcwcp's receiver on
	   first "paddle down" event in a character. The timestamp is then
	   updated by libcw on specific time intervals. The intervals are a
	   function of keyboard key presses or mouse button presses recorded
	   by xcwcp. */
	struct timeval main_timer;

	/* Safety flag to ensure that we keep the library in sync with keyer
	   events. Without it, there's a chance that of a on-off event, one half
	   will go to one application instance, and the other to another
	   instance.

	   TODO (acerion) 2023.08.12: this struct is used outside of xcwcp and
	   its instances, and is meant to be thread-safe. Does the above comment
	   about instances still make sense? Do we still need this member? */
	bool tracked_key_state;

	/* Flag indicating if receive polling has received a character, and
	   may need to augment it with a word space on a later poll. */
	bool is_pending_iws;

	/* Flag indicating possible receive errno detected in signal handler
	   context and needing to be passed to the foreground. */
	int libcw_receive_errno;

	pthread_t thread;
	bool run_thread;

	cw_easy_rec_callback_t callback;
	void * callback_data;
};




static bool cw_easy_rec_poll_data_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd);
static bool cw_easy_rec_poll_character_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd);
static bool cw_easy_rec_poll_iws_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd);




cw_easy_rec_t * cw_easy_rec_new(void)
{
	cw_easy_rec_t * easy_rec = (cw_easy_rec_t *) calloc(1, sizeof (cw_easy_rec_t));
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] Failed to allocate new easy rec\n");
		return NULL;
	}
	easy_rec->rec = cw_rec_new();
	if (NULL == easy_rec->rec) {
		free(easy_rec);
		fprintf(stderr, "[ERROR] Failed to allocate new receiver in easy rec\n");
		return NULL;
	}

	return easy_rec;
}




void cw_easy_rec_delete(cw_easy_rec_t ** easy_rec)
{
	if (NULL == easy_rec) {
		return;
	}
	if (NULL == *easy_rec) {
		return;
	}
	if (NULL != (*easy_rec)->rec) {
		cw_rec_delete(&(*easy_rec)->rec);
	}
	free(*easy_rec);
	*easy_rec = NULL;
}




void cw_easy_rec_handle_libcw_keying_event(void * easy_receiver, int key_state)
{
	if (NULL == easy_receiver) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return;
	}

	cw_easy_rec_t * easy_rec = (cw_easy_rec_t *) easy_receiver;
	/* Ignore calls where the key state matches our tracked key
	   state.  This avoids possible problems where this event
	   handler is redirected between application instances; we
	   might receive an end of tone without seeing the start of
	   tone. */
	if (key_state == easy_rec->tracked_key_state) {
		//fprintf(stderr, "tracked key state == %d\n", easy_rec->tracked_key_state);
		return;
	} else {
		//fprintf(stderr, "tracked key state := %d\n", key_state);
		easy_rec->tracked_key_state = key_state;
	}

	/* If this is a tone start and we're awaiting an inter-word
	   space, cancel that wait and clear the receive buffer. */
	if (key_state && easy_rec->is_pending_iws) {
		/* Tell receiver to prepare (to make space) for
		   receiving new character. */
		cw_rec_reset_state(easy_rec->rec);

		/* The tone start means that we're seeing the next
		   incoming character within the same word, so no
		   inter-word space is possible at this point in
		   time. The space that we were observing/waiting for,
		   was just inter-character space. */
		easy_rec->is_pending_iws = false;
	}

	//fprintf(stderr, "calling callback, stage 2\n");
	gettimeofday(&easy_rec->main_timer, NULL);

	/* Pass tone state on to the library.  For tone end, check to
	   see if the library has registered any receive error. */
	if (key_state) {
		/* Key down. */
		//fprintf(stderr, "start receive tone: %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (CW_SUCCESS != cw_rec_mark_begin(easy_rec->rec, &easy_rec->main_timer)) {
			// TODO: Perhaps this should be counted as test error
			perror("cw_rec_mark_begin");
			return;
		}
	} else {
		/* Key up. */
		//fprintf(stderr, "end receive tone:   %10ld . %10ld\n", easy_rec->main_timer->tv_sec, easy_rec->main_timer->tv_usec);
		if (CW_SUCCESS != cw_rec_mark_end(easy_rec->rec, &easy_rec->main_timer)) {
			/* Handle receive error detected on tone end.
			   For ENOMEM and ENOENT we set the error in a
			   class flag, and display the appropriate
			   message on the next receive poll. */
			switch (errno) {
			case EAGAIN:
				/* libcw treated the tone as noise (it
				   was shorter than noise threshold).
				   No problem, not an error. */
				break;
			case ENOMEM:
			case ERANGE:
			case EINVAL:
			case ENOENT:
				easy_rec->libcw_receive_errno = errno;
				cw_rec_reset_state(easy_rec->rec);
				break;
			default:
				perror("cw_rec_mark_end");
				// TODO: Perhaps this should be counted as test error
				return;
			}
		}
	}

	return;
}




/**
   @brief Main polling loop of a receiver

   The loop tries to periodically poll data from easy receiver. On successful
   poll, a call to cw_easy_rec_t::callback is performed.

   The loop is running as long as cw_easy_rec_t::run_thread is true.

   @reviewedon 2023.08.12

   @param[in/out] arg Easy receiver
*/
static void * thread_fn(void * arg)
{
	cw_easy_rec_t * easy_rec = (cw_easy_rec_t *) arg;
	while (easy_rec->run_thread) {
		usleep(1000);
		cw_easy_rec_data_t erd = { 0 };
		if (cw_easy_rec_poll_data_internal(easy_rec, &erd)) {
			if (easy_rec->callback) {
				/* This may pass the data to application that is using the
				   receiver. */
				easy_rec->callback(easy_rec->callback_data, &erd);
			}
		}
	}

	return NULL;
}




void cw_easy_rec_start(cw_easy_rec_t * easy_rec)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return;
	}
	gettimeofday(&easy_rec->main_timer, NULL);
	easy_rec->run_thread = true;
	pthread_create(&easy_rec->thread, NULL, thread_fn, (void *) easy_rec);
}




void cw_easy_rec_stop(cw_easy_rec_t * easy_rec)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return;
	}
	easy_rec->run_thread = false;
	pthread_join(easy_rec->thread, NULL);
}




/**
   \brief Poll given easy receiver for character or inter-word-space

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver to poll
   @param[out] erd Variable to store result of polling

   @param true if character or space has been received (successful receive has occurred)
   @param false otherwise
*/
static bool cw_easy_rec_poll_data_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd)
{
	easy_rec->libcw_receive_errno = 0;

	if (easy_rec->is_pending_iws) {
		/* Check if receiver received the pending inter-word-space. */
		cw_easy_rec_poll_iws_internal(easy_rec, erd);

		if (!easy_rec->is_pending_iws) {
			/*
			  We received the pending space. After it the receiver may have
			  received another character. Try to get it too.

			  TODO (acerion 2023.07.16): is this call really necessary? Is
			  it realistic to expect one (non-iws) character after another?
			*/
			if (cw_easy_rec_poll_character_internal(easy_rec, erd)) {
				/* This 'notice' log is used to help detecting the situation
				   described in above TODO. */
				fprintf(stderr, "[WARN ] Easy rec: unexpected successful poll of character after a space has been polled\n");
			}
			return true; /* A space has been polled successfully. */
		}
	} else {
		/* Not awaiting a possible space, so just poll the
		   next possible received character. */
		if (cw_easy_rec_poll_character_internal(easy_rec, erd)) {
			return true; /* A character has been polled successfully. */
		}
	}

	return false; /* Nothing was polled at this time. */
}




static bool cw_easy_rec_poll_character_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd)
{
	/* Don't use receiver.easy_rec->main_timer - it is used exclusively for
	   marking initial "key down" events. Use local throw-away
	   timer.

	   Additionally using reveiver.easy_rec->main_timer here would mess up time
	   intervals measured by receiver.easy_rec->main_timer, and that would
	   interfere with recognizing dots and dashes. */
	struct timeval timer;
	gettimeofday(&timer, NULL);

	errno = 0;
	const cw_ret_t cwret = cw_rec_poll_character(easy_rec->rec, &timer, &erd->character, &erd->is_iws, NULL);
	erd->errno_val = errno;
	if (CW_SUCCESS == cwret) {

		/* A full character has been received. Directly after
		   it comes a space. Either a short inter-character
		   space followed by another character (in this case
		   we won't display the inter-character space), or
		   longer inter-word space - this space we would like
		   to catch and display.

		   Set a flag indicating that next poll may result in
		   inter-word space. */
		easy_rec->is_pending_iws = true;

		//fprintf(stderr, "[DD] Received character '%c'\n", erd->character);

		return true;

	} else {
		/* Handle receive error detected on trying to read a character. */
		switch (erd->errno_val) {
		case EAGAIN:
			//fprintf(stderr, "EAGAIN\n");
			/* Call made too early, receiver hasn't
			   received a full character yet. Try next
			   time. */
			break;

		case ERANGE:
			//fprintf(stderr, "ERANGE\n");
			/* Call made not in time, or not in proper
			   sequence. Receiver hasn't received any
			   character (yet). Try harder. */
			break;

		case ENOENT:
			fprintf(stderr, "ENOENT\n");
			/* Invalid character in receiver's buffer. */
			cw_rec_reset_state(easy_rec->rec);
			break;

		case EINVAL:
			fprintf(stderr, "EINVAL\n");
			/* Timestamp error. */
			cw_rec_reset_state(easy_rec->rec);
			break;

		default:
			perror("cw_rec_poll_character");
			break;
		}

		return false;
	}
}




static bool cw_easy_rec_poll_iws_internal(cw_easy_rec_t * easy_rec, cw_easy_rec_data_t * erd)
{
	/* We expect the receiver to contain a character, but we don't
	   ask for it this time. The receiver should also store
	   information about an inter-character space. If it is longer
	   than a regular inter-character space, then the receiver
	   will treat it as inter-word space, and communicate it over
	   is_iws.

	   Don't use receiver.easy_rec->main_timer - it is used eclusively for
	   marking initial "key down" events. Use local throw-away
	   timer. */
	struct timeval timer;
	gettimeofday(&timer, NULL);
	//fprintf(stderr, "poll_iws(): %10ld : %10ld\n", timer.tv_sec, timer.tv_usec);

	if (CW_SUCCESS != cw_rec_poll_character(easy_rec->rec, &timer, &erd->character, &erd->is_iws, NULL)) {
		return false;
	}
	if (erd->is_iws) {
		//fprintf(stderr, "[DD] Character at inter-word-space: '%c'\n", erd->character);

		cw_rec_reset_state(easy_rec->rec);
		easy_rec->is_pending_iws = false;
		return true; /* Inter-word-space has been polled. */
	} else {
		/* We don't reset easy_rec->is_pending_iws. The
		   space that currently lasts, and isn't long enough
		   to be considered inter-word space, may grow to
		   become the inter-word space. Or not.

		   This growing of inter-character space into
		   inter-word space may be terminated by incoming next
		   tone (key down event) - the tone will mark
		   beginning of new character within the same
		   word. And since a new character begins, the flag
		   will be reset (elsewhere). */
		return false; /* Inter-word-space has not been polled. */
	}
}



#if 0
int cw_easy_rec_get_libcw_errno(const cw_easy_rec_t * easy_rec)
{
	return easy_rec->libcw_receive_errno;
}




void cw_easy_rec_clear_libcw_errno(cw_easy_rec_t * easy_rec)
{
	easy_rec->libcw_receive_errno = 0;
}




bool cw_easy_rec_is_pending_inter_word_space(const cw_easy_rec_t * easy_rec)
{
	return easy_rec->is_pending_iws;
}
#endif




void cw_easy_rec_clear(cw_easy_rec_t * easy_rec)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return;
	}
	cw_rec_reset_state(easy_rec->rec);
	easy_rec->is_pending_iws = false;
	easy_rec->libcw_receive_errno = 0;
	easy_rec->tracked_key_state = false;
}




cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_set_speed(easy_rec->rec, speed);
}




cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * easy_rec, int tolerance)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[ERROR] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_set_tolerance(easy_rec->rec, tolerance);
}



int cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec)
{
	if (NULL == easy_rec) {
		fprintf(stderr, "[EE] %s:%d: NULL argument\n", __func__, __LINE__);
		return CW_FAILURE;
	}
	return cw_rec_get_tolerance(easy_rec->rec);
}




void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_callback_t cb, void * data)
{
	easy_rec->callback = cb;
	easy_rec->callback_data = data;
}


