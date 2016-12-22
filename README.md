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

---
#sigutils API overview
There are three API levels in sigutils, with every high-level API relying on lower-level APIs:

1. Modem API (which allows to retrieve a stream of symbols from a given signal source)
2. Block API (enables complex stream manipulation using functional blocks in a GNURadio-like fashion)
3. DSP API (low-level API to manipulate samples individually, with things like PLLs, oscillators, etc)

## sigutils type foundation
All API levels share the following set of data types, used to exchange data in a coherent way:

`SUFLOAT`: Default real-valued sample type. Currently maps to `double`.  
`SUSCOUNT`: Used to enumerate samples. Maps to `unsigned long`.  
`SUBOOL`: Boolean type (defined as `int`). Can be either `SU_TRUE` (1) or `SU_FALSE` (0).  
`SUCOMPLEX`: Complex type, composed of a real and an imaginary party, mostly used to store I/Q data. Currently defined to `complex SUFLOAT`.  
`SUSYMBOL`: Data type used for the decision device output. Special values are:
* `SU_NOSYMBOL`: decision device output queue is currently empty, the symbol reader is faster than the demodulator.
* `SU_EOS`: End of stream, the signal source cannot provide any more samples (this happens when the actual underlying device has been disconnected, or when the end of a recorded signal has been reached).

Any other value is considered a valid symbol identifier.


