#!/bin/bash

PACKAGE="unixcw"
VERSION="3.4.0"


# setup
REPO=`pwd`
cd ../../build
BUILD=`pwd`


echo "REPO = " $REPO
echo "BUILD = " $BUILD

if [ "$REPO" == x"" ]; then
    echo "REPO is empty"
    exit
elif [ "$REPO" == x"/" ]; then
    echo "REPO is root directory"
    exit
fi

if [ "$BUILD" == x"" ]; then
    echo "BUILD is empty"
    exit
elif [ "$BUILD" == x"/" ]; then
    echo "BUILD is root directory"
    exit
fi



# clean up old files, prepare brand new, empty dir
rm -rf $BUILD/*




# prepare $PACKAGE_X.Y.Z.debian.tar.gz
cd $REPO
rm *.tar.gz
make dist

dist=`ls *tar.gz`

mv $dist $BUILD/
cd $BUILD



tar xvfz $dist
dist_dir=`ls | grep -v tar.gz`
echo "dist dir is $dist_dir"


tar cvfz $PACKAGE\_$VERSION.debian.tar.gz $dist_dir/debian
# rm -rf $dist_dir/debian


mv $dist_dir $PACKAGE-$VERSION
tar cvfz $PACKAGE\_$VERSION.orig.tar.gz $PACKAGE-$VERSION --exclude=debian


# go to final build dir and start building Debian package
cd $PACKAGE-$VERSION
echo ""
echo `pwd`
debuild -us -uc