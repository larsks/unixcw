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

# General initializations.
AC_PREREQ(2.57)

AC_INIT([unixcw], [3.6.1])

# https://www.gnu.org/software/autoconf/manual/autoconf-2.67/html_node/C-Compiler.html
# If your package does not like this default, then it is acceptable to insert
# the line ‘: ${CFLAGS=""}’ after AC_INIT and before AC_PROG_CC to ...
: ${CFLAGS="-O2"}
: ${CXXFLAGS="-O2"}

AC_CONFIG_SRCDIR([src/libcw/libcw_gen.c])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Libtool initialization, added during tests on FreeBSD
LT_INIT

# Specify a configuration header.
AC_CONFIG_HEADERS(src/config.h)

AC_CONFIG_FILES([src/libcw/libcw.pc])

# General checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_LIBTOOL
AC_PROG_RANLIB # Added during tests on FreeBSD
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AM_PROG_CC_C_O

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_VOLATILE




# OS-specific configuration. On top there are default values of some
# flags, to be modified per host type.

# General notice: FreeBSD 9.3 doesn't like empty quotes
# (e.g. DL_LIB=\"\"). Empty string should be totally empty.

# libcw on Linux needs "-ldl" library (dlopen() and related symbols)
# for dynamic loading of ALSA and PulseAudio libraries. Tested BSD
# systems (FreeBSD and OpenBSD) don't have a separate -ldl library
# (and since they don't support ALSA and PulseAudio, libcw on BSDs
# wouldn't even need the library).
DL_LIB=

# libcw on OpenBSD (tested on 5.5) needs -lossaudio for _oss_ioctl
# symbol. No explicit OSS library needed by libcw on FreeBSD and
# Linux. Quick and short googling suggests that NetBSD also uses
# ossaudio library.
# TODO: consider using AC_CHECK_LIB(ossaudio, _oss_ioctl)
OSS_LIB=




