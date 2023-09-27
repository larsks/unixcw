/*
  Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
  Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)

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
   @file test_result.c

   Code related to result of test functions: pass/fail/something else
*/




#include <stdio.h>

#include "test_result.h"




const char * get_test_result_string(test_result_t result)
{
#define BEGIN_GREEN   "\x1B[32m"
#define BEGIN_RED     "\x1B[31m"
#define END_COLOR     "\x1B[0m"

	/* String literals with 'static storage duration'
	   (https://en.cppreference.com/w/c/language/string_literal). */
	switch (result) {
	case test_result_pass:
		return "["BEGIN_GREEN"PASS"END_COLOR"]";
	case test_result_fail:
		return   "["BEGIN_RED"FAIL"END_COLOR"]";
	default:
		return   "["BEGIN_RED"????"END_COLOR"]";
	}
}


