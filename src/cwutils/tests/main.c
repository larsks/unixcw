/*
  Copyright (C) 2022-2023  Kamil Ignacak (acerion@wp.pl)

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
   \file main.c

   Main file for a program that tests functions from cwutils directory.
*/




#include <stdio.h>

#include "cmdline_combine_arguments.h"




int main(void)
{
	int ret = 0;
	ret += test_combine_arguments();
	return ret;
}