# Explicit linking with libintl is needed on:
# - FreeBSD (12.1, 13.2)
# - Alpine Linux (3.18.4)
#
# I'm not using AC_SEARCH_LIBS because I don't want to append libintl through
# LIBS to LDADD of all unixcw programs. Only some programs require linking
# with a library that provides textdomain().
#
# TODO acerion 2023.10.25: check if xcwcp (which doesn't require
# textdomain()), when passed LIBS=-libintl during compilation, links with
# this library at run time.
#
# TODO acerion 2023.10.25: searching for textdomain() should be executed only
# when unixcw programs using textdomain() and friends are enabled for
# compilation. If we only compile libcw + xcwcp, checking for textdomain() is
# completely unnecessary.
#
INTL_LIB=
AC_CHECK_LIB(intl, textdomain, # Try finding the function in separate libintl.
             [INTL_LIB=-lintl], # Success, i18n function found in libintl.
             [AC_CHECK_LIB(c, textdomain,
                           [], # Success, we are probably on OS with glibcs
                           [AC_MSG_ERROR([Can't find 'textdomain' function in neither libc nor libintl, cannot continue.])])]) # Failure. Can't continue.




IS_FREEBSD=false
AC_CANONICAL_HOST
case $host_os in
    *linux*|*Linux*)
	AC_MSG_NOTICE([Detected Linux: $host_os])
	# Needed because of dynamic loading of libraries in libcw.
	DL_LIB=\"-ldl\"
	;;
    *freebsd*|*FreeBSD*)
	AC_MSG_NOTICE([Detected FreeBSD: $host_os])
	IS_FREEBSD=true
	;;
    *openbsd*|*OpenBSD*)
	AC_MSG_NOTICE([Detected OpenBSD: $host_os])
	OSS_LIB=\"-lossaudio\"
	;;
    *netbsd*|*NetBSD*)
	# I didn't test unixcw on NetBSD, but let's give it a
	# chance.
	AC_MSG_NOTICE([Detected NetBSD: $host_os])
	OSS_LIB=\"-lossaudio\"
	;;
    *)
	AC_MSG_NOTICE([Detected other OS: $host_os])
	# Until someone complains, for other OSes we will configure
	# the flags the same way as on Linux.
	DL_LIB=\"-ldl\"
	;;
esac
AM_CONDITIONAL([HOST_IS_FREEBSD], [test $IS_FREEBSD = true])

AC_SUBST(DL_LIB)
AC_SUBST(OSS_LIB)
AC_SUBST(INTL_LIB)




# Check specifically for gzip and ldconfig, and substitute the harmless ":"
# if absent.
AC_PATH_PROG(GZIP, gzip, ,)
if test -z "$GZIP" ; then
  GZIP=":"
fi
AC_PATH_PROG(LDCONFIG, ldconfig, ,)
if test -z "$LDCONFIG" ; then
  LDCONFIG=":"
fi
AC_SUBST(GZIP)
AC_SUBST(LDCONFIG)




# On Alpine Linux 3.17..3.18 using busybox/gzip, the "--best" option doesn't
# exit, and the "-d" and "-{1-9}" don't mix well.
#
# if test -d "unixcw-3.6.1"; then find "unixcw-3.6.1" -type d ! -perm -200 -exec chmod u+w {} ';' && rm -rf "unixcw-3.6.1" || { sleep 5 && rm -rf "unixcw-3.6.1"; }; else :; fi
# case 'unixcw-3.6.1.tar.gz' in \
# *.tar.gz*) \
#  eval GZIP= gzip --best -dc unixcw-3.6.1.tar.gz | ${TAR-tar} xf - ;;\
# *.tar.bz2*) \
# [...]
# esac
# gzip: unrecognized option: best
# BusyBox v1.36.1 (2023-10-09 08:04:29 UTC) multi-call binary.
#
# Usage: gzip [-cfkdt123456789] [FILE]...
# [...]
#
# This snippet from cwdaemon 0.12.0 fixes this.
my_gzip_env="--best"
if gzip --best -d 2>&1 | grep "unrecognized" > /dev/null; then
   my_gzip_env=""
fi
AC_SUBST(GZIP_ENV, $my_gzip_env)




# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# library's current:revision:age
#
# I'm defining it here so that it's available in config.h and can be
# used in libcw's cw_version() function that should return the
# numbers.
LIBCW_VERSION=\"8:0:0\"
AC_DEFINE_UNQUOTED([LIBCW_VERSION], $LIBCW_VERSION, [Library version, libtool notation])
AC_SUBST(LIBCW_VERSION)


# ########################################################################
# definitions of custom command line options for configure script
# ########################################################################


# Build support for console buzzer output? Yes by default.
AC_ARG_ENABLE([console],
    AS_HELP_STRING([--disable-console], [disable support for console buzzer sound output]),
    [],
    [enable_console=yes])

AC_MSG_CHECKING([whether to include console buzzer sound support])
if test "$enable_console" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build support for OSS sound system? Yes by default.
AC_ARG_ENABLE([oss],
    AS_HELP_STRING([--disable-oss], [disable support for OSS sound system output]),
    [],
    [enable_oss=yes])

AC_MSG_CHECKING([whether to include OSS sound system support])
if test "$enable_oss" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build support for ALSA sound system? Yes by default.
AC_ARG_ENABLE([alsa],
    AS_HELP_STRING([--disable-alsa], [disable support for ALSA sound system output]),
    [],
    [enable_alsa=yes])

AC_MSG_CHECKING([whether to include ALSA sound system support])
if test "$enable_alsa" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build support for PulseAudio sound system? Yes by default.
AC_ARG_ENABLE(pulseaudio,
    AS_HELP_STRING([--disable-pulseaudio], [disable support for PulseAudio sound system output]),
    [],
    [enable_pulseaudio=yes])

AC_MSG_CHECKING([whether to include PulseAudio sound system support])
if test "$enable_pulseaudio" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build cwgen? Yes by default.
AC_ARG_ENABLE(cwgen,
    AS_HELP_STRING([--disable-cwgen], [do not build cwgen]),
    [],
    [enable_cwgen=yes])

AC_MSG_CHECKING([whether to build cwgen])
if test "$enable_cwgen" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([WITH_CWGEN], [test $enable_cwgen = "yes"])
WITH_CWGEN=$enable_cwgen


# Build cw? Yes by default.
AC_ARG_ENABLE(cw,
    AS_HELP_STRING([--disable-cw], [do not build cw (application with command line user interface)]),
    [],
    [enable_cw=yes])

AC_MSG_CHECKING([whether to build cw])
if test "$enable_cw" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build cwcp? Yes by default.
AC_ARG_ENABLE(cwcp,
    AS_HELP_STRING([--disable-cwcp], [do not build cwcp (application with curses user interface)]),
    [],
    [enable_cwcp=yes])

AC_MSG_CHECKING([whether to build cwcp])
if test "$enable_cwcp" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


# Build xcwcp? Yes by default.
AC_ARG_ENABLE(xcwcp,
    AS_HELP_STRING([--disable-xcwcp], [do not build xcwcp (application with Qt5 user interface)]),
    [],
    [enable_xcwcp=yes])

AC_MSG_CHECKING([whether to build xcwcp])
if test "$enable_xcwcp" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi




# Include test code in receiver code? This will mostly matter for "receiver"
# functionality of xcwcp: xcwcp will get an additional option in "modes"
# menu, with which a receiver can be auto-tested.
AC_ARG_ENABLE(dev_receiver_test,
    AS_HELP_STRING([--enable-dev-receiver-test], [enable test code embedded in receiver]),
    [],
    [enable_dev_receiver_test=no])

AC_MSG_CHECKING([whether to enable test code embedded in receiver])
if test "$enable_dev_receiver_test" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([ENABLE_DEV_RECEIVER_TEST], [test $enable_dev_receiver_test = "yes"])




# Additional debugs in libcw.
AC_ARG_ENABLE(dev_libcw_debugging,
    AS_HELP_STRING([--enable-dev-libcw-debugging], [enable additional debugs in libcw (messages/debug code/asserts)]),
    [],
    [enable_dev_libcw_debugging=no])

AC_MSG_CHECKING([whether to enable additional debugs in libcw])
if test "$enable_dev_libcw_debugging" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(ENABLE_DEV_LIBCW_DEBUGGING, test "$enable_dev_libcw_debugging" = "yes")
if test "$enable_dev_libcw_debugging" = "yes" ; then
   AC_DEFINE([ENABLE_DEV_LIBCW_DEBUGGING], [1], [Define as 1 if you want to enable additional debugs in libcw.])
fi

if test "$ENABLE_DEV_LIBCW_DEBUGGING" = 'yes' ; then
    LIBCW_NDEBUG=""
else
    LIBCW_NDEBUG="-DNDEBUG"
fi
AC_SUBST(LIBCW_NDEBUG)




# Enable writing raw PCM samples from libcw generator to disc file for
# debugging purposes?
AC_ARG_ENABLE(dev_pcm_samples_file,
    AS_HELP_STRING([--enable-dev-pcm-samples-file], [enable writing raw PCM samples to debug file)]),
    [],
    [enable_dev_pcm_samples_file=no])

