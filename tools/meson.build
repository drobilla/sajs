# Copyright 2021-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

###########
# Utility #
###########

tool_c_args = c_suppressions
tool_c_args += ['-DSAJS_VERSION="@0@"'.format(meson.project_version())]
if cc.get_id() == 'msvc'
  tool_c_args += [
    '/D_CRT_SECURE_NO_WARNINGS',
    '/wd5045', # will insert Spectre mitigation for memory load
  ]
endif

sajs_pipe = executable(
  'sajs-pipe',
  files('sajs-pipe.c'),
  c_args: tool_c_args + program_c_args,
  dependencies: [sajs_dep],
  install: true,
  link_args: program_link_args,
)

############
# Man Page #
############

if not get_option('man').disabled()
  install_man('sajs-pipe.1')
endif
