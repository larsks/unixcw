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
   @file cwgen_args.c

   Test program for cwgen, testing correspondence between program's optput
   and program's command-line options.

   1. Generate (random) command-line options for cwgen.
   2. Run a cwgen program with the options.
   3. Forward cwgen's output to text file.
   4. Read words (groups) from the text file.
   5. Confirm that the words match the options (count of words (groups),
      count of repeats, length of the words (groups), etc).

   Currently the test program doesn't do any statistical analysis of letters
   in the words (groups) to confirm their randomness.

   TODO acerion 2023.10.20: expand the program to do statistical analysis of
   letters in the words (groups) to confirm their randomness.

   The test program forwards cwgen's output to text file and then reads the
   file. This allows me to "manually" inspect cwgen's output even after the
   program has exited. This is useful when inspecting tests' failure or when
   validating correctness of tests.
*/




#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#if defined(__FreeBSD__)
#include <sys/types.h> /* open() and its flags on FreeBSD. */
#include <sys/stat.h>
#endif
#include <unistd.h>

#include <cwutils/lib/random.h>

#include "wordset.h"




static int get_opt_groups(uint32_t groups, char * buffer, size_t size);
static int get_opt_groupsize(uint32_t groupsize, uint32_t groupsize_h, char * buffer, size_t size);
static int get_opt_repeat(uint32_t repeat, char * buffer, size_t size);
static int get_opt_charset(const char * charset, char * buffer, size_t buffer_size);
static int get_opt_limit(uint32_t limit, char * buffer, size_t size);

static int generate_opts(wordset_opts_t * opts);

static int argv_print(FILE * file, char * argv[], int argc);




int main(void)
{
	cw_random_srand(0);

	char cwgen_path[PATH_MAX + 1] = { 0 };
	if (0 == access("./src/cwgen/cwgen", X_OK)) {
		if (NULL == realpath("./src/cwgen/cwgen", cwgen_path)) {
			(void) fprintf(stderr, "[ERROR] Can't find path to tested binary in src dir: %s\n", strerror(errno));
			return -1;
		}
	} else if (0 == access("../cwgen", X_OK)) {
		if (NULL == realpath("../cwgen", cwgen_path)) {
			(void) fprintf(stderr, "[ERROR] Can't find path to tested binary in parent dir: %s\n", strerror(errno));
			return -1;
		}
	} else {
		(void) fprintf(stderr, "[ERROR] Can't find path to tested binary\n");
		return -1;
	}


	const char * file_path = "/tmp/cwgen_args_test.txt";
	char * argv[10] = { 0 };
	int argc = 0;
	int retv = 0;
	remove(file_path);

	wordset_opts_t opts = { 0 };
	if (0 != generate_opts(&opts)) {
		(void) fprintf(stderr, "[ERROR] failed to generate opts\n");
		return -1;
	}


	/* Build argv for cwgen. */


	argv[argc++] = cwgen_path;


	char buf_groups[30] = { 0 };
	retv = get_opt_groups(opts.groups, buf_groups, sizeof (buf_groups));
	if (0 != retv) {
		(void) fprintf(stderr, "[ERROR] buf_groups: %d\n", retv);
		return -1;
	}
	if (0 != strlen(buf_groups)) {
		argv[argc++] = buf_groups;
	}


	char buf_groupsize[30] = { 0 };
	retv = get_opt_groupsize(opts.groupsize, opts.groupsize_h, buf_groupsize, sizeof (buf_groupsize));
	if (0 != retv) {
		(void) fprintf(stderr, "[ERROR] buf_groupsize: %d\n", retv);
		return -1;
	}
	if (0 != strlen(buf_groupsize)) {
		argv[argc++] = buf_groupsize;
	}


	char buf_repeat[30] = { 0 };
	retv = get_opt_repeat(opts.repeat, buf_repeat, sizeof (buf_repeat));
	if (0 != retv) {
		(void) fprintf(stderr, "[ERROR] buf_repeat: %d\n", retv);
		return -1;
	}
	if (0 != strlen(buf_repeat)) {
		argv[argc++] = buf_repeat;
	}


	char buf_charset[128] = { 0 };
	retv = get_opt_charset(opts.charset, buf_charset, sizeof (buf_charset));
	if (0 != retv) {
		(void) fprintf(stderr, "[ERROR] buf_charset: %d\n", retv);
		return -1;
	}
	if (0 != strlen(buf_charset)) {
		argv[argc++] = buf_charset;
	}


	char buf_limit[30] = { 0 };
	retv = get_opt_limit(opts.limit, buf_limit, sizeof (buf_limit));
	if (0 != retv) {
		(void) fprintf(stderr, "[ERROR] buf_limit: %d\n", retv);
		return -1;
	}
	if (0 != strlen(buf_limit)) {
		argv[argc++] = buf_limit;
	}


#if 1 /* Debug. */
	argv_print(stderr, argv, argc);
#endif


	/* Run cwgen, print output to text file. */
	pid_t pid = fork();
	int out_fd = 0;
	switch (pid) {
	case -1:
		(void) fprintf(stderr, "[ERROR] fork(): %s\n", strerror(errno));
		return -1;
	case 0:
		out_fd = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		dup2(out_fd, 1);
		close(out_fd);
		execv(cwgen_path, argv);
		(void) fprintf(stderr, "[ERROR] exec(): %s\n", strerror(errno));
		return -1;
	default:
		waitpid(pid, NULL, 0);
		break;
	}


	/* Get wordset from the text file into data struct. */
	wordset_t wordset = { 0 };
	FILE * file = fopen(file_path, "r");
	wordset_read_from_file(file, &wordset);
	fclose(file);
#if 1 /* Debug. */
	wordset_printf(stdout, &wordset);
	(void) fprintf(stdout, "\n");
#endif


	/* Validate wordset, handle result of validation. */
	bool success = false;
	if (0 != wordset_validate(&wordset, &opts, &success)) {
		(void) fprintf(stderr, "[ERROR] Failed to validate wordset\n");
		return -1;
	}
	if (!success) {
		(void) fprintf(stderr, "[ERROR] Validation of wordset was unsuccessful\n");
		return -1;
	}
	(void) fprintf(stderr, "[INFO ] Wordset validated, no errors found\n");

	return 0;
}