AC_MSG_CHECKING([whether to enable writing raw PCM samples to debug file])
if test "$enable_dev_pcm_samples_file" = "yes" ; then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AM_CONDITIONAL([ENABLE_DEV_PCM_SAMPLES_FILE], test "$enable_dev_pcm_samples_file" = "yes")
if test "$enable_dev_pcm_samples_file" = "yes" ; then
   AC_DEFINE([ENABLE_DEV_PCM_SAMPLES_FILE], [1], [Define as 1 if you want to enable writing raw PCM samples to debug file.])
fi




# #####
#  end
# #####





# ########################################################################
# Process command line options, and if enabled - checking if requirements
# for enabled features are satisfied.
# ########################################################################


if test "$enable_console" = "no" ; then
    WITH_CONSOLE='no'
else
     # Search for header that provides KIOCSOUND
    AC_CHECK_HEADERS([sys/kd.h sys/vtkd.h sys/kbio.h])
    if test "$ac_cv_header_sys_kd_h" = 'no' \
	&& test "$ac_cv_header_sys_vtkd_h" = 'no' \
	&& test "$ac_cv_header_sys_kbio_h" = 'no' ; then

	WITH_CONSOLE='no'
	AC_MSG_WARN([Cannot find either sys/kd.h, sys/vtkd.h, or sys/kbio.h - support for console buzzer sound output will be disabled])
    else
	WITH_CONSOLE='yes'
    fi
fi

if test "$WITH_CONSOLE" = 'yes' ; then
    AC_DEFINE([LIBCW_WITH_CONSOLE], [1], [Define as 1 if your build machine can support console buzzer.])
fi



if test "$enable_oss" = "no" ; then
    WITH_OSS='no'
else
    AC_CHECK_HEADERS([soundcard.h sys/soundcard.h])
    if test "$ac_cv_header_soundcard_h" = 'no' \
	&& test "$ac_cv_header_sys_soundcard_h" = 'no' ; then

	WITH_OSS='no'
        AC_MSG_WARN([Cannot find either soundcard.h or sys/soundcard.h - support for OSS sound system will be disabled])
    else
	WITH_OSS='yes'
    fi
fi

if test "$WITH_OSS" = 'yes' ; then
    AC_DEFINE([LIBCW_WITH_OSS], [1], [Define as 1 if your build machine can support OSS.])
fi



if test "$enable_alsa" = "no" ; then
    WITH_ALSA='no'
else
    AC_CHECK_LIB(asound, snd_pcm_open)
    if test "$ac_cv_lib_asound_snd_pcm_open" = 'yes' ; then

	# Check for certain routines which are missing from liboss-salsa-dev
        # (kfreebsd's libasound implementation). If a function is found
	# build system defines HAVE_function_name in config.h.
	AC_CHECK_FUNCS([ \
	    snd_pcm_hw_params_test_periods \
	    snd_pcm_hw_params_test_buffer_size \
	    ])

	WITH_ALSA='yes'
    else
	WITH_ALSA='no'
	AC_MSG_WARN([Cannot find ALSA library files - support for ALSA sound system will be disabled])
    fi
fi

if test "$WITH_ALSA" = 'yes' ; then
    AC_DEFINE([LIBCW_WITH_ALSA], [1], [Define as 1 if your build machine can support ALSA.])
fi



if test "$enable_pulseaudio" = "no" ; then
    WITH_PULSEAUDIO='no'
else
    AC_CHECK_LIB(pulse-simple, pa_simple_new)
    if test "$ac_cv_lib_pulse_simple_pa_simple_new" = 'yes' ; then

	WITH_PULSEAUDIO='yes'
    else
	WITH_PULSEAUDIO='no'
	AC_MSG_WARN([Cannot find PulseAudio library files - support for PulseAudio sound system will be disabled])
    fi
fi

if test "$WITH_PULSEAUDIO" = 'yes' ; then
    AC_DEFINE([LIBCW_WITH_PULSEAUDIO], [1], [Define as 1 if your build machine can support PulseAudio.])
fi


WITH_CWGEN=$enable_cwgen
WITH_CW=$enable_cw


if test "$enable_cwcp" = "no" ; then
   WITH_CWCP='no'
else
    AC_CHECK_LIB(curses, initscr)
    if test $ac_cv_lib_curses_initscr = 'yes' ; then
	WITH_CWCP='yes'
    else
	WITH_CWCP='no'
	AC_MSG_WARN([Cannot find libcurses - unable to build cwcp])
    fi
fi


if test "$enable_xcwcp" = "no" ; then
    WITH_XCWCP='no'
