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

SUBDIRS=tests

-include $(top_builddir)/Makefile.inc

# Targets to be built in this directory. Only libcw.X should be
# installed. Test library should not be installed.
lib_LTLIBRARIES=libcw.la
noinst_LTLIBRARIES=libcw_test.la
man_MANS=libcw.3 cw.7
pkgconfig_DATA=libcw.pc


EXTRA_DIST=include.awk libdoc.awk libfuncs.awk libpc.awk libsigs.awk \
	libcw.3.m4 \
	libcw.pc.in \
	cw.7 \
	Doxyfile

# These files are used to build two different targets - list them only
# once. I can't compile these files into an utility library because
# the two targets are compiled with different CPPFLAGS.
#
# https://www.gnu.org/software/automake/manual/html_node/Program-Sources.html
# "Header files listed in a _SOURCES definition will be included in the
# distribution but otherwise ignored."
LIBCW_SOURCE_FILES = \
	libcw.c \
	libcw_gen.c libcw_gen.h libcw_gen_internal.h \
	libcw_rec.c libcw_rec.h libcw_rec_internal.h \
	libcw_tq.c libcw_tq.h libcw_tq_internal.h \
	libcw_data.c libcw_data.h \
	libcw_key.c libcw_key.h \
	libcw_utils.c libcw_utils.h \
	libcw_signal.c libcw_signal.h \
	libcw_null.c libcw_null.h \
	libcw_console.c libcw_console.h \
	libcw_oss.c libcw_oss.h \
	libcw_alsa.c libcw_alsa.h \
	libcw_pa.c libcw_pa.h \
	libcw_debug.c libcw_debug_internal.h





# target: shared library

# source code files used to build libcw shared library
libcw_la_SOURCES = $(LIBCW_SOURCE_FILES)

# target-specific linker flags (objects to link)
libcw_la_LIBADD=-lm -lpthread $(DL_LIB) $(OSS_LIB)

# target-specific linker flags (additional flags)
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# LIBCW_VERSION is defined in configure.ac
libcw_la_LDFLAGS = -version-number $(LIBCW_VERSION)

# target-specific compiler flags
libcw_la_CFLAGS = -rdynamic

# target-specific preprocessor flags (#defs and include dirs)
#
# $(LIBCW_NDEBUG) activates asserts in base libcw for dev builds, and
# deactivates asserts in base libcw for regular builds.
libcw_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBCW_NDEBUG) -I${top_srcdir}/src/





libcw_includedir=$(includedir)
libcw_include_HEADERS = libcw.h libcw2.h libcw_debug.h





# target: shared library for tests

# source code files used to build libcw shared library
libcw_test_la_SOURCES = $(LIBCW_SOURCE_FILES)

# target-specific linker flags (objects to link)
libcw_test_la_LIBADD=-lm -lpthread $(DL_LIB) $(OSS_LIB)

# target-specific linker flags (additional flags)
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# LIBCW_VERSION is defined in configure.ac
libcw_test_la_LDFLAGS = -version-number $(LIBCW_VERSION)

# target-specific compiler flags
libcw_test_la_CFLAGS = -rdynamic

# target-specific preprocessor flags (#defs and include dirs)
#
# $(LIBCW_NDEBUG) activates asserts in base libcw for dev builds, and
# deactivates asserts in base libcw for regular builds.
libcw_test_la_CPPFLAGS = $(AM_CPPFLAGS) $(LIBCW_NDEBUG) -DLIBCW_UNIT_TESTS





# target: libcw man page
libcw.3: libcw.3.m4
	cat `LC_ALL=C ls $(top_srcdir)/src/libcw/*.c` | $(AC_AWK) -f $(top_srcdir)/src/libcw/libdoc.awk | $(AC_AWK) -f $(top_srcdir)/src/libcw/libsigs.awk  > signatures
	cat `LC_ALL=C ls $(top_srcdir)/src/libcw/*.c` | $(AC_AWK) -f $(top_srcdir)/src/libcw/libdoc.awk | $(AC_AWK) -f $(top_srcdir)/src/libcw/libfuncs.awk > functions
	$(AC_AWK) -f $(top_srcdir)/src/libcw/include.awk < $(top_srcdir)/src/libcw/libcw.3.m4 >libcw.3
	rm -f signatures functions





# target: libcw.pc
pkgconfigdir = $(libdir)/pkgconfig




# CLEANFILES extends list of files that need to be removed when
# calling "make clean"
CLEANFILES = libcw.3




