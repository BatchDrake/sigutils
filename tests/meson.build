dep_catch2 = dependency('catch2-with-main', required : false)
if not dep_catch2.found()
    dep_catch2 = dependency('catch2', required : false)
endif

if not dep_catch2.found()
  warning('Testing is disabled because catch2 was not found')
endif

if dep_catch2.found()
    # Test sources
    test_srcs = files([
        'test_ncqo.cpp',
    ])

    # Catch hack to be compatible with v2 and v3...
    test_cpp_args = []
    if dep_catch2.version().version_compare('>3')
        test_cpp_args += '-DCATCH2_V3'
    else
        test_cpp_args += '-DCATCH2_V2'
    endif

    # Test executable
    test_exe = executable('sigutils_test',
        test_srcs,
        cpp_args : test_cpp_args,
        dependencies : [dep_sigutils, dep_catch2],
        build_by_default : false,
        install : false)
    
    # Let meson know what our executable is
    test('Sigutils tests', test_exe)
endif