/**
   @brief Get option string for 'groups' option
*/
static int get_opt_groups(uint32_t groups, char * buffer, size_t size)
{
	buffer[0] = '\0';

	/* Count of groups can be zero: cwgen will then use its default count
	   of groups. */
	if (0 == groups) {
		return 0;
	}

	int n_printed = snprintf(buffer, size, "--groups=%u", groups);
	if (n_printed < 0 || (size_t) n_printed >= size) {
		return -1;
	}

	return 0;
}




/**
   @brief Get option string for 'groupsize' option
*/
static int get_opt_groupsize(uint32_t groupsize, uint32_t groupsize_h, char * buffer, size_t size)
{
	buffer[0] = '\0';

	if (0 == groupsize) {
		return 0;
	}

	if (groupsize_h == 0 || groupsize_h == groupsize) {
		/* 'groupsize_h == 0' means unused 'high' bound. */
		int n_printed = snprintf(buffer, size, "--groupsize=%u", groupsize);
		if (n_printed < 0 || (size_t) n_printed >= size) {
			return -1;
		}
	} else {
		int n_printed = snprintf(buffer, size, "--groupsize=%u-%u", groupsize, groupsize_h);
		if (n_printed < 0 || (size_t) n_printed >= size) {
			(void) fprintf(stderr, "[ERROR] buf_groupsize: %d\n", n_printed);
			return -2;
		}
	}

	return 0;
}




/**
   @brief Get option string for 'repeat' option
*/
static int get_opt_repeat(uint32_t repeat, char * buffer, size_t size)
{
	buffer[0] = '\0';

	/* Count of repeats can be zero: cwgen won't repeat a word. */
	if (0 == repeat) {
		return 0;
	}

	int n_printed = snprintf(buffer, size, "--repeat=%u", repeat);
	if (n_printed < 0 || (size_t) n_printed >= size) {
		return -1;
	}

	return 0;
}




