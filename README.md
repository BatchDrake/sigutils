#The sigutils library - Getting started

The sigutils library is a digital signal processing library written in C, designed for blind signal analysis and automatic demodulation in GNU/Linux.

## Requierements and dependencies
sigutils has been tested in GNU/Linux (i386 and x86_64), but it will probably work in many other architectures as well. Autoconf (2.65 or later) and automake (1.11.1 or later) are required to build the library. The following librares (along with their respective development files) must also be present:

* libsndfile (1.0.2 or later)
* alsa (1.0.25 or later)

## Getting the code
Just clone it from the GitHub repository:

```
% git clone https://github.com/BatchDrake/sigutils.git
```

## Building sigutils
First, you must generate the configure script:
```
% autoreconf -fvi
```

After that, you just need to run the standard ./configure and make:
```
% ./configure
% make
```

## Running all unit tests
If the compilation was successful, an executable file named `sigutils` must exist in the `src/` directory, containing a set of unit tests for various library features. It's a good idea to run all unit tests before going on. However, these tests rely on a real signal recording from [sigidwiki](http://www.sigidwiki.com).

In order to run all unit tests, you must download [this ZIP file](http://www.sigidwiki.com/images/3/36/Strange_4psk.zip) first, extract the file `strange 8475 khz_2015-12-04T15-33-32Z_8474.1kHz.wav`, rename it to `test.wav` and place it in the project's root directory. After this step, you can run all unit tests by executing:


```
% src/sigutils
```

