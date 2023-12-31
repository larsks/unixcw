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

# -include $(top_builddir)/Makefile.inc

# program(s) to be built in current dir
bin_PROGRAMS = cw

# source code files used to build cw program
cw_SOURCES = cw.c cw.h

# target-specific preprocessor flags (#defs and include dirs)
cw_CPPFLAGS = -I$(top_srcdir)/src/ -I$(top_srcdir)/src/libcw/

# Target-specific linker flags (objects to link). Order is important:
# first static library then dynamic. Otherwise linker may not find
# symbols from the dynamic library.
cw_LDADD = $(top_builddir)/src/cwutils/lib_cw.a -L$(top_builddir)/src/libcw/.libs -lcw $(INTL_LIB)



# copy man page to proper directory during installation
man_MANS = cw.1
# and mark it as distributable, too
EXTRA_DIST = cw.1 \
	demo.cw prelude.cw test.cw \
	tests/basic_options.sh



# Test targets.
# This test target will be invoked when creating deb package.
check:

# This test target can be invoked manually.
real_check: all
	( echo "Trying with OSS sound system";					\
	  LD_LIBRARY_PATH=../libcw:$$LD_LIBRARY_PATH ./cw -so -f test.cw ) || 	\
	( echo "Retrying with ALSA sound system";			    	\
	  LD_LIBRARY_PATH=../libcw:$$LD_LIBRARY_PATH ./cw -sa -f test.cw ) ||	\
	( echo "Retrying with console output";				        \
	  LIBCW_DEBUG=1 							\
	  LD_LIBRARY_PATH=../libcw:$$LD_LIBRARY_PATH ./cw -sc -f test.cw )
