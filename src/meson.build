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

sources = (
  [
    'main.c',
    'message.c',
    'parse_utils.c',
    'parser.c',
    'process_asserts.c',
    'process_init.c',
    'processor.c',
    'rpc.c',
  ]
)

executable('glib-lsp', sources: sources, dependencies: deps, c_args: extra_cflags)

testable_lib = shared_library('glib-lsp-lib', sources, dependencies: deps, install: false)
