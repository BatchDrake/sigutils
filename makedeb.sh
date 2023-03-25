#!/bin/bash
#
#  Copyright (C) 2023 Ángel Ruiz Fernández
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as
#  published by the Free Software Foundation, version 3.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this program.  If not, see
#  <http://www.gnu.org/licenses/>
#

if [ "$#" != "1" ]; then
    echo $0: Usage:
    echo "         $0 version"
    exit 1
fi

PKG_VERSION=$1
PKG_ARCH=`dpkg --print-architecture`
PKG_DEPENDS='libsndfile1 (>= 1.0.31-2build1), libvolk2.5 (>= 2.5.1-1), libfftw3-single3 (>= 3.3.8-2)'
PKG_DEV_DEPENDS='libsndfile1-dev (>= 1.0.31-2build1), libvolk2-dev (>= 2.5.1-1), libfftw3-dev (>= 3.3.8-2)'

BINDIR=libsigutils_${PKG_VERSION}_${PKG_ARCH}
DEVDIR=libsigutils-dev_${PKG_VERSION}_${PKG_ARCH}
############################ Binary package ####################################
# create structure
rm -Rf $BINDIR
mkdir $BINDIR
cd $BINDIR
mkdir -p usr/lib/
mkdir -p DEBIAN/

# create debian thing
rm -f DEBIAN/control
cat <<EOF >>DEBIAN/control
Package: libsigutils
Version: $PKG_VERSION
Section: libs
Priority: optional
Architecture: $PKG_ARCH
Depends: $PKG_DEPENDS
Maintainer: arf20 <aruizfernandez05@gmail.com>
Description: Small signal processing utility library
EOF

# copy files
cp ../build/libsigutils* usr/lib/

# set permissions
cd ..
chmod 755 -R $BINDIR/

# build deb
dpkg-deb -Zgzip --build $BINDIR

############################ Development package ###############################
# create structure
rm -Rf $DEVDIR
mkdir $DEVDIR
cd $DEVDIR
mkdir -p usr/lib/pkgconfig/
mkdir -p usr/include/sigutils/sigutils/specific/
mkdir -p usr/include/sigutils/sigutils/util/
mkdir -p DEBIAN/

# create debian thing
rm -f DEBIAN/control
cat <<EOF >>DEBIAN/control
Package: libsigutils-dev
Version: $PKG_VERSION
Section: libdevel
Priority: optional
Architecture: $PKG_ARCH
Depends: libsigutils (= $PKG_VERSION), $PKG_DEV_DEPENDS, pkg-config
Maintainer: arf20 <aruizfernandez05@gmail.com>
Description: Small signal processing utility library development files
EOF

# copy files
cp ../build/sigutils.pc usr/lib/pkgconfig/
cp ../sigutils/*.h usr/include/sigutils/sigutils/
cp ../sigutils/specific/*.h usr/include/sigutils/sigutils/specific
cp ../util/*.h usr/include/sigutils/sigutils/util

# set permissions
cd ..
chmod 755 -R $DEVDIR

# build deb
dpkg-deb --build $DEVDIR