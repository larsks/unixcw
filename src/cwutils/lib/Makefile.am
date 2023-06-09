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


noinst_LIBRARIES = libcwutils.a
libcwutils_a_SOURCES = wav.c misc.c elements.c elements_detect.c element_stats.c



libcwutils_CPPFLAGS = $(AM_CPPFLAGS)


# Target-specific linker flags (objects to link). Order is important:
# first static libraries then dynamic. Otherwise linker may not find
# symbols from the dynamic library.
#libcwutils_LDADD = $(top_builddir)/src/cwutils/lib_libcw_tests.a


