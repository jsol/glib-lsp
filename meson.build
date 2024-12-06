project('glib-lsp', 'c')
deps = []
deps += dependency('gio-2.0')
deps += dependency('gio-unix-2.0')
deps += dependency('glib-2.0')
deps += dependency('json-glib-1.0')
deps += dependency('tree-sitter', static: true)
deps += dependency('tree-sitter-c', static: true)

subdir('src')
subdir('tools')
subdir('test')
