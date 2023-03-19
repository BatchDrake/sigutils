#!/bin/bash

ver=$1

# create structure
mkdir libsigutils_${ver}_amd64
cd libsigutils_${ver}_amd64
mkdir -p usr/lib/
mkdir -p DEBIAN/

# create debian thing
rm DEBIAN/control
cat <<EOF >>DEBIAN/control
Package: libsigutils
Version: $ver
Section: base
Priority: optional
Architecture: amd64
Depends: libsndfile1 (>= 1.0.31-2), libvolk2.4 (>= 2.4.1-2), libfftw3-3 (>= 3.3.8-2)
Maintainer: arf20 <aruizfernandez05@gmail.com>
Description: Small signal processing utility library
EOF

# copy files
cp ../build/libsigutils* usr/lib/

# set permissions
cd ..
chmod 755 -R libsigutils_${ver}_amd64/

# build deb
dpkg-deb --build libsigutils_${ver}_amd64


# dev
# create structure
mkdir libsigutils-dev_${ver}_amd64
cd libsigutils-dev_${ver}_amd64
mkdir -p usr/lib/pkgconfig/
mkdir -p usr/include/sigutils/sigutils/specific/
mkdir -p usr/include/sigutils/sigutils/util/
mkdir -p DEBIAN/

# create debian thing
rm DEBIAN/control
cat <<EOF >>DEBIAN/control
Package: libsigutils-dev
Version: $ver
Section: base
Priority: optional
Architecture: amd64
Depends: libsndfile1 (>= 1.0.31-2), libvolk2.4 (>= 2.4.1-2), libfftw3-3 (>= 3.3.8-2)
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
chmod 755 -R libsigutils-dev_${ver}_amd64/

# build deb
dpkg-deb --build libsigutils-dev_${ver}_amd64
