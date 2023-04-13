# Building

**sigutils** has been tested in GNU/Linux (i386, x86_64 and armhf), but it will probably work in many other architectures as well.


## Getting the code

Just clone it from the GitHub repository:

```bash
git clone https://github.com/BatchDrake/sigutils.git
```


## Requirements and dependencies

The following libraries (along with their respective development files) must also be present:

* CMake 3.12.0 or higher is required for the build.
* sndfile (1.0.2 or later)
* fftw3 (3.0 or later)
* volk (1.0 or later) (Optional)

### Requirements and dependencies in Ubuntu

```bash
sudo apt-get install cmake libsndfile1-dev libvolk2-dev libfftw3-dev
```

### Requirements and dependencies in Archlinux

```bash
sudo pacman -S libsndfile libvolk fftw cmake
```

### Requirements and dependencies in Windows MSYS2 MinGW64

```bash
pacman -S mingw-w64-x86_64-cc mingw-w64-x86_64-make mingw-w64-x86_64-cmake mingw-w64-x86_64-libsndfile mingw-w64-x86_64-fftw mingw-w64-x86_64-volk
```


## Building and installing sigutils

### Creating and moving to a build environment

First, you must create a build directory:

```bash
mkdir build
cd build
```


### Configuring the project

From the build directory, the project must be configured with CMake.

```bash
cmake ..
```

You may want to specify extra options.


#### Configuring in Windows MSYS2 MinGW64

In Windows you may want to instruct that you want MSYS makefiles and the mingw PREFIX.

```bash
/msys64/mingw64/bin/cmake -G"MSYS Makefiles" -DCMAKE_INSTALL_PREFIX:PATH=/msys64/mingw64 
```


### Building

If the previous commands were successful, you can start the build by typing:

```bash
make
```


### Installing

You may want to install the built library in your system:

```bash
sudo make install
```
