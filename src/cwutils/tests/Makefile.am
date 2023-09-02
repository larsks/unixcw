# targets to be built in this directory
check_PROGRAMS=cwutils_tests




cwutils_tests_SOURCES=main.c \
	cmdline_combine_arguments.c \
	cmdline_combine_arguments.h
cwutils_tests_CPPFLAGS=-I$(top_srcdir)/src
# TODO acerion 2023.09.02: I decided to re-use existing lib_cw.a (which is
# built for cw program, not for the test program). Re-think building multiple
# convenience libraries in cwutils. Maybe we could have just one, with all
# the functions in it?
cwutils_tests_LDADD = $(top_builddir)/src/cwutils/lib_cw.a -L$(top_builddir)/src/libcw/.libs -lcw




TESTS=cwutils_tests

