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

SUBDIRS=lib

# targets to be built in this directory
check_PROGRAMS=cw_dictionary_tests

CFLAGS += $(AM_CPPFLAGS)

if ENABLE_DEV_RECEIVER_TEST
CPPFLAGS += -DENABLE_DEV_RECEIVER_TEST
endif



# source code files used to build cw_dictionary_tests program
cw_dictionary_tests_SOURCES = dictionary.c memory.c i18n.c cw_config.c cw_common.c

# target-specific preprocessor flags (#defs and include dirs)
cw_dictionary_tests_CPPFLAGS = $(AM_CPPFLAGS) -DCW_DICTIONARY_UNIT_TESTS -I${top_srcdir}/src/

# Target-specific linker flags (objects to link).
cw_dictionary_tests_LDADD=-L$(top_builddir)/src/libcw/.libs -lcw $(INTL_LIB)

# target-specific compiler flags
cw_dictionary_tests_CFLAGS = -rdynamic



# no header from this dir should be installed
# noinst_HEADERS = cw_cmdline.h cw_copyright.h cw_config.h cw_common.h cw_words.h dictionary.h i18n.h memory.h

# convenience libraries
noinst_LIBRARIES = lib_cw.a lib_cwcp.a lib_cwgen.a lib_xcwcp.a lib_libcw_tests.a lib_rec_tests.a

lib_cw_a_SOURCES          = cw_copyright.h i18n.c i18n.h cw_config.c cw_config.h cw_cmdline.c cw_cmdline.h memory.c memory.h cw_common.c cw_common.h sleep.c sleep.h
lib_cwcp_a_SOURCES        = cw_copyright.h i18n.c i18n.h cw_config.c cw_config.h cw_cmdline.c cw_cmdline.h memory.c memory.h cw_common.c cw_common.h dictionary.c dictionary.h cw_words.h sleep.c sleep.h

lib_xcwcp_a_SOURCES       = cw_copyright.h i18n.c i18n.h cw_config.c cw_config.h cw_cmdline.c cw_cmdline.h memory.c memory.h cw_common.c cw_common.h dictionary.c dictionary.h cw_words.h cw_easy_legacy_receiver.c cw_easy_legacy_receiver.h cw_easy_legacy_receiver_internal.h sleep.c sleep.h
if ENABLE_DEV_RECEIVER_TEST
lib_xcwcp_a_LIBADD        = $(top_builddir)/src/test_framework/basic_utils/lib.a
endif

lib_cwgen_a_SOURCES       = cw_copyright.h i18n.c i18n.h cw_config.c cw_config.h cw_cmdline.c cw_cmdline.h memory.c memory.h sleep.c sleep.h
lib_libcw_tests_a_SOURCES = cw_easy_rec.c cw_easy_rec.h cw_copyright.h i18n.c i18n.h cw_config.c cw_config.h cw_cmdline.c cw_cmdline.h memory.c memory.h cw_common.c cw_common.h sleep.c sleep.h
lib_rec_tests_a_SOURCES   = cw_rec_tester.c cw_rec_tester.h cw_easy_legacy_receiver.c cw_easy_legacy_receiver.h cw_easy_legacy_receiver_internal.h sleep.c sleep.h
lib_rec_tests_a_LIBADD    = $(top_builddir)/src/test_framework/basic_utils/lib.a


# Test targets; no self-test, but make sure all is built.
#check: all


# CLEANFILES extends list of files that need to be removed when
# calling "make clean"
CLEANFILES = greptest.sh



TESTS = $(check_SCRIPTS)

# run test programs (only libcwunittests unit tests suite)
check_SCRIPTS = greptest.sh
greptest.sh:
	echo './cw_dictionary_tests | grep "test result: success"' > greptest.sh
	chmod +x greptest.sh