/**
   @brief Get option string for 'charset' option
*/
static int get_opt_charset(const char * charset, char * opt_buffer, size_t opt_buffer_size)
{
	opt_buffer[0] = '\0';

	/* When charset is empty, cwgen shall use its default charset. */
	if (0 == strlen(charset)) {
		return 0;
	}

	int n_printed = snprintf(opt_buffer, opt_buffer_size, "--charset=%s", charset);
	if (n_printed < 0 || (size_t) n_printed >= opt_buffer_size) {
		return -1;
	}

	return 0;
}




/**
   @brief Get option string for 'limit' option
*/
static int get_opt_limit(uint32_t limit, char * buffer, size_t size)
{
	buffer[0] = '\0';

	/* When limit is zero, cwget shall use its default value (no limit). */
	if (0 == limit) {
		return 0;
	}

	int n_printed = snprintf(buffer, size, "--limit=%u", limit);
	if (n_printed < 0 || (size_t) n_printed >= size) {
		return -1;
	}

	return 0;
}




/**
   @brief Generate random values of options

   The values are random, but valid. The values can be used to build options
   strings passed in command line to cwgen.

   @param opts Structure with options used to build cwgen's options strings

*/
static int generate_opts(wordset_opts_t * opts)
{
	/* Randomly generate number of groups. */
	bool use_default_groups = false;
	cw_random_get_bool(&use_default_groups);
	if (use_default_groups) {
		/* Don't specify count of groups for cwgen. */
		opts->groups = 0;
	} else {
		cw_random_get_uint32(1, 30, &opts->groups);
	}



	/* Randomly generate upper and lower bound on group size. */
	bool use_default_groupsize = false;
	cw_random_get_bool(&use_default_groupsize);
	if (use_default_groupsize) {
		/* Don't specify group size for cwgen. */
		opts->groupsize = 0;
		opts->groupsize_h = 0;
	} else {
		const uint32_t size_max = 10;
		cw_random_get_uint32(1, size_max, &opts->groupsize);
		bool equal = false;
		cw_random_get_bool(&equal);
		if (equal) {
			/* Lower and upper (high) bound of group size should be equal,
			   i.e. all generated groups (words) will have the same count of
			   letters. */
			opts->groupsize_h = opts->groupsize;
		} else {
			/* groupsize_h should be higher than groupsize, hence "+1" in
			   first arg. */
			cw_random_get_uint32(opts->groupsize + 1, size_max + 10, &opts->groupsize_h);
		}
	}



	/* Randomly generate count of repeats. */
	bool use_default_repeat = false;
	cw_random_get_bool(&use_default_repeat);
	if (use_default_repeat) {
		/* Don't specify repeat for cwgen. */
		opts->repeat = 0;
	} else {
		cw_random_get_uint32(0, 4, &opts->repeat);
	}



	/* Randomly generate a charset (perhaps empty). */
	bool use_default_charset = false;
	cw_random_get_bool(&use_default_charset);
	if (use_default_charset) {
		/* Don't specify charset explicitly, allow cwgen to use its default charset. */
		opts->charset[0] = '\0';
	} else {
		/* TODO acerion 2023.10.20: generate the random charset. Right now the
		   code uses hardcoded charset. */
		int n_printed = snprintf(opts->charset, sizeof (opts->charset), "%s", "lkjhgfdsa08642");
		if (n_printed < 0 || (size_t) n_printed >= sizeof (opts->charset)) {
			(void) fprintf(stderr, "[ERROR] opts.charset: %d\n", n_printed);
			return -1;
		}
	}



	/* Randomly generate value of limit (may be zero). */
	bool use_default_limit = false;
	cw_random_get_bool(&use_default_limit);
	if (use_default_limit) {
		/* Don't specify limit for cwgen. */
		opts->limit = 0;
	} else {
		cw_random_get_uint32(0, 199, &opts->limit);
	}

	return 0;
}




static int argv_print(FILE * file, char * argv[], int argc)
{
	for (int i = 0; i < argc; i++) {
		(void) fprintf(file, "[INFO ] argv[%d] = [%s]\n", i, argv[i]);
	}
	fflush(file);

	return 0;
}

