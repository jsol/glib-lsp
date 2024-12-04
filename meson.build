project('curl-test', 'c')

deps = []
deps += dependency('gio-2.0')
deps += dependency('gio-unix-2.0')
deps += dependency('glib-2.0')
deps += dependency('json-glib-1.0')
deps += dependency('tree-sitter', static: true)
deps += dependency('tree-sitter-c', static: true)

main_sources = files([
  'main.c',
  'message.c',
  'parser.c',
  'parse_utils.c',
  'processor.c',
  'process_init.c',
  'process_asserts.c',
  'rpc.c'
])


executable('glib-reader',
  sources: main_sources,
  dependencies : deps
  )
