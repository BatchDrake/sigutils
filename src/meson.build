# Find dependencies
dep_thread = dependency('threads')
dep_sndfile = dependency('sndfile', version : '>=1.0.2')
dep_fftw3 = dependency('fftw3', version : '>=3.0')
dep_fftw3f = dependency('fftw3f', version : '>=3.0')
# This is a temporary fix until https://github.com/FFTW/fftw3/issues/180 gets solved
dep_fftw3f_threads = meson.get_compiler('c').find_library('fftw3f_threads', dirs: dep_fftw3.get_variable(pkgconfig: 'libdir'))
dep_volk = dependency('volk', version : '>=1.0', required : false)
dep_math = meson.get_compiler('c').find_library('m', required : false)
dep_ws2 = meson.get_compiler('c').find_library('ws2_32', required : target_machine.system() == 'windows')

# Library
srcs = files([
  'sigutils/specific/apt.c',
  'sigutils/agc.c',
  'sigutils/block.c',
  'sigutils/clock.c',
  'sigutils/coef.c',
  'sigutils/dc_corrector.c',
  'sigutils/detect.c',
  'sigutils/equalizer.c',
  'sigutils/iir.c',
  'sigutils/lfsr.c',
  'sigutils/lib.c',
  'sigutils/log.c',
  'sigutils/matfile.c',
  'sigutils/ncqo.c',
  'sigutils/pll.c',
  'sigutils/property.c',
  'sigutils/smoothpsd.c',
  'sigutils/softtune.c',
  'sigutils/specific/apt.c',
  'sigutils/specttuner.c',
  'sigutils/taps.c',
  'sigutils/tvproc.c',
  'sigutils/version.c',
  'util/util.c',
])

if target_machine.system() == 'windows'
  srcs += files([
    'util/win32-fcntl.c',
    'util/win32-mman.c',
    'util/win32-poll.c',
    'util/win32-pwd.c',
    'util/win32-statvfs.c',
    'util/win32-stdlib.c',
    'util/win32-termios.c',
    'util/win32-time.c',
    'util/win32-unistd.c',
  ])
endif

includes = include_directories([
    'include/',
    # This is a fix. It is not clear why fftw3 include dirs are not picked up by pkg-config...
    dep_fftw3.get_variable(pkgconfig: 'includedir'),
])

c_args = []
if dep_volk.found()
  c_args += '-DHAVE_VOLK'
endif
if not get_option('double_precission')
  c_args += '-D_SU_SINGLE_PRECISION'
endif

lib_sigutils = library('sigutils', srcs,
  soversion : '1',
  include_directories : includes,
  dependencies : [dep_thread, dep_sndfile, dep_fftw3, dep_fftw3f, dep_fftw3f_threads, dep_volk, dep_math, dep_ws2],
  c_args : c_args,
  version : meson.project_version(),
  install : true)

pkg = import('pkgconfig')
pkg.generate(lib_sigutils, extra_cflags : c_args)

# Headers
install_headers(
  files([
    'include/sigutils/agc.h',
    'include/sigutils/block.h',
    'include/sigutils/clock.h',
    'include/sigutils/coef.h',
    'include/sigutils/dc_corrector.h',
    'include/sigutils/decider.h',
    'include/sigutils/defs.h',
    'include/sigutils/detect.h',
    'include/sigutils/equalizer.h',
    'include/sigutils/iir.h',
    'include/sigutils/lfsr.h',
    'include/sigutils/log.h',
    'include/sigutils/matfile.h',
    'include/sigutils/ncqo.h',
    'include/sigutils/pll.h',
    'include/sigutils/property.h',
    'include/sigutils/sampling.h',
    'include/sigutils/sigutils.h',
    'include/sigutils/smoothpsd.h',
    'include/sigutils/softtune.h',
    'include/sigutils/specttuner.h',
    'include/sigutils/taps.h',
    'include/sigutils/tvproc.h',
    'include/sigutils/types.h',
    'include/sigutils/version.h',
  ]),
  subdir  : 'sigutils')

install_headers(
  files([
	  'include/sigutils/specific/apt.h',
  ]),
  subdir  : 'sigutils/specific')

install_headers(
  files([
    'include/sigutils/util/compat-fcntl.h',
    'include/sigutils/util/compat-in.h',
    'include/sigutils/util/compat-inet.h',
    'include/sigutils/util/compat-mman.h',
    'include/sigutils/util/compat-netdb.h',
    'include/sigutils/util/compat-poll.h',
    'include/sigutils/util/compat-pwd.h',
    'include/sigutils/util/compat-select.h',
    'include/sigutils/util/compat-socket.h',
    'include/sigutils/util/compat-stat.h',
    'include/sigutils/util/compat-statvfs.h',
    'include/sigutils/util/compat-stdlib.h',
    'include/sigutils/util/compat-termios.h',
    'include/sigutils/util/compat-time.h',
    'include/sigutils/util/compat-unistd.h',
    'include/sigutils/util/util.h',
    'include/sigutils/util/win32-fcntl.h',
    'include/sigutils/util/win32-in.h',
    'include/sigutils/util/win32-inet.h',
    'include/sigutils/util/win32-mman.h',
    'include/sigutils/util/win32-netdb.h',
    'include/sigutils/util/win32-poll.h',
    'include/sigutils/util/win32-pwd.h',
    'include/sigutils/util/win32-socket.h',
    'include/sigutils/util/win32-stat.h',
    'include/sigutils/util/win32-statvfs.h',
    'include/sigutils/util/win32-stdlib.h',
    'include/sigutils/util/win32-termios.h',
    'include/sigutils/util/win32-time.h',
    'include/sigutils/util/win32-unistd.h',
  ]),
  subdir  : 'sigutils/util')

# Create a dep for the test executables
dep_sigutils = declare_dependency(
  include_directories : includes,
  link_with : lib_sigutils,
  compile_args : c_args,
  dependencies: dep_volk
)
