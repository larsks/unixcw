#ifndef H_CW_EASY_REC
#define H_CW_EASY_REC




#include <libcw_rec.h>
#include <libcw2.h>

#include "cw_easy_legacy_receiver.h"




#if defined(__cplusplus)
extern "C"
{
#endif




struct cw_easy_rec_t;
typedef struct cw_easy_rec_t cw_easy_rec_t;




/**
   @brief Container for single point of data received by a receiver

   When receiver is polled for data and the receiver responds that it
   successfully detected (received) some character, the character is returned
   in variables of this type.
*/
typedef struct cw_easy_rec_data_t {
	char character;
	char representation[20]; /* TODO: use a constant for representation's size. */
	int errno_val;
	bool is_iws;             /* Is receiver in 'found inter-word-space' state? */
	bool is_error;
} cw_easy_rec_data_t;




/**
   @brief Callback executed on successful receiving of single data point (single character) by receiver

   @param[in] callback_data Pointer to some object in an application that is using the receiver
   @param[in] erd Data with information about what was received by the receiver
*/
typedef void (*cw_easy_rec_receive_callback_t)(void * callback_data, cw_easy_rec_data_t * erd);




/**
   @brief Constructor of easy receiver object

   Objects created with the constructor should be deleted with cw_easy_rec_delete().

   @reviewedon 2023.08.12

   @return Easy receiver on success
   @return NULL on failure
*/
cw_easy_rec_t * cw_easy_rec_new(void);




/**
   @brief Destructor of easy receiver object

   Resources associated with the easy receiver are deallocated and the @p
   easy_rec pointer is set to NULL.

   Call the destructor on objects returned by cw_easy_rec_new() constructor.

   @reviewedon 2023.08.12

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

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver that should start receiving
*/
void cw_easy_rec_start(cw_easy_rec_t * easy_rec);




/**
   \brief Stop easy receiver

   Stop the process of receiving by given receiver.

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver to stop
*/
void cw_easy_rec_stop(cw_easy_rec_t * easy_rec);




void cw_easy_rec_clear(cw_easy_rec_t * easy_rec);




/**
   @brief Wrapper around cw_rec_set_speed() for easy receiver

   Right now there is no way to set easy receiver in adaptive mode.

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver for which to set the speed
   @param[in] speed New value of speed

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to set speed in receiver
*/
cw_ret_t cw_easy_rec_set_speed(cw_easy_rec_t * easy_rec, int speed);




/**
   @brief Wrapper around cw_rec_set_tolerance() for easy receiver

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver for which to set the tolerance
   @param[in] tolerance New value oftolerance

   @return CW_SUCCESS on success
   @return CW_FAILURE if function failed to set tolerance in receiver
*/
cw_ret_t cw_easy_rec_set_tolerance(cw_easy_rec_t * rec, int tolerance);




/**
   @brief Wrapper around cw_rec_get_tolerance() for easy receiver

   @reviewedon 2023.08.12

   @param[in] easy_rec Easy receiver from which to get the tolerance

   @return Receive tolerance of a receiver
*/
int cw_easy_rec_get_tolerance(const cw_easy_rec_t * easy_rec);




/**
   \brief Register a callback that will be called whenever successful receive occurs

   'successful receive' means that either a character or an
   inter-word-space has been received.

   The callback will be called on each successful receive. A variable of type
   cw_easy_rec_data_t that contains details of current successful receive
   event will be passed to the callback.

   @reviewedon 2023.08.12

   @param[in/out] easy_rec Easy receiver with which to register a callback
   @param[in] cb Callback to be registered - function that will be called on each successful receive
   @param[in] data Pointer to client-side variable that will be passed to @p cb
*/
void cw_easy_rec_register_receive_callback(cw_easy_rec_t * easy_rec, cw_easy_rec_receive_callback_t cb, void * data);




/**
   \brief Handler for the keying callback from the CW library
   indicating that the state of a key has changed

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

   To be registered in generator with
   cw_gen_register_value_tracking_callback_internal().

   @reviewedon 2023.08.12

   @param[in/out] easy_receiver Easy receiver
   @param[in] key_state
*/
void cw_easy_rec_handle_libcw_keying_event(void * easy_receiver, int key_state);




#if defined(__cplusplus)
}
#endif




#endif // #ifndef H_CW_EASY_REC

