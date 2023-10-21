#ifndef CWGEN_TESTS_WORDSET_H
#define CWGEN_TESTS_WORDSET_H




#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>




#define WORDSET_WORDS_COUNT_MAX   1024
#define WORDSET_WORD_SIZE_MAX      128




typedef struct {
	uint32_t groups;

	/*
	  If ::groupsize_h is zero, this field is the exact size of group
	  (word). Otherwise this is lower end of group size range.

	  Always must be at least 1.
	*/
	uint32_t groupsize;

	/*
	  Upper (high) end of group size range. Set to zero if you don't
	  want to use range of allowed sizes, and want to only use
	  ::groupsize to specify exact size.
	*/
	uint32_t groupsize_h;

	uint32_t repeat;

	char charset[128];

	/*
	  Count of non-white characters that will be printed by cwgen to
	  stdout. When limit is specified, the last word may be truncated to
	  satisfy the limit. Also a count of repeats may be ignored and fewer
	  than 'repeat' words will be printed.
	*/
	uint32_t limit;
} wordset_opts_t;




typedef struct {
	uint32_t words_count;
	char words[WORDSET_WORDS_COUNT_MAX][WORDSET_WORD_SIZE_MAX];
} wordset_t;



/**
   @brief Read wordset from file

   @return 0 if reading was successful
   @return other value otherwise
*/
int wordset_read_from_file(FILE * file, wordset_t * wordset);

/**
   @brief Print wordset to given file

   Useful for debugging.
*/
int wordset_printf(FILE * file, const wordset_t * wordset);


/**
   @brief Validate contents of wordset

   See if words in @p wordset meet criteria set by options from @p opts.

   Compare output of cwgen (stored in @p wordset) with options passed to
   cwgen (options created from @p opts).

   Status of validation is returned through @p success (true: wordset is
   valid; false: words in wordset don't match criteria specified by @p opts).

   @return 0 if validation was completed without internal errors
   @return other value if validation was not completed because of internal errors
*/
int wordset_validate(const wordset_t * wordset, const wordset_opts_t * opts, bool * success);




#endif /* #ifndef CWGEN_TESTS_WORDSET_H */

