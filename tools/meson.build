# Compiler flags
extra_cflags = [
  '-W',
  '-Wformat=2',
  '-Winline',
  '-Wstrict-prototypes',
  '-Wmissing-prototypes',
  '-Wdisabled-optimization',
  '-Wfloat-equal',
  '-Wall',
  '-Werror',
  '-Ddebug=false',
  '-Dstrip=true',
]

executable(
  'print',
  sources: ['print.c'],
  dependencies: deps,
  c_args: extra_cflags,
)

executable(
  'run',
  sources: ['run.c'],
  include_directories: '../src',
  dependencies: deps,
  link_with: testable_lib,
  c_args: extra_cflags,
)
