# Next lines are for non-recursive automake.
# http://karelzak.blogspot.com/2013/02/non-recursive-automake.html
noinst_PROGRAMS=
check_PROGRAMS=
include src/cwutils/wav_state_detector/Makemodule.am
if WITH_CWGEN
include src/cwgen/tests/Makemodule.am
endif




ACLOCAL_AMFLAGS = -I m4

SUBDIRS=src

EXTRA_DIST = \
	icon_unixcw.svg icon_unixcw.xpm \
	unixcw-2.3.spec unixcw-3.6.1.lsm \
	po/UnixCW.po \
	THANKS HISTORY \
	patches # debian




TESTS =
if WITH_CWGEN
TESTS += ./src/cwgen/tests/cwgen_args
endif




clean-local:
# 'noinst' is a directory into which some compilation results are placed.
	rm -rf noinst

