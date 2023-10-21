/*
  Copyright (C) 2023  Kamil Ignacak (acerion@wp.pl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program. If not, see <https://www.gnu.org/licenses/>.
*/




/**
   @file wordset.c

   Data structure with words (groups of non-white characters).

   The default source of the words in the context of unixcw is cwgen.

   This file provides a function for reading the words from text file and a
   function for validating whether the words in a wordset meet criteria
   specified by a set of cwgen's options.
*/




#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "wordset.h"




static const uint32_t WORDSET_DEFAULT_GROUPS_COUNT   = 128;
static const uint32_t WORDSET_DEFAULT_GROUP_SIZE     =   5;
static const char * WORDSET_DEFAULT_CHARSET          =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";




typedef enum {
	read_word_err,      /**< Error while reading a word. */
	read_word_ok,       /**< Success: non empty word was read. */
	read_word_empty,    /**< Empty word was read. */
	read_word_eof       /**< End-Of-File was reached before reading a word. */
} read_word_t;

static read_word_t read_word(FILE * file, char * buf, size_t size);

static int wordset_append(wordset_t * wordset, const char * word);




/**
   @brief Append single word to a wordset
*/
static int wordset_append(wordset_t * wordset, const char * word)
{
	const size_t size = sizeof (wordset->words[wordset->words_count]);
	strncpy(wordset->words[wordset->words_count], word, size);
	wordset->words[wordset->words_count][size - 1] = '\0';
	wordset->words_count++;

	return 0;
}




int wordset_printf(FILE * file, const wordset_t * wordset)
{
	for (uint32_t i = 0; i < wordset->words_count; i++) {
		(void) fprintf(file, "%s ", wordset->words[i]);
	}
	fflush(file);

	return 0;
}




int wordset_read_from_file(FILE * file, wordset_t * wordset)
{
	bool run = true;
	while (run) {
		char word[WORDSET_WORD_SIZE_MAX] = { 0 };
		const int ret = read_word(file, word, sizeof (word));
		if (read_word_ok == ret) {
			wordset_append(wordset, word);
		}
		run = ret == read_word_ok || ret == read_word_empty;
	}
	return 0;
}