else
    # http://stackoverflow.com/questions/5178511/integrate-qt-project-with-autotool
    PKG_CHECK_MODULES(QT5, [Qt5Widgets Qt5Gui Qt5Core >= 5.0.0], [
			   AC_PATH_PROGS(MOC, [moc-qt5 moc], moc,`eval $PKG_CONFIG --variable=host_bins Qt5Core`)

			   # https://stackoverflow.com/questions/11663702/how-to-suppress-warnings-for-file-included-from-header
			   QT_INCLUDE_DIR=`$PKG_CONFIG --variable=includedir Qt5Core`
			   QT5_CFLAGS="-isystem $QT_INCLUDE_DIR"
			   QT5_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtWidgets"
			   QT5_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtGui"
			   QT5_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtCore"

			   AC_MSG_RESULT([QT5 CLFAGS is $QT5_CFLAGS])
			   AC_MSG_RESULT([QT5 LIBS is $QT5_LIBS])

			   AC_SUBST(QT5_CFLAGS)
			   AC_SUBST(QT5_LIBS)
			   AC_SUBST(MOC)

			   WITH_XCWCP='yes'

			   ], [

			   AC_MSG_WARN([Cannot find Qt5 files - unable to build xcwcp])

			   WITH_XCWCP='no'

			   ])
fi






# #####
#  end
# #####



# unixcw uses per-target linker object lists (target_LDADD)
LIBS=



# Checks for header files
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h sys/ioctl.h \
                  sys/param.h sys/time.h unistd.h locale.h libintl.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([string.h strings.h])
if test "$ac_cv_header_string_h" = 'no' \
    && test "$ac_cv_header_strings_h" = 'no' ; then
  AC_MSG_WARN([Cannot find either string.h or strings.h])
fi




# Strange. If AC_CHECK_FUNCS[floorf...] is not preceded directly by
# AC_CHECK_LIB[m], the AC_CHECK_FUNCS can't find the math libraries.
AC_CHECK_LIB(m, floorf, [], [AC_MSG_ERROR([math library is missing, cannot continue])])
AC_CHECK_FUNCS([floorf fabsf sinf cosf sqrtf], [], [AC_MSG_ERROR([math function is missing, cannot continue])])

AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread library is missing, cannot continue])])

AC_CHECK_FUNCS([getopt_long])

AC_CHECK_FUNCS([strtoul strchr strdup strrchr strspn], [], [AC_MSG_ERROR([memory function is missing, cannot continue])])

AC_CHECK_FUNCS([malloc realloc memset memmove], [], [AC_MSG_ERROR([memory function is missing, cannot continue])])

AC_CHECK_FUNCS([gettimeofday setlocale select strerror], [], [AC_MSG_ERROR([a function is missing, cannot continue])])




# Decide on which subdirectories to build; substitute into SRC_SUBDIRS.
# Build cwcp if curses is available, and xcwcp if Qt is available.
# libcw tests depend on static library from cwutils, so cwutils goes before libcw.
SRC_SUBDIRS="test_framework cwutils libcw"

if test "$WITH_CWGEN" = 'yes' ; then
    SRC_SUBDIRS="$SRC_SUBDIRS cwgen"
fi

if test "$WITH_CW" = 'yes' ; then
    SRC_SUBDIRS="$SRC_SUBDIRS cw"
fi

if test "$WITH_CWCP" = 'yes' ; then
    SRC_SUBDIRS="$SRC_SUBDIRS cwcp"
fi

if test "$WITH_XCWCP" = 'yes' ; then
    SRC_SUBDIRS="$SRC_SUBDIRS xcwcp"
fi

AC_SUBST(SRC_SUBDIRS)



# Used for debug purposes.
# CFLAGS=`echo "$CFLAGS" | sed 's/-O2/-O0/g'`



GCC_WARNINGS="-Wall -Wextra -Wswitch-default"
GCC_C_WARNINGS="-Wimplicit-function-declaration"
# For silencing warnings about format with localization macro: printf(_("")).
GCC_WARNINGS="$GCC_WARNINGS -Wno-format-nonliteral"

# For silencing warnings about libcw's deprecated symbols. As long as they
# stay in libcw I still need to use them and test them.
GCC_WARNINGS="$GCC_WARNINGS -Wno-deprecated-declarations"

