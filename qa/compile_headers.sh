#!/bin/bash




dir=`pwd`




# Compile headers in C code.

files=`find ./src/ -type f -name \*.h | grep -v xcwcp`

includes="-I$dir/ -I$dir/src/ -I$dir/src/libcw -I$dir/src/cwcp -I$dir/src/cwutils -I$dir/src/cwgen -I$dir/src/cw  -I$dir/src/libcw/tests/"

# use -D_GNU_SOURCE because of this:
# https://stackoverflow.com/questions/32672333/including-alsa-asoundlib-h-and-sys-time-h-results-in-multiple-definition-co
# https://stackoverflow.com/questions/43255796/redefinition-of-struct-timeval-in-several-headers

for file in $files;
do
	gcc -c -Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE $includes $file
	rm $file.gch
done




# Compile headers in C++ code.

files=`find ./src/ -type f -name \*.h | grep xcwcp`

QT_INCLUDE_DIR=`pkg-config --variable=includedir Qt5Core`
QT_CFLAGS="-isystem $QT_INCLUDE_DIR"
QT_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtWidgets"
QT_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtGui"
QT_CFLAGS+=" -isystem $QT_INCLUDE_DIR/QtCore"

includes="$QT_CFLAGS -I$dir/ -I$dir/src/ -I$dir/src/libcw -I$dir/src/xcwcp"

for file in $files;
do
	g++ -c -Wall -Wextra -pedantic -fPIC -D_GNU_SOURCE $includes $file
	rm $file.gch
done

