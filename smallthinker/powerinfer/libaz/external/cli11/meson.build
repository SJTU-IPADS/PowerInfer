project('CLI11', ['cpp'],
  version         : run_command(find_program('scripts/ExtractVersion.py'), check: true).stdout().strip(),
  license         : 'BSD-3-clause',
  meson_version   : '>= 0.60',
  default_options : ['cpp_std=c++11', 'warning_level=3']
)

cxx = meson.get_compiler('cpp')

use_single_header = get_option('single-file-header')
use_precompiled =   get_option('precompiled')

if use_precompiled and use_single_header
  error('Options "single-file"header" and "precompiled" are mutually exclusive')
endif

cli11_headers = files(
  'include/CLI/App.hpp',
  'include/CLI/Argv.hpp',
  'include/CLI/CLI.hpp',
  'include/CLI/Config.hpp',
  'include/CLI/ConfigFwd.hpp',
  'include/CLI/Encoding.hpp',
  'include/CLI/Error.hpp',
  'include/CLI/Formatter.hpp',
  'include/CLI/FormatterFwd.hpp',
  'include/CLI/Macros.hpp',
  'include/CLI/Option.hpp',
  'include/CLI/Split.hpp',
  'include/CLI/StringTools.hpp',
  'include/CLI/TypeTools.hpp',
  'include/CLI/Validators.hpp',
  'include/CLI/Version.hpp',
)

cli11_impl_headers = files(
  'include/CLI/impl/App_inl.hpp',
  'include/CLI/impl/Argv_inl.hpp',
  'include/CLI/impl/Config_inl.hpp',
  'include/CLI/impl/Encoding_inl.hpp',
  'include/CLI/impl/Formatter_inl.hpp',
  'include/CLI/impl/Option_inl.hpp',
  'include/CLI/impl/Split_inl.hpp',
  'include/CLI/impl/StringTools_inl.hpp',
  'include/CLI/impl/Validators_inl.hpp',
)

subdir('single-include')

CLI11_inc = include_directories(['include'])

warnings = ['-Wshadow', '-Wsign-conversion', '-Wswitch-enum']
if cxx.get_id() == 'gcc' and cxx.version().version_compare('>=4.9')
  warnings += '-Weffc++'
endif
if cxx.get_id() == 'clang'
  warnings += [
    '-Wcast-align',
    '-Wimplicit-atomic-properties',
    '-Wmissing-declarations',
    '-Woverlength-strings',
    '-Wstrict-selector-match',
    '-Wundeclared-selector',
  ]
endif
add_project_arguments(cxx.get_supported_arguments(warnings), language: 'cpp')

if use_precompiled
  libcli11 = static_library(
    'CLI11',
    'src/Precompile.cpp',
    include_directories : CLI11_inc,
    cpp_args            : ['-DCLI11_COMPILE'],
  )
else
  libcli11 = []
endif

CLI11_dep = declare_dependency(
  sources             : single_header,
  link_with           : libcli11,
  include_directories : CLI11_inc,
  version             : meson.project_version(),
)

meson.override_dependency('CLI11', CLI11_dep)

if get_option('tests')
  subdir('tests')
endif
