# The sigutils library - Getting started

The sigutils library is a digital signal processing library written in C, designed for blind signal analysis and automatic demodulation in GNU/Linux.

## Requirements and dependencies
**sigutils** has been tested in GNU/Linux (i386, x86_64 and armhf), but it will probably work in many other architectures as well. CMake 3.7.2 or higher is required for the build. The following libraries (along with their respective development files) must also be present:

* sndfile (1.0.2 or later)
* fftw3 (3.0 or later)

Optional **volk** support requires version 1.0 or higher.

## Getting the code
Just clone it from the GitHub repository:

```
% git clone https://github.com/BatchDrake/sigutils.git
```

## Building and installing sigutils
First, you must create a build directory and configure it with:

```
% mkdir build
% cd build
% cmake ..
```

If the previous commands were successful, you can start the build by typing:

```
% make
```

And proceed to install the library in your system by running as root:

```
# make install
# ldconfig
```

## Running all unit tests
~~If the compilation was successful, an executable file named `sigutils` must exist in the `src/` directory, containing a set of unit tests for various library features. It's a good idea to run all unit tests before going on. However, these tests rely on a real signal recording from [sigidwiki](http://www.sigidwiki.com).~~

~~In order to run all unit tests, you must download [this ZIP file](http://www.sigidwiki.com/images/3/36/Strange_4psk.zip) first, extract the file `strange 8475 khz_2015-12-04T15-33-32Z_8474.1kHz.wav`, rename it to `test.wav` and place it in the project's root directory. After this step, you can run all unit tests by executing:~~

_Please note: many of these unit tests are failing until the API harmonization work is completed. Don't worry to much about them for now. However, if both your build and installation were successful, you should see a list of unit tests taking place after running:_

```
% src/sutest
```

---
# sigutils API overview
There are three API levels in sigutils, with every high-level API relying on lower-level APIs:

1. Modem API (which allows to retrieve a stream of symbols from a given signal source)
2. Block API (enables complex stream manipulation using functional blocks in a GNU Radio-like fashion)
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


## The modem API
The core of the modem API is the `su_modem_t` object, which can be configured to retrieve discrete symbols from a variety of sources and modulations. Currently, only the QPSK modem, WAV file source and generic block source are implemented.

### Creating a QPSK modem
Modems are created using the modem constructor `su_modem_new`, which accepts the class name of the specific modem to instantiate:

```
su_modem_t *modem;  
	
if ((modem = su_modem_new("qpsk")) == NULL) {  
  fprintf(stderr, "su_modem_new: failed to initialize QPSK modem\n");  
  exit(EXIT_FAILURE);  
}
```

### Configuring the modem
Once an appropriate modem type has been initialized, it must be configured before reading samples from it. Two actions are required to configure a modem: setting the signal source and setting modem parameters. If the signal source is a WAV file, this can be configured by calling `su_modem_set_wav_source`:

```
if (!su_modem_set_wav_source(modem, "test.wav")) {
  fprintf(
      stderr,
      "su_modem_set_wav_source: failed to set modem wav source to test.wav\n");
  exit(EXIT_FAILURE);
}
```

Alternatively, arbitrary block sources can be specified using `su_modem_set_source`:

```
su_block_t *source_block = /* initialize source */;

if (!su_modem_set_source(modem, source_block)) {
  fprintf(
      stderr,
      "su_modem_set_source: failed to set modem source\n");
  exit(EXIT_FAILURE);
}
```

Modem parameters are configured through the `su_modem_set_*` methods. Although every modem can accept a different set of parameters, they generally use the same naming convention for equivalent parameters:

```
su_modem_set_bool(modem, "abc", SU_FALSE); /* Automatic baud rate control */
su_modem_set_bool(modem, "afc", SU_TRUE); /* Automatic frequency control */
su_modem_set_int(modem, "mf_span", 4); /* Matched filter span (in symbols) */
su_modem_set_float(modem, "baud", 468); /* Baud rate: 468 baud */
su_modem_set_float(modem, "fc", 910); /* Carrier frequency: 910 Hz */
su_modem_set_float(modem, "rolloff", .25); /* Roll-off factor of the matched filter */
```

Additionally, if the WAV source is being used, the `int` parameter `samp_rate` is automatically initialized, matching the WAV file sample rate. Otherwise, this parameter must be configured manually.

After both the source and modem parameters have been properly initialized, the modem can be *switched on* by calling `su_modem_start`:

```
if (!su_modem_start(modem)) {
  fprintf(stderr, "su_modem_start: failed to start modem\n");
  exit(EXIT_FAILURE);
}
```

This tells the modem to preallocate any parameter-dependent temporary data to start demodulation. Note that this method itself will not start demodulation. Demodulation happens in an *on demand* basis when a symbol read operation is requested. 

### Reading symbols
After starting the modem, symbols can be read by looping over `su_modem_read`:

```
SUSYMBOL sym;

/* While we don't reach the end of the stream */
while((sym = su_modem_read(modem)) != SU_EOS) {
  if (sym != SU_NOSYMBOL) {
    printf("Got symbol: %d\n", sym);
  }
}

```

Symbols are retrieved as an integer value, starting from 1. If the modem considers there is too much noise to make a good decision, it will return `SU_NOSYMBOL`. If the source cannot provide any more samples, `su_modem_read` will return `SU_EOS`.

### Deleting the modem
Once we are done using the modem object, it must be released using the `su_modem_destroy` method:

```
su_modem_destroy(modem);
```

## Building and installing sigutils under Windows
Under MSYS2 MinGW64
First, you must create a build directory and configure it with:

```
% mkdir build
% cd build
% cmake -G "MSYS Makefiles" ..
```

If the previous commands were successful, you can start the build by typing:

```
% cmake --build .
```

And proceed to install the library in your system by running as root:

```
# make install
```
