#ifndef H_CW_EASY_LEGACY_RECEIVER
#define H_CW_EASY_LEGACY_RECEIVER




#include <stdbool.h>
#include <sys/time.h>

#include "libcw2.h"




#if defined(__cplusplus)
extern "C"
{
#endif




struct cw_easy_legacy_receiver_t;
typedef struct cw_easy_legacy_receiver_t cw_easy_legacy_receiver_t;




/* TODO: move this type to libcw_rec.h and use it to pass arguments to
   functions such as cw_rec_poll_representation_ics_internal(). */
typedef struct cw_rec_data_t {
	char character;
	char representation[20]; /* TODO: use a constant for representation's size. */
	int errno_val;
	bool is_iws;             /* Is receiver in 'found inter-word-space' state? */
	bool is_error;
} cw_rec_data_t;




/**
   @brief Constructor of easy legacy receiver object

   Use cw_easy_legacy_receiver_delete() destructor to deallocate the object.

   @reviewedon 2023.08.15

   @return newly allocated easy receiver object on success
   @return NULL on failure
*/
cw_easy_legacy_receiver_t * cw_easy_legacy_receiver_new(void);




/**
   @brief Destructor of easy legacy receiver object

   Used to deallocate object returned by cw_easy_legacy_receiver_new()

   @reviewedon 2023.08.15

   @param[in] easy_rec Pointer to easy receiver object to destruct
*/
void cw_easy_legacy_receiver_delete(cw_easy_legacy_receiver_t ** easy_rec);




/**
   @brief Start the easy legacy receiver

   A started receiver is ready to receive and recognize Morse code

   @param[in/out] easy_rec Easy receiver that should be started
*/
void cw_easy_legacy_receiver_start(cw_easy_legacy_receiver_t * easy_rec);




/**
   @brief Try polling a character from receiver

   See if a receiver has received/recognized a character (a character other
   than ' ').

   Function may return false (failure) for completely valid reasons, e.g.
   when it's too early to decide if a receiver has received something or not.

   Call this function periodically on a receiver.

   @reviewedon 2023.08.27: made sure that the function was correctly moved from xcwcp
   @reviewedon 2023.08.28

   @param[in] easy_rec Easy receiver from which to try to poll the character
   @param[out] erd Data of easy receiver, filled on successful poll.

   @return CW_SUCCESS if receiver has received a character (@p erd is updated accordingly)
   @return CW_FAILURE if receiver didn't receive a character
*/
int cw_easy_legacy_receiver_poll_character(cw_easy_legacy_receiver_t * easy_rec, cw_rec_data_t * erd);




/**
   @brief Try polling an inter-word-space from receiver

   See if a receiver has received/recognized an inter-word-space (a ' ' character).

   'iws' in function name stands for 'inter-word-space' (i.e. a ' ' character).

   Function may return false (failure) for completely valid reasons, e.g.
   when it's too early to decide if a receiver has received something or not.

   Call this function if cw_easy_legacy_receiver_is_pending_iws() returns
   true.

   @reviewedon 2023.08.27: made sure that the function was correctly moved from xcwcp
   @reviewedon 2023.08.28

   @param[in] easy_rec Easy receiver from which to try to poll the character
   @param[out] erd Data of easy receiver, filled on successful poll.

   @return CW_SUCCESS if receiver has received a space (@p erd is updated accordingly)
   @return CW_FAILURE if receiver didn't receive a space
*/
int cw_easy_legacy_receiver_poll_iws(cw_easy_legacy_receiver_t * easy_rec, cw_rec_data_t * erd);




/**
   @brief Get errno set by libcw in given easy receiver

   @reviewedon 2023.08.15

   @param[in] easy_rec Easy receiver for which to get an errno

   @return errno value set for given @p easy_rec
*/
int cw_easy_legacy_receiver_get_libcw_errno(const cw_easy_legacy_receiver_t * easy_rec);




/**
   @brief Clear errno value that was set in given easy receiver

   @reviewedon 2023.08.15

   @param[in/out] Easy receiver for which to clear an errno
*/
void cw_easy_legacy_receiver_clear_libcw_errno(cw_easy_legacy_receiver_t * easy_rec);




/**
   @brief See if receiver is expecting to receive/recognize a ' ' character

   'iws' in function name stands for 'inter-word-space' (i.e. a ' ' character).

   Call this function periodically to see if a receiver is in a state, in
   which receiving inter-word-space (a ' ' character) is likely.

   If this function returns true, then call
   cw_easy_legacy_receiver_poll_iws() to try to get the space from receiver.

   @reviewedon 2023.08.15

   @param[in] easy_rec Easy receiver for which we want to check the possibility of receiving a ' ' character.

   @return true if receiver is in state in which it may have receiver a ' ' character
   @return false otherwise
*/
bool cw_easy_legacy_receiver_is_pending_iws(const cw_easy_legacy_receiver_t * easy_rec);




/**
   @brief Clear internal state of easy receiver

   @param[in/out] easy_rec Easy receiver to clear
*/
void cw_easy_legacy_receiver_clear(cw_easy_legacy_receiver_t * easy_rec);




/**
   \brief Handle straight key event

   \param is_down
*/
void cw_easy_legacy_receiver_sk_event(cw_easy_legacy_receiver_t * easy_rec, bool is_down);





/**
   \brief Handle event on left paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void cw_easy_legacy_receiver_ik_left_event(cw_easy_legacy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles);




/**
   \brief Handle event on right paddle of iambic keyer

   \param is_down
   \param is_reverse_paddles
*/
void cw_easy_legacy_receiver_ik_right_event(cw_easy_legacy_receiver_t * easy_rec, bool is_down, bool is_reverse_paddles);




/**
   \brief Handler for the keying callback from the CW library
   indicating that the state of a key has changed.

   The "key" is libcw's internal key structure. It's state is updated
   by libcw when e.g. one iambic keyer paddle is constantly
   pressed. It is also updated in other situations. In any case: the
   function is called whenever state of this key changes.

   Notice that the description above talks about a key, not about a
   receiver. Key's states need to be interpreted by receiver, which is
   a separate task. Key and receiver are separate concepts. This
   function connects them.

   This function, called on key state changes, calls receiver
   functions to ensure that receiver does "receive" the key state
   changes.

   @param[in] easy_receiver Easy receiver object that will do the receiving
   @param[in] key_state Current state of key, passed to receiver's inner functions
*/
void cw_easy_legacy_receiver_handle_libcw_keying_event(void * easy_receiver, int key_state);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_LEGACY_RECEIVER

