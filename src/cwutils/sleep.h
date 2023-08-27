#ifndef H_CWUTILS_SLEEP
#define H_CWUTILS_SLEEP




/**
   @brief Sleep for given amount of milliseconds

   The function uses nanosleep(), and can handle incoming SIGALRM signals
   that cause regular nanosleep() to return. The function calls nanosleep()
   until all time specified by @p msecs has elapsed.

   The function may sleep a little longer than specified by @param
   msecs if it needs to spend some time handling SIGALRM signal. Other
   restrictions from nanosleep()'s man page also apply.

   @reviewed 2023-08-27
*/
void cw_millisleep_internal(int msecs);




#endif /* #ifndef H_CWUTILS_SLEEP */


