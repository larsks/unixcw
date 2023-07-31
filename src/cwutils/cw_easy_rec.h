#ifndef H_CW_EASY_REC
#define H_CW_EASY_REC



#include <libcw_rec.h>
#include "libcw2.h"
#include "cw_rec_utils.h"



#if defined(__cplusplus)
extern "C"
{
#endif






struct cw_easy_rec_t;
typedef struct cw_easy_rec_t cw_easy_rec_t;




typedef struct cw_easy_rec_data_t {
	char character;
	char representation[20]; /* TODO: use a constant for representation's size. */
	int errno_val;
	bool is_iws;             /* Is receiver in 'found inter-word-space' state? */
	bool is_error;
} cw_easy_rec_data_t;




typedef void (*cw_easy_rec_callback_t)(cw_easy_rec_data_t * erd, void * callback_data);




/**
   @brief Constructor of easy receiver object

   Objects created with the constructor should be deleted with cw_easy_rec_delete().

   @return Easy receiver on success
   @return NULL on failure
*/
cw_easy_rec_t * cw_easy_rec_new(void);




/**
   @brief Destructor of easy receiver object

   Call the destructor on objects returned by cw_easy_rec_new() constructor.

   @param[in/out] easy_rec Pointer to easy receiver
*/
void cw_easy_rec_delete(cw_easy_rec_t ** easy_rec);




/**
   \brief Start easy receiver

   Let the receiver start receiving.

   Notice that without calling
   cw_gen_register_value_tracking_callback_internal() and
   cw_easy_rec_register_receive_callback() the receiver won't be doing anything
   useful.

   @param[in/out] easy_rec Easy receiver that should start receiving
*/
void cw_easy_rec_start(cw_easy_rec_t * easy_rec);




/**
   \brief Stop easy receiver

   Stop the process of receiving by given receiver

   @param[in/out] easy_rec Easy receiver to stop
*/
void cw_easy_rec_stop(cw_easy_rec_t * easy_rec);




void cw_easy_rec_clear(cw_easy_rec_t * easy_rec);




/**
   @brief Wrapper around cw_rec_set_speed() for easy receiver

   Right now there is no way to set easy receiver in adaptive mode.

   @param[in] easy_rec easy_rec Easy receiver for which to set the speed
   @param[in] speed New value of speed

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to set speed in receiver
*/
cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed);




cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * rec, int tolerance);
int cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec);




/**
   \brief Register a callback that will be called whenever successful receive occurs

   'successful receive' means that either a character or an
   inter-character-space has been received.

   The callback will be called on each successful receive. The first argument
   to the callback will be a variable of type cw_easy_rec_data_t that contains
   details of current receive event.

   @param[in/out] easy_rec easy_rec Easy receiver with which to register a callback
   @param[in] cb Callback to be registered - function that will be called on each receive
   @param[in] data Pointer to client-side variable that will be passed to @p cb
*/
void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_callback_t cb, void * data);




void cw_easy_rec_handle_libcw_keying_event(void * easy_receiver, int key_state);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_REC

