# Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
# Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

-include $(top_builddir)/Makefile.inc




TESTS = $(check_SCRIPTS)

# targets to be built in this directory
check_PROGRAMS = libcw_tests

# run test programs
check_SCRIPTS = libcw_test_quick.sh

libcw_test_quick.sh:
# empty




# List of files that implement tests of specific bug fixes.
bug_test_files = \
	libcw_test_tq_short_space.c \
	libcw_test_tq_short_space.h

test_framework_files = \
	test_framework.c \
	test_framework.h

modern_api_test_files = \
	libcw_data_tests.c \
	libcw_data_tests.h \
	gen/cw_gen_remove_last_character.c \
	gen/cw_gen_remove_last_character.h \
	gen/cw_gen_enqueue_character_no_ics.c \
	gen/cw_gen_enqueue_character_no_ics.h \
	gen/cw_gen_get_timing_parameters_internal.c \
	gen/cw_gen_get_timing_parameters_internal.h \
	libcw_gen_tests.c \
	libcw_gen_tests.h \
	libcw_gen_tests_state_callback.c \
	libcw_gen_tests_state_callback.h \
	libcw_rec_tests.c \
	libcw_rec_tests.h \
	libcw_utils_tests.c \
	libcw_utils_tests.h \
	libcw_key_tests.c \
	libcw_key_tests.h \
	libcw_debug_tests.c \
	libcw_debug_tests.h \
	libcw_tq_tests.c \
	libcw_tq_tests.h


legacy_api_test_files = \
	legacy/cw_get_receive_parameters.c \
	legacy/cw_get_receive_parameters.h \
	legacy/cw_get_send_parameters.c \
	legacy/cw_get_send_parameters.h \
	libcw_legacy_api_tests.c \
	libcw_legacy_api_tests.h \
	libcw_legacy_api_tests_rec_poll.c \
	libcw_legacy_api_tests_rec_poll.h



# Tests that test some functionality, not a programmer API.
functional_test_files =

# Functional tests that require samples sink file. The file is available only
# when dev support is enabled.
if ENABLE_DEV_PCM_SAMPLES_FILE
functional_test_files+=libcw_gen_tests_debug_pcm_file_timings.c \
	libcw_gen_tests_debug_pcm_file_timings.h
endif



libcw_tests_SOURCES = \
	$(legacy_api_test_files) \
	$(modern_api_test_files) \
	$(functional_test_files) \
	$(bug_test_files) \
	$(test_framework_files) \
	common.c common.h \
	test_data.c \
	test_sets.c \
	test_main.c




libcw_tests_CPPFLAGS  = $(AM_CPPFLAGS) -I$(top_srcdir)/src/cwutils -DLIBCW_UNIT_TESTS
# Part of attempt to detect test functions that aren't called by test program.
# https://stackoverflow.com/a/4857905
libcw_tests_CPPFLAGS += -ffunction-sections




# Target-specific linker flags (objects to link). Order is important:
# first static libraries then dynamic. Otherwise linker may not find
# symbols from the dynamic library.
libcw_tests_LDADD  =
libcw_tests_LDADD += $(top_builddir)/src/cwutils/lib_libcw_tests.a
libcw_tests_LDADD += $(top_builddir)/src/cwutils/lib_rec_tests.a
libcw_tests_LDADD += $(top_builddir)/src/cwutils/lib/libcwutils.a
libcw_tests_LDADD += $(top_builddir)/src/test_framework/basic_utils/lib.a
libcw_tests_LDADD += $(INTL_LIB) -lm -lpthread $(DL_LIB) -L../.libs -lcw_test




# Part of attempt to detect test functions that aren't called by test program.
# https://stackoverflow.com/a/4857905
# Lookg for something like this in output of linker:
#
# libtool: link: gcc -g -O2 [...]
# [...]
# /usr/bin/ld: removing unused section '.text.test_straight_key' in file 'libcw_tests-libcw_key_tests.o'
# [...]
# make[4]: Leaving directory '/home/kamil/unixcw/git_repo/src/libcw/tests'
libcw_tests_LDFLAGS = -Wl,--gc-sections -Wl,--print-gc-sections




EXTRA_DIST = \
	$(check_SCRIPTS) \
	count_functions_under_test.py




# sources, references
#
# source of snippet related to "check_SCRIPTS" and related sh script:
# http://www.freesoftwaremagazine.com/books/agaal/automatically_writing_makefiles_with_autotools
