\page building Building

**sigutils** has been tested in GNU/Linux (i386, x86_64 and armhf), but it will probably work in many other architectures as well.


## Getting the code

Just clone it from the GitHub repository. Make sure you pass `--recurse-submodules` to `git clone` so all required submodules are also cloned.

```bash
git clone --recurse-submodules https://github.com/BatchDrake/sigutils.git
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

### Configuring the project

You can configure the project into a `build/` folder by executing the following command. This will check for dependencies automatically.

```bash
cmake -B build .
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
cmake --build build
```


### Installing

You may want to install the built library in your system:

```bash
sudo cmake --build build --target install
```
