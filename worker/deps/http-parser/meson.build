libhttpparser_sources = [
  'http_parser.c',
]

libuv_cargs = '-DHTTP_PARSER_STRICT=0'


libhttpparser = library(
  'libhttpparser',
  libhttpparser_sources,
  include_directories: include_directories('.'),
  c_args: libuv_cargs,
  install: true,
)

libhttpparser_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: libhttpparser,
)

install_headers('http_parser.h')
