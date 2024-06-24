dep_doxygen = find_program('doxygen', required : false)
dep_python = import('python').find_installation(required : false)
dep_coverxygen = run_command(dep_python, '-c', 'import importlib.util; import sys; exit(1) if importlib.util.find_spec(\'coverxygen\') is None else exit(0)', check : false).returncode() == 0

if not dep_doxygen.found()
  warning('Documentation disabled because doxygen was not found')
endif

if not dep_coverxygen
  warning('Documentation coverage report disabled because coverxygen was not found')
endif

if dep_doxygen.found()
  # Doxygen input folders...
  doxygen_input = [
    meson.current_source_dir(),
    join_paths(meson.current_source_dir(), '../src'),
    join_paths(meson.current_source_dir(), '../src/include'),
    join_paths(meson.current_source_dir(), '../src/include/sigutils'),
    join_paths(meson.current_source_dir(), '../src/include/sigutils/specific'),
    join_paths(meson.current_source_dir(), '../src/include/sigutils/util'),
    join_paths(meson.current_source_dir(), '../src/sigutils'),
    join_paths(meson.current_source_dir(), '../src/sigutils/specific'),
    join_paths(meson.current_source_dir(), '../src/util'),
  ]

  # Configure doxygen...
  doxygen_conf_data = configuration_data()
  doxygen_conf_data.set('VERSION', meson.project_version())
  doxygen_conf_data.set('INPUT', ' '.join(doxygen_input))
  doxygen_conf_data.set('OUTPUT_DIRECTORY', meson.current_build_dir())
  sigutils_doxygen_conf = configure_file(
    input: 'sigutils.doxygen.in',
    output: 'sigutils.doxygen',
    configuration: doxygen_conf_data,
  )

  # Actual docs target
  run_target('docs',
    command : [dep_doxygen, sigutils_doxygen_conf]
  )    
endif

if dep_doxygen.found() and dep_coverxygen
  run_target('docs_coverage',
    command : [
      dep_python,
      '-m', 'coverxygen',
      '--xml-dir', join_paths(meson.current_build_dir(), 'xml'),
      '--src-dir', meson.current_source_dir(),
      '--output', join_paths(meson.current_build_dir(), 'coverxygen.md'),
      '--format', 'markdown-summary'
    ]
  )

endif