\page building Building

**sigutils** has been tested in GNU/Linux (i386, x86_64 and armhf), but it will probably work in many other architectures as well.


## Getting the code

Just clone it from the GitHub repository.

```bash
git clone https://github.com/BatchDrake/sigutils.git
```


## Requirements and dependencies

The following libraries (along with their respective development files) must also be present:

* Meson is required for the build.
* sndfile (1.0.2 or later)
* fftw3 (3.0 or later)
* volk (1.0 or later) (Optional)

### Requirements and dependencies in Ubuntu

```bash
sudo apt-get install meson libsndfile1-dev libvolk2-dev libfftw3-dev
```

### Requirements and dependencies in Archlinux

```bash
sudo pacman -S meson libsndfile libvolk fftw
```

### Requirements and dependencies in Windows MSYS2 MinGW64

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-libsndfile mingw-w64-ucrt-x86_64-fftw mingw-w64-ucrt-x86_64-volk
```


## Building and installing sigutils

### Configuring the project

You can configure the project into a `builddir/` folder by executing the following command. This will check for dependencies automatically.

```bash
meson setup builddir
```

You may want to specify extra options.


### Building

If the previous commands were successful, you can start the build by typing:

```bash
meson compile -C builddir
```


### Installing

You may want to install the built library in your system:

```bash
sudo meson install -C builddir
```
