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


# A short and non-complicated name. Makefiles using this lib will be using
# full qualified path which will make the name unambiguous.
noinst_LIBRARIES = lib.a

# https://www.gnu.org/software/automake/manual/html_node/Headers.html:
# "when the header belongs to a single convenience library or program, we
# recommend listing it in the program’s or library’s _SOURCES variable"
lib_a_SOURCES = \
	param_ranger.c param_ranger.h \
	resource_meas.c resource_meas.h \
	test_result.c test_result.h

lib_a_CPPFLAGS = -I$(top_srcdir)/src/libcw/  -I${top_srcdir}/src/



