#!/bin/bash

ver=$1

# create structure
mkdir libsigutils_$ver
cd libsigutils_$ver
mkdir -p usr/lib/
mkdir -p DEBIAN/

# create debian thing
rm DEBIAN/control
touch DEBIAN/control
echo 'Package: sigutils' >> DEBIAN/control
echo 'Version: '$ver >> DEBIAN/control
echo 'Section: base' >> DEBIAN/control
echo 'Priority: optional' >> DEBIAN/control
echo 'Architecture: amd64' >> DEBIAN/control
echo 'Depends: libsndfile1 (>= 1.0.31-2), libvolk2.4 (>= 2.4.1-2), libfftw3-3 (>= 3.3.8-2)' >> DEBIAN/control
echo 'Maintainer: arf20 <aruizfernandez05@gmail.com>' >> DEBIAN/control
echo 'Description: Small signal processing utility library' >> DEBIAN/control

# copy files
cp ../build/libsigutils* usr/lib/

# set permissions
cd ..
chmod 755 -R libsigutils_$ver/

# build deb
dpkg-deb --build libsigutils_$ver
