# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

project(
  'sajs',
  ['c'],
  default_options: [
    'b_ndebug=if-release',
    'buildtype=release',
    'c_std=c99',
  ],
  license: 'ISC',
  meson_version: '>= 0.54.0',
  version: '0.0.1',
)

sajs_src_root = meson.current_source_dir()
versioned_name = 'sajs-@0@'.format(meson.project_version().split('.')[0])

#######################
# Compilers and Flags #
#######################

# Required tools
pkg = import('pkgconfig')
cc = meson.get_compiler('c')

# Set global warning suppressions
warning_level = get_option('warning_level')
c_suppressions = []
if cc.get_id() in ['clang', 'emscripten']
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-declaration-after-statement',
      '-Wno-padded',
      '-Wno-unsafe-buffer-usage',
    ]

    if host_machine.system() == 'windows'
      c_suppressions += [
        '-Wno-deprecated-declarations',
      ]
    endif
  endif

  if warning_level in ['everything', '3']
    c_suppressions += [
      '-Wno-nullability-extension',
    ]

    if host_machine.system() in ['dragonfly', 'freebsd', 'netbsd', 'openbsd']
      c_suppressions += [
        '-Wno-c11-extensions',
      ]
    endif
  endif

  if not meson.is_cross_build()
    c_suppressions += [
      '-Wno-poison-system-directories',
    ]
  endif

elif cc.get_id() == 'gcc'
  if warning_level == 'everything'
    c_suppressions += [
      '-Wno-padded',
      '-Wno-switch-default',
    ]
  endif

elif cc.get_id() == 'msvc'
  if warning_level == 'everything'
    c_suppressions += [
      '/wd4710', # function not inlined
      '/wd4711', # function selected for automatic inline expansion
      '/wd5045', # will insert Spectre mitigation
    ]
  endif

  if warning_level in ['everything', '3']
    c_suppressions += [
      '/wd4706', # assignment within conditional expression
    ]
  endif
endif

c_suppressions = cc.get_supported_arguments(c_suppressions)

##########################
# Platform Configuration #
##########################

library_c_args = []
program_c_args = []
program_link_args = []
library_link_args = []

# Use versioned name everywhere to support parallel major version installations
if host_machine.system() == 'windows'
  if get_option('default_library') == 'both'
    error('default_library=both is not supported on Windows')
  endif
  soversion = ''
else
  soversion = meson.project_version().split('.')[0]
endif

# Disable linking against the standard library if possible
if (
  host_machine.system() not in ['darwin', 'windows']
  and cc.get_id() in ['clang', 'emscripten', 'gcc']
)
  if (
    get_option('stdlib').disabled()
    or (
      get_option('stdlib').auto()
      and not get_option('b_coverage')
      and get_option('b_sanitize') == 'none'
    )
  )
    library_c_args += ['-fno-stack-protector', '-nostdlib']
    library_link_args += ['-fno-stack-protector', '-nostdlib']
  endif
endif

# Add special flags for building with emscripten to run in node
if cc.get_id() == 'emscripten'
  wasm_c_args = []
  wasm_link_args = [['-s', 'ENVIRONMENT=node'], ['-s', 'INITIAL_MEMORY=8mb']]

  library_c_args += wasm_c_args
  program_c_args += wasm_c_args
  library_link_args += wasm_link_args
  program_link_args += wasm_link_args
  program_link_args += [['-s', 'EXIT_RUNTIME']]
endif

###########
# Library #
###########

include_dirs = include_directories(['include'])
c_headers = files('include/sajs/sajs.h')
c_sources = files('src/lexer.c', 'src/status.c', 'src/writer.c')

# Set appropriate arguments for building against the library type
extra_c_args = []
if get_option('default_library') == 'static'
  extra_c_args = ['-DSAJS_STATIC']
endif

# Build shared and/or static library
libsajs = library(
  versioned_name,
  c_sources,
  c_args: extra_c_args + library_c_args + c_suppressions + ['-DSAJS_INTERNAL'],
  gnu_symbol_visibility: 'hidden',
  include_directories: include_dirs,
  install: true,
  link_args: library_link_args,
  soversion: soversion,
  version: meson.project_version(),
)

# Declare dependency for internal meson dependants
sajs_dep = declare_dependency(
  compile_args: extra_c_args,
  include_directories: include_dirs,
  link_with: libsajs,
)

# Generage pkg-config file for external dependants
pkg.generate(
  libsajs,
  description: 'Low-level streaming JSON reader and writer',
  extra_cflags: extra_c_args,
  filebase: versioned_name,
  name: 'Sajs',
  subdirs: [versioned_name],
  version: meson.project_version(),
)

# Override pkg-config dependency for internal meson dependants
meson.override_dependency(versioned_name, sajs_dep)

# Install header to a versioned include directory
install_headers(c_headers, subdir: versioned_name / 'sajs')

#########
# Tools #
#########

if not get_option('tools').disabled()
  subdir('tools')
else
  sajs_pipe = disabler()
endif

#########
# Tests #
#########

if not get_option('tests').disabled()
  subdir('test')
endif

###########
# Summary #
###########

if not meson.is_subproject()
  summary(
    'Tests',
    not get_option('tests').disabled(),
    bool_yn: true,
    section: 'Components',
  )
  summary(
    {
      'Install prefix': get_option('prefix'),
      'Headers': get_option('prefix') / get_option('includedir'),
      'Libraries': get_option('prefix') / get_option('libdir'),
    },
    section: 'Directories',
  )
endif
