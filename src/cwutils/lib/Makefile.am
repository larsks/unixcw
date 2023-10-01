# Copyright (C) 2001-2006  Simon Baldwin (simon_baldwin@yahoo.com)
# Copyright (C) 2011-2023  Kamil Ignacak (acerion@wp.pl)

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
# with this program. If not, see <https://www.gnu.org/licenses/>.

# -include $(top_builddir)/Makefile.inc


noinst_LIBRARIES = libcwutils.a

# https://www.gnu.org/software/automake/manual/html_node/Headers.html:
# "when the header belongs to a single convenience library or program, we
# recommend listing it in the program’s or library’s _SOURCES variable"
libcwutils_a_SOURCES = \
	elements.c elements.h \
	elements_detect.c elements_detect.h \
	element_stats.c element_stats.h \
	misc.c misc.h \
	random.c random.h \
	wav.c wav.h

libcwutils_a_CPPFLAGS = -I$(top_srcdir)/src/libcw/  -I${top_srcdir}/src/