GCC_WARNINGS="$GCC_WARNINGS -Wswitch-enum"

# Inspired by Krister Walfridsson's blog post
GCC_WARNINGS="$GCC_WARNINGS -Wduplicated-cond -Wlogical-op"
GCC_WARNINGS="$GCC_WARNINGS -Wnull-dereference"
GCC_WARNINGS="$GCC_WARNINGS -Wdouble-promotion -Wshadow -Wformat=2"
# GCC_WARNINGS="$GCC_WARNINGS -Wrestrict -Wduplicated-branches " # Not supported yet on my PC
GCC_C_WARNINGS="$GCC_C_WARNINGS -Wjump-misses-init"

# Add -Wall, -Wextra, -pedantic, and other paranoia to gcc command
# flags, and -Wall to with a few suppressions to icc command flags,
# then similarly, though less so, for g++ and icpc.
if test "`basename $CC`" = "gcc" ; then
  CFLAGS="$CFLAGS -std=c99"
  # SUSv3
  # CFLAGS="$CFLAGS -D_XOPEN_SOURCE=600"
  CFLAGS="$CFLAGS -D_GNU_SOURCE -D_FORTIFY_SOURCE=1"
  CFLAGS="$CFLAGS $GCC_WARNINGS $GCC_C_WARNINGS -pedantic -Wfloat-equal -Wpointer-arith"
  CFLAGS="$CFLAGS -Wcast-qual -Wwrite-strings -Wmissing-prototypes"
  CFLAGS="$CFLAGS -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls"
else
  if test "`basename $CC`" = "icc" ; then
    CFLAGS="$CFLAGS -Wall -wd181,188,279,383,593,810,981,1469"
  fi
fi
if test "`basename $CXX`" = "g++" ; then
  # -Wno-variadic-macros is to silence g++'s warnings about C99
  # variadic macros from libcw_debug.h
  CXXFLAGS="$CXXFLAGS $GCC_WARNINGS -pedantic -Wno-long-long -Wno-variadic-macros -D_FORTIFY_SOURCE=1"
else
  if test "`basename $CXX`" = "icpc" ; then
    CXXFLAGS="$CXXFLAGS -Wall -wd181,188,279,383,593,810,981,1469"
  fi
fi

# Determine if -fPIC or -KPIC is available for building .so libraries.
# Because gcc complains about invalid flags, but then continues, we have to
# check by searching the compile stdout and stderr for any output.
if test -z "$CFLAG_PIC" ; then
  AC_MSG_CHECKING([for -KPIC or -fPIC compiler options])
  cat >conftest.c <<-EOF
  int so_test() { return 0; }
EOF
  if $CC -fPIC -c conftest.c 2>&1 | egrep -q '.' ; then
    if $CC -KPIC -c conftest.c 2>&1 | egrep -q '.' ; then
      CFLAG_PIC=""
    else
      CFLAG_PIC="-KPIC"
    fi
  else
    CFLAG_PIC="-fPIC"
  fi
  rm -f conftest.c conftest.o
  if test -n "$CFLAG_PIC" ; then
    AC_MSG_RESULT($CFLAG_PIC)
  else
    AC_MSG_RESULT(no)
  fi
fi
AC_SUBST(CFLAG_PIC)

# Determine if the C compiler builds DSO files, by testing with -shared.
CC_LINKS_SO="no"
if test -n "$CC" ; then
  AC_MSG_CHECKING(whether cc builds .so files with -shared)
  cat >conftest.c <<-EOF
  int so_test() { return 0; }
EOF
  $CC -c conftest.c >/dev/null 2>/dev/null
  $CC -shared -o conftest.so conftest.o >/dev/null 2>/dev/null
  rm -f conftest.c conftest.o
  if test -f conftest.so ; then
    nm conftest.so | grep -q so_test
    if test $? -eq 0 ; then
      CC_LINKS_SO="yes"
    fi
  fi
  rm -f conftest.so
  if test $CC_LINKS_SO = "yes" ; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
fi
AC_SUBST(CC_LINKS_SO)

