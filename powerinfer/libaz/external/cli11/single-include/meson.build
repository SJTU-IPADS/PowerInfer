# Because Meson does not allow outputs to be placed in subfolders, we must have
# meson.build here when generating the single file header so that it is placed
# in the correct location.

pymod = import('python')
prog_python = pymod.find_installation()

single_main_file = files('CLI11.hpp.in')

if use_single_header
  single_header = custom_target(
    'CLI11.hpp',
    input: [files('../scripts/MakeSingleHeader.py'), cli11_headers, cli11_impl_headers],
    output: 'CLI11.hpp',
    command : [prog_python, '@INPUT@', '--main', single_main_file, '--output', '@OUTPUT@'],
    depend_files: [single_main_file],
  )
else
  # the `declare_dependency` needs to have the single_header source as a source
  # dependency, to ensure that the generator runs before any attempts to include
  # the header happen. Adding an empty list is an idiomatic way to ensure the
  # variable exists but does nothing
  single_header = []
endif
