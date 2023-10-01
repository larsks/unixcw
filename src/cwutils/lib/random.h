#ifndef UNIXCW_CWUTILS_LIB_RANDOM_H
#define UNIXCW_CWUTILS_LIB_RANDOM_H




#include <stdint.h>




/**
   @brief Initialize random number generator

   Pass @p seed value to some internal seed() function. The seed is used to
   initialize (pseudo-)random number generator.

   If @p seed is not zero, it will be used to seed the generator, per above description.
   If @p seed is zero, the function will generate a seed itself internally.

   This conditional behaviour of the function has one downside: if you decide
   to use zero as an intended seed (or the value will be selected to be zero
   for you), you will have to edit source code to pass zero seed to
   generator. But that's rather unlikely.

   @param seed Initial value used to seed a random number generator (if the value is non-zero).

   @return value of seed
*/
uint32_t cw_random_srand(uint32_t seed);




/**
   @brief Generate random integer

   Returned value will be between @p lower and @p upper, inclusive. Value is
   returned through @p result argument.

   Negative values of @p upper and/or @p lower are not (yet) supported.

   @param[in] lower Lower bound of range of returned values (inclusive)
   @param[in] upper Upper bound of range of returned values (inclusive)
   @param[out] result Output variable for returned random number

   @return 0 on success
   @return -1 on failure
*/
int cw_random_get_int(int lower, int upper, int * result);




/**
   @brief Generate random uint32_t integer

   Returned value will be between @p lower and @p upper, inclusive. Value is
   returned through @p result argument.

   @param[in] lower Lower bound of range of returned values (inclusive)
   @param[in] upper Upper bound of range of returned values (inclusive)
   @param[out] result Output variable for returned random number

   @return 0 on success
   @return -1 on failure
*/
int cw_random_get_uint32(uint32_t lower, uint32_t upper, uint32_t * result);




#endif /* #ifndef UNIXCW_CWUTILS_LIB_RANDOM_H */