# A linker might also build DSO files with -G, but we'll prefer to use the
# compiler if it will cooperate.
LD_LINKS_SO="no"
AC_PATH_PROG(LD, ld, , )
if test -n "$LD" ; then
  AC_MSG_CHECKING(whether ld builds .so files with -G)
  cat >conftest.c <<-EOF
  int so_test() { return 0; }
EOF
  $CC -c conftest.c >/dev/null 2>/dev/null
  $LD -G -o conftest.so conftest.o >/dev/null 2>/dev/null
  rm -f conftest.c conftest.o
  if test -f conftest.so ; then
    nm conftest.so | grep -q so_test
    if test $? -eq 0 ; then
      LD_LINKS_SO="yes"
    fi
  fi
  rm -f conftest.so
  if test $LD_LINKS_SO = "yes" ; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
else
  LD=":"
fi
AC_SUBST(LD)
AC_SUBST(LD_LINKS_SO)





AC_CONFIG_FILES([Makefile.inc
	Makefile
	src/Makefile
	src/libcw/Makefile
	src/libcw/tests/Makefile
	src/cwutils/Makefile
	src/cwutils/lib/Makefile
	src/test_framework/Makefile
	src/test_framework/basic_utils/Makefile
	])

if test "$WITH_CWGEN" = 'yes' ; then
   AC_CONFIG_FILES([src/cwgen/Makefile])
fi
if test "$WITH_CW" = 'yes' ; then
   AC_CONFIG_FILES([src/cw/Makefile])
fi
if test "$WITH_CWCP" = 'yes' ; then
   AC_CONFIG_FILES([src/cwcp/Makefile])
fi
if test "$WITH_XCWCP" = 'yes' ; then
   AC_CONFIG_FILES([src/xcwcp/Makefile])
fi




AC_OUTPUT




# Put this at the very end, so that these are the last lines seen by
# user on console with low number of rows.
AC_MSG_NOTICE([----------------------------------------------------------------------])
AC_MSG_NOTICE([------------------ $PACKAGE_NAME $PACKAGE_VERSION build configuration ------------------])
AC_MSG_NOTICE([  operating system:  .....................................  $host_os])
AC_MSG_NOTICE([  build libcw:  ..........................................  yes])
AC_MSG_NOTICE([      libcw soname version:  .............................  $LIBCW_VERSION])
AC_MSG_NOTICE([      include console buzzer support:  ...................  $WITH_CONSOLE])
AC_MSG_NOTICE([      include OSS support:  ..............................  $WITH_OSS])
AC_MSG_NOTICE([      include ALSA support:  .............................  $WITH_ALSA])
AC_MSG_NOTICE([      include PulseAudio support:  .......................  $WITH_PULSEAUDIO])
AC_MSG_NOTICE([  build cw:  .............................................  $WITH_CW])
AC_MSG_NOTICE([  build cwgen:  ..........................................  $WITH_CWGEN])
AC_MSG_NOTICE([  build cwcp:  ...........................................  $WITH_CWCP])
AC_MSG_NOTICE([  build xcwcp:  ..........................................  $WITH_XCWCP])
if test "$WITH_XCWCP" = 'yes' ; then
    AC_MSG_NOTICE([      Qt5 LIBS:  .........................................  $QT5_LIBS])
    AC_MSG_NOTICE([      Qt5 MOC:  ..........................................  $MOC])
    AC_MSG_NOTICE([      Qt5 CFLAGS:  .......................................  $QT5_CFLAGS])
fi
AC_MSG_NOTICE([  CFLAGS:  ...............................................  $CFLAGS])

AC_MSG_NOTICE([  development support options:])
AC_MSG_NOTICE([      enable test code embedded in receiver: .............  $enable_dev_receiver_test])
AC_MSG_NOTICE([      enable additional debugs in libcw: .................  $enable_dev_libcw_debugging])
AC_MSG_NOTICE([      enable writing raw PCM samples to debug file: ......  $enable_dev_pcm_samples_file])

AC_MSG_NOTICE([----------------------------------------------------------------------])