int wordset_validate(const wordset_t * wordset, const wordset_opts_t * opts, bool * success)
{
	const uint32_t times = opts->repeat + 1;

	uint32_t wordset_count = wordset->words_count;
	if (opts->limit > 0) {
		/* Non-zero limit complicates the logic of entire function a lot. */

		uint32_t chars_count = 0;
		for (uint32_t i = 0; i < wordset_count; i++) {
			const char * word = wordset->words[i];
			chars_count += strlen(word);
		}

		/*
		  Count of characters in a wordset can be lower than limit - this is
		  OK. This can happen when count of groups (words) and count of
		  letters in a group is low, which produces low total count of
		  characters.
		*/
		if (chars_count > opts->limit) {
			(void) fprintf(stderr, "[ERROR] Count of chars in wordset (%u) is higher than limit (%u)\n",
			               chars_count, opts->limit);
			*success = false;
			return 0;
		}

		/* Round down wordset count, discard N repeated words, with
		   last word potentially truncated due to limit. */
		if (wordset_count % times == 0) {
			/*
			  There are 'times' words, but last of them may be truncated:
			  "lf lf lf 24 24 24 8l88 8l88 8l88 j8hkjf j8hkjf j8h"
			  Reject all 'times' words.
			*/
			wordset_count -= times;
		} else {
			const uint32_t rem = wordset_count % times;
			wordset_count -= rem;
		}
	} else {
		if (opts->groups > 0) {
			if (opts->groups * times != wordset_count) {
				(void) fprintf(stderr, "[ERROR] wordset has incorrect count of words: expected %u, has %u\n",
				               opts->groups, wordset_count);
				*success = false;
				return 0;
			}
		} else {
			/* If not specified, cwgen's default groups count is
			   WORDSET_DEFAULT_GROUPS_COUNT. */
			if (WORDSET_DEFAULT_GROUPS_COUNT * times != wordset_count) {
				(void) fprintf(stderr, "[ERROR] wordset has incorrect default count of words: expected %u, has %u\n",
				               WORDSET_DEFAULT_GROUPS_COUNT, wordset_count);
				*success = false;
				return 0;
			}
		}
	}

	uint32_t groupsize_min = UINT32_MAX;
	uint32_t groupsize_max = 0;
	/* Remember that groupsize_h may be zero, as in "not used". */
	const bool expected_exact_size = opts->groupsize_h == 0 || opts->groupsize_h == opts->groupsize;
	const uint32_t exact_groupsize = opts->groupsize == 0 ? WORDSET_DEFAULT_GROUP_SIZE : opts->groupsize;
	/* cwgen was started with options specifying either exact
	   group size, or group size range. */
	for (uint32_t i = 0; i < wordset_count; i++) {
		const char * word = wordset->words[i];
		const size_t len = strlen(word);
		if (expected_exact_size) {
			if (len != exact_groupsize) {
				(void) fprintf(stderr, "[ERROR] word %u ([%s]) has length other than expected %u\n",
				               i, word, exact_groupsize);
				*success = false;
				return 0;
			}
		} else {
			if (len < opts->groupsize || len > opts->groupsize_h) {
				(void) fprintf(stderr, "[ERROR] word %u ([%s]) has length outside of expected <%u-%u> range\n",
				               i, word, opts->groupsize, opts->groupsize_h);
				*success = false;
				return 0;
			}
		}

		if (len > groupsize_max) {
			groupsize_max = len;
		}
		if (len < groupsize_min) {
			groupsize_min = len;
		}
	}
	if (0 != wordset_count) {
		/* wordset_count may be zero if due to a specified opts->limit we
		   reduce count of words to zero. */
		fprintf(stderr, "[INFO ] Tested %u words, detected group sizes in range <%u - %u>\n", wordset_count, groupsize_min, groupsize_max);
	} else {
		fprintf(stderr, "[WARN ] Skipped detection of group sizes because of limited wordset count %u\n", wordset_count);
	}


	if (times > 1) {
		for (uint32_t i = 0; i < wordset_count; i = i + times) {
			for (uint32_t t = 1; t < times; t++) {
				const char * word_a = wordset->words[i];
				const char * word_b = wordset->words[i + t];
#if 1 /* Debug. */
				(void) fprintf(stderr, "[DEBUG] comparing words %2u & %2u: [%s] & [%s]\n", i, i + t, word_a, word_b);
#endif
				if (0 != strcmp(word_a, word_b)) {
					(void) fprintf(stderr, "[ERROR] repeats: word #%u != word #%u + 1: [%s] != [%s]\n",
					               i, i + t, word_a, word_b);
					*success = false;
					return 0;
				}
			}
		}
	}

	const char * the_charset = NULL;
	if (strlen(opts->charset) > 0) {
		the_charset = opts->charset;
	} else {
		the_charset = WORDSET_DEFAULT_CHARSET;
	}
	for (uint32_t i = 0; i < wordset_count; i++) {
		const char * word = wordset->words[i];
		for (size_t j = 0; j < strlen(word); j++) {
			if (NULL == strchr(the_charset, word[j])) {
				(void) fprintf(stderr, "[ERROR] word %u ([%s]) has charcters outside of charset [%s]\n",
				               i, word, the_charset);
				*success = false;
				return 0;
			}
		}
	}

	*success = true;
	return 0;
}




/**
   @brief Read a word from text file into a buffer

   TODO acerion 2023.10.20: move the function to cwutils.

   @p buf is a buffer preallocated by caller.

   @return Status indicating success/failure of reading a word.
 */
static read_word_t read_word(FILE * file, char * buf, size_t size)
{
	size_t pos = 0;
	while (true) {
		char a_char = 0;
		const int n_read = fscanf(file, "%c", &a_char);
		if (n_read == 1) {
			if (isspace(a_char)) {
				if (0 == pos) {
					return read_word_empty;
				} else {
					return read_word_ok;
				}
			}
			buf[pos++] = a_char;
			if (pos == size) {
				(void) fprintf(stderr, "[ERROR] buffer overflow in word buffer\n");
				return read_word_err;
			}
		} else if (feof(file)) {
			if (0 == pos) {
				return read_word_eof;
			} else {
				return read_word_ok;
			}
		} else if (ferror(file)) {
			(void) fprintf(stderr, "[ERROR] error set on text file descriptor: %s\n", strerror(errno));
			return read_word_err;
		} else {
			(void) fprintf(stderr, "[ERROR] unexpected return value for text file descriptor: %d\n", n_read);
			return read_word_err;
		}
	}

	(void) fprintf(stderr, "[ERROR] unexpected return after a loop\n");
	return read_word_err;
}

