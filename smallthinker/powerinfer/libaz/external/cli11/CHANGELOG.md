# Changelog

## Unreleased

## Version 2.5: Help Formatter

This version add a new formatter with improved control capabilities and output
aligned with standards for help output. It also add a modifier to enable use of
non-standard option names. Along with several bug fixes for edge cases in string
and config file parsing.

- Better help formatter [#866][], this better aligns the help generation with
  UNIX standard and allows use in help2man. [#1093][]
- Add mechanism to allow option groups to be hidden and all options be
  considered part of the parent for help display [#1039][]
- Add a modifier to allow non-standard single flag option names such as
  `-option`. [#1078][]
- Add modifier for subcommands to disable fallthrough which can resolve some
  issues with positional arguments [#1073][]
- Add some polish to config file output removing some unnecessary output and add
  modifier to control output of default values [#1075][]
- Add the ability to specify pair/tuple defaults and improved parsing [#1081][]
- Bugfix: Take the configurability of an option name into account when
  determining naming conflicts [#1049][]
- Bugfix: Fix an issue where an extra subcommand header was being printed in the
  output [#1058][]
- Bugfix: Add additional fuzzing tests and fixes for a bug in escape string
  processing, and resolve inconsistencies in the handing of `{}` between command
  line parsing and config file parsing. [#1060][]
- Bugfix: Improve handling of some ambiguities in vector input processing for
  config files, specifically in the case of vector of vector inputs. [#1069][]
- Bugfix: Fix an issue in the handling of uint8_t enums, and some issues related
  to single element tuples [#1087][]
- Bugfix: Fix an issue with binary strings containing a `\x` [#1097][]
- Bugfix: Move the help generation priority so it triggers before config file
  processing [#1106][]
- Bugfix: Fixed an issue where max/min on positionals was not being respected
  and optional positionals were being ignored [#1108][]
- Bugfix: Fix an issue with strings which started and ended with brackets being
  misinterpreted as vectors. The parsing now has special handling of strings
  which start with `[[` [#1110][]
- Bugfix: Fix some macros for support in C++26 related to wide string parsing
  [#1113][]
- Bugfix: Allow trailing spaces on numeric string conversions [#1115][]
- Docs: Update pymod.find_installation to find python in meson.build [#1076][]
- Docs: Add example for transform validators [#689][]
- Docs: Fix several spelling mistakes [#1101][]
- Backend: Update copyright dates to 2025 [#1112][]
- Backend: Update CMAKE minimum version to 3.10 [#1084][]

[#1039]: https://github.com/CLIUtils/CLI11/pull/1039
[#1049]: https://github.com/CLIUtils/CLI11/pull/1049
[#1058]: https://github.com/CLIUtils/CLI11/pull/1058
[#1060]: https://github.com/CLIUtils/CLI11/pull/1060
[#1069]: https://github.com/CLIUtils/CLI11/pull/1069
[#866]: https://github.com/CLIUtils/CLI11/pull/866
[#1073]: https://github.com/CLIUtils/CLI11/pull/1073
[#1075]: https://github.com/CLIUtils/CLI11/pull/1075
[#689]: https://github.com/CLIUtils/CLI11/pull/689
[#1076]: https://github.com/CLIUtils/CLI11/pull/1076
[#1078]: https://github.com/CLIUtils/CLI11/pull/1078
[#1081]: https://github.com/CLIUtils/CLI11/pull/1081
[#1084]: https://github.com/CLIUtils/CLI11/pull/1084
[#1087]: https://github.com/CLIUtils/CLI11/pull/1087
[#1093]: https://github.com/CLIUtils/CLI11/pull/1093
[#1097]: https://github.com/CLIUtils/CLI11/pull/1097
[#1101]: https://github.com/CLIUtils/CLI11/pull/1101
[#1106]: https://github.com/CLIUtils/CLI11/pull/1106
[#1108]: https://github.com/CLIUtils/CLI11/pull/1108
[#1110]: https://github.com/CLIUtils/CLI11/pull/1110
[#1112]: https://github.com/CLIUtils/CLI11/pull/1112
[#1113]: https://github.com/CLIUtils/CLI11/pull/1113
[#1115]: https://github.com/CLIUtils/CLI11/pull/1115

## Version 2.4: Unicode and TOML support

This version adds Unicode support, support for TOML standard including multiline
strings, digit separators, string escape sequences,and dot notation. An initial
round of a fuzzer was added to testing which has caught several bugs related to
config file processing, and a few other edge cases not previously observed.

- Add Unicode support and bug fixes [#804][], [#923][], [#876][], [#848][],
  [#832][], [#987][]
- Match TOML standard for string and numerical entries, multiline strings
  [#968][], [#967][],[#964][], [#935][]
- Add validation for environmental variables [#926][]
- Add an escape string transform [#970][]
- Add A REVERSE multi-option policy to support multiple config files and other
  applications [#918][]
- Add usage message replacement [#768][]
- Allow using dot notation for subcommand arguments such as `--sub1.field`
  [#789][]
- Bugfix: Fuzzing tests and fixes [#930][], [#905][], [#874][], [#846][]
- Bugfix: Missing coverage tests [#928][]
- Bugfix: CMake package and package config tests and fixes [#916][]
- Bugfix: Support for Windows ARM compilation and tests [#913][], [#914][]
- Bugfix: Environmental variable checks in non-triggered subcommands [#904][]
- Bugfix: Environmental variables were not being correctly process by config
  pointer [#891][]
- Bugfix: Undefined behavior in `sum_string_vector` [#893][]
- Bugfix: Warnings and updates for CUDA 11 support [#851][]
- Backend: Add tests for newer compilers (lost with Travis CI) [#972][]
- Backend: Increase minimum CMake to 3.5 [#898][]
- Backend: Remove integrated Conan support (provided now by Conan center)
  [#853][]
- Tests: Support Catch2 Version 3 [#896][], [#980][]

[#768]: https://github.com/CLIUtils/CLI11/pull/768
[#789]: https://github.com/CLIUtils/CLI11/pull/789
[#804]: https://github.com/CLIUtils/CLI11/pull/804
[#832]: https://github.com/CLIUtils/CLI11/pull/832
[#846]: https://github.com/CLIUtils/CLI11/pull/846
[#848]: https://github.com/CLIUtils/CLI11/pull/848
[#851]: https://github.com/CLIUtils/CLI11/pull/851
[#853]: https://github.com/CLIUtils/CLI11/pull/853
[#874]: https://github.com/CLIUtils/CLI11/pull/874
[#876]: https://github.com/CLIUtils/CLI11/pull/876
[#891]: https://github.com/CLIUtils/CLI11/pull/891
[#893]: https://github.com/CLIUtils/CLI11/pull/893
[#896]: https://github.com/CLIUtils/CLI11/pull/896
[#898]: https://github.com/CLIUtils/CLI11/pull/898
[#904]: https://github.com/CLIUtils/CLI11/pull/904
[#905]: https://github.com/CLIUtils/CLI11/pull/905
[#913]: https://github.com/CLIUtils/CLI11/pull/913
[#914]: https://github.com/CLIUtils/CLI11/pull/914
[#916]: https://github.com/CLIUtils/CLI11/pull/916
[#918]: https://github.com/CLIUtils/CLI11/pull/918
[#923]: https://github.com/CLIUtils/CLI11/pull/923
[#926]: https://github.com/CLIUtils/CLI11/pull/926
[#928]: https://github.com/CLIUtils/CLI11/pull/928
[#930]: https://github.com/CLIUtils/CLI11/pull/930
[#935]: https://github.com/CLIUtils/CLI11/pull/935
[#964]: https://github.com/CLIUtils/CLI11/pull/964
[#967]: https://github.com/CLIUtils/CLI11/pull/967
[#968]: https://github.com/CLIUtils/CLI11/pull/968
[#970]: https://github.com/CLIUtils/CLI11/pull/970
[#972]: https://github.com/CLIUtils/CLI11/pull/972
[#980]: https://github.com/CLIUtils/CLI11/pull/980
[#987]: https://github.com/CLIUtils/CLI11/pull/987

### Version 2.4.1: Missing header

A transitive include that might be present in some standard libraries is now
included directly. This also fixes a test on architectures that need libatomic
linked and fix an inadvertent breaking change regarding unused defaults for
config files

- Bugfix: Include cstdint [#996][]
- Bugfix: Fix change in operation of config_ptr with unused default in the count
  method [#1003][]
- Tests: Include libatomic if required for fuzzing test [#1000][]

[#996]: https://github.com/CLIUtils/CLI11/pull/996
[#1000]: https://github.com/CLIUtils/CLI11/pull/1000
[#1003]: https://github.com/CLIUtils/CLI11/pull/1003

### Version 2.4.2: Build systems

This version improves support for alternative build systems, like Meson and
Bazel. The single-include file now is in its own subdirectory. Several smaller
fixes as well.

- Meson: fixes, cleanups, and modernizations [#1024][] & [#1025][]
- Support building with Bazel [#1033][]
- Restore non-arch dependent path for the pkgconfig file [#1012][]
- Add `get_subcommand_no_throw` [#1016][]
- Move single file to `single-include` folder [#1030][] & [#1036][]
- Fixed `app.set_failure_message(...)` -> `app.failure_message(...)` [#1018][]
- Add IWYU pragmas [#1008][]
- Fix internal header include paths [#1011][]
- Improved clarity in `RequiredError` [#1029][]
- Added ability to use lexical_cast overloads constrained with enable_if
  [#1021][]
- Bug fixes in latest release related to environmental variable parsing from
  option groups and unrecognized fields in a config file [#1005][]

[#1005]: https://github.com/CLIUtils/CLI11/pull/1005
[#1008]: https://github.com/CLIUtils/CLI11/pull/1008
[#1011]: https://github.com/CLIUtils/CLI11/pull/1011
[#1012]: https://github.com/CLIUtils/CLI11/pull/1012
[#1016]: https://github.com/CLIUtils/CLI11/pull/1016
[#1018]: https://github.com/CLIUtils/CLI11/pull/1018
[#1021]: https://github.com/CLIUtils/CLI11/pull/1021
[#1024]: https://github.com/CLIUtils/CLI11/pull/1024
[#1025]: https://github.com/CLIUtils/CLI11/pull/1025
[#1029]: https://github.com/CLIUtils/CLI11/pull/1029
[#1030]: https://github.com/CLIUtils/CLI11/pull/1030
[#1033]: https://github.com/CLIUtils/CLI11/pull/1033
[#1036]: https://github.com/CLIUtils/CLI11/pull/1036

## Version 2.3: Precompilation Support

This version adds a pre-compiled mode to CLI11, which allows you to precompile
the library, saving time on incremental rebuilds, making CLI11 more competitive
on compile time with classic compiled CLI libraries. The header-only mode is
still default, and is not yet distributed via binaries.

- Add `CLI11_PRECOMPILED` as an option [#762][]
- Bugfix: Include `<functional>` in `FormatterFwd` [#727][]
- Bugfix: Add missing `Macros.hpp` to `Error.hpp` [#755][]
- Bugfix: Fix subcommand callback trigger [#733][]
- Bugfix: Variable rename to avoid warning [#734][]
- Bugfix: `split_program_name` single file name error [#740][]
- Bugfix: Better support for min/max overrides on MSVC [#741][]
- Bugfix: Support MSVC 2022 [#748][]
- Bugfix: Support negated flag in config file [#775][]
- Bugfix: Better errors for some confusing config file situations [#781][]
- Backend: Restore coverage testing (lost with Travis CI) [#747][]

[#727]: https://github.com/CLIUtils/CLI11/pull/727
[#733]: https://github.com/CLIUtils/CLI11/pull/733
[#734]: https://github.com/CLIUtils/CLI11/pull/734
[#740]: https://github.com/CLIUtils/CLI11/pull/740
[#741]: https://github.com/CLIUtils/CLI11/pull/741
[#747]: https://github.com/CLIUtils/CLI11/pull/747
[#748]: https://github.com/CLIUtils/CLI11/pull/748
[#755]: https://github.com/CLIUtils/CLI11/pull/755
[#762]: https://github.com/CLIUtils/CLI11/pull/762
[#775]: https://github.com/CLIUtils/CLI11/pull/775
[#781]: https://github.com/CLIUtils/CLI11/pull/781

### Version 2.3.1: Missing implementation

A function implementation was missing after the pre-compile move, missed due to
the fact we lost 100% after losing coverage checking. We are working on filling
out 100% coverage again to ensure this doesn't happen again!

- Bugfix: `App::get_option_group` implementation missing [#793][]
- Bugfix: Fix spacing when setting an empty footer [#796][]
- Bugfix: Address Klocwork static analysis checking issues [#785][]

[#785]: https://github.com/CLIUtils/CLI11/pull/785
[#793]: https://github.com/CLIUtils/CLI11/pull/793
[#796]: https://github.com/CLIUtils/CLI11/pull/796

### Version 2.3.2: Minor maintenance

This version provides a few fixes collected over the last three months before
adding features for 2.4.

- Bugfix: Consistently use ADL for `lexical_cast`, making it easier to extend
  for custom template types [#820][]
- Bugfix: Tweak the parsing of files for flags with `disable_flag_override`
  [#800][]
- Bugfix: Handle out of bounds long long [#807][]
- Bugfix: Spacing of `make_description` min option output [#808][]
- Bugfix: Print last parsed subcommand's help message [#822][]
- Bugfix: Avoid floating point warning in GCC 12 [#803][]
- Bugfix: Fix a few gcc warnings [#813][]
- Backend: Max CMake tested 3.22 -> 3.24 [#823][]

[#800]: https://github.com/CLIUtils/CLI11/pull/800
[#803]: https://github.com/CLIUtils/CLI11/pull/803
[#807]: https://github.com/CLIUtils/CLI11/pull/807
[#808]: https://github.com/CLIUtils/CLI11/pull/808
[#813]: https://github.com/CLIUtils/CLI11/pull/813
[#820]: https://github.com/CLIUtils/CLI11/pull/820
[#822]: https://github.com/CLIUtils/CLI11/pull/822
[#823]: https://github.com/CLIUtils/CLI11/pull/823

## Version 2.2: Option and Configuration Flexibility

New features include support for output of an empty vector, a summing option
policy that can be applied more broadly, and an option to validate optional
arguments to discriminate from positional arguments. A new validator to check
for files on a default path is included to allow one or more default paths for
configuration files or other file arguments. A number of bug fixes and code
cleanup for various build configurations. Clean up of some error outputs and
extension of existing capability to new types or situations.

There is a possible minor breaking change in behavior of certain types which
wrapped an integer, such as `std::atomic<int>` or `std::optional<int>` when used
in a flag. The default behavior is now as a single argument value vs. summing
all the arguments. The default summing behavior is now restricted to pure
integral types, int64_t, int, uint32_t, etc. Use the new `sum` multi option
policy to revert to the older behavior. The summing behavior on wrapper types
was not originally intended.

- Add `MultiOptionPolicy::Sum` and refactor the `add_flag` to fix a bug when
  using `std::optional<bool>` as type. [#709][]
- Add support for an empty vector result in TOML and as a default string.
  [#660][]
- Add `.validate_optional_arguments()` to support discriminating positional
  arguments from vector option arguments. [#668][]
- Add `CLI::FileOnDefaultPath` to check for files on a specified default path.
  [#698][]
- Change default value display in help messages from `=XXXX` to `[XXXXX]` to
  make it clearer. [#666][]
- Modify the Range Validator to support additional types and clean up the error
  output. [#690][]
- Bugfix: The trigger on parse modifier did not work on positional argument.s
  [#713][]
- Bugfix: The single header file generation was missing custom namespace
  generation. [#707][]
- Bugfix: Clean up File Error handling in the argument processing. [#678][]
- Bugfix: Fix a stack overflow error if nameless commands had fallthrough.
  [#665][]
- Bugfix: A subcommand callback could be executed multiple times if it was a
  member of an option group. [#666][]
- Bugfix: Fix an issue with vectors of multi argument types where partial
  argument sets did not result in an error. [#661][]
- Bugfix: Fix an issue with type the template matching on C++20 and add some CI
  builds for C++20. [#663][]
- Bugfix: Fix typo in C++20 detection on MSVC. [#706][]
- Bugfix: An issue where the detection of RTTI being disabled on certain MSVC
  platforms did not disable the use of dynamic cast calls. [#666][]
- Bugfix: Resolve strict-overflow warning on some GCC compilers. [#666][]
- Backend: Add additional tests concerning the use of aliases for option groups
  in config files. [#666][]
- Build: Add support for testing in meson and cleanup symbolic link generation.
  [#701][], [#697][]
- Build: Support building in WebAssembly. [#679][]

[#660]: https://github.com/CLIUtils/CLI11/pull/660
[#661]: https://github.com/CLIUtils/CLI11/pull/661
[#663]: https://github.com/CLIUtils/CLI11/pull/663
[#665]: https://github.com/CLIUtils/CLI11/pull/665
[#666]: https://github.com/CLIUtils/CLI11/pull/666
[#668]: https://github.com/CLIUtils/CLI11/pull/668
[#678]: https://github.com/CLIUtils/CLI11/pull/678
[#679]: https://github.com/CLIUtils/CLI11/pull/679
[#690]: https://github.com/CLIUtils/CLI11/pull/690
[#697]: https://github.com/CLIUtils/CLI11/pull/697
[#698]: https://github.com/CLIUtils/CLI11/pull/698
[#701]: https://github.com/CLIUtils/CLI11/pull/701
[#706]: https://github.com/CLIUtils/CLI11/pull/706
[#707]: https://github.com/CLIUtils/CLI11/pull/707
[#709]: https://github.com/CLIUtils/CLI11/pull/709
[#713]: https://github.com/CLIUtils/CLI11/pull/713

## Version 2.1: Names and callbacks

The name restrictions for options and subcommands are now much looser, allowing
a wider variety of characters than before, even spaces can be used (use quotes
to include a space in most shells). The default configuration parser was
improved, allowing your configuration to sit in a larger file. And option
callbacks have a few new settings, allowing them to be run even if the option is
not passed, or every time the option is parsed.

- Option/subcommand name restrictions have been relaxed. Most characters are now
  allowed. [#627][]
- The config parser can accept streams, specify a specific section, and inline
  comment characters are supported [#630][]
- `force_callback` & `trigger_on_parse` added, allowing a callback to always run
  on parse even if not present or every time the option is parsed [#631][]
- Bugfix(cmake): Only add `CONFIGURE_DEPENDS` if CLI11 is the main project
  [#633][]
- Bugfix(cmake): Ensure the cmake/pkg-config files install to a arch independent
  path [#635][]
- Bugfix: The single header file generation was missing the include guard.
  [#620][]

[#620]: https://github.com/CLIUtils/CLI11/pull/620
[#627]: https://github.com/CLIUtils/CLI11/pull/627
[#630]: https://github.com/CLIUtils/CLI11/pull/630
[#631]: https://github.com/CLIUtils/CLI11/pull/631
[#633]: https://github.com/CLIUtils/CLI11/pull/633
[#635]: https://github.com/CLIUtils/CLI11/pull/635

### Version 2.1.1: Quick Windows fix

- A collision with `min`/`max` macros on Windows has been fixed. [#642][]
- Tests pass with Boost again [#646][]
- Running the pre-commit hooks in development no longer requires docker for
  clang-format [#647][]

[#642]: https://github.com/CLIUtils/CLI11/pull/642
[#646]: https://github.com/CLIUtils/CLI11/pull/646
[#647]: https://github.com/CLIUtils/CLI11/pull/647

## Version 2.1.2: Better subproject builds

- Use `main` for the main branch of the repository [#657][]
- Bugfix(cmake): Enforce at least C++11 when using CMake target [#656][]
- Build: Don't run doxygen and CTest includes if a submodule [#656][]
- Build: Avoid a warning on CMake 3.22 [#656][]
- Build: Support compiling the tests with an external copy of Catch2 [#653][]

[#653]: https://github.com/CLIUtils/CLI11/pull/653
[#656]: https://github.com/CLIUtils/CLI11/pull/656
[#657]: https://github.com/CLIUtils/CLI11/pull/657

## Version 2.0: Simplification

This version focuses on cleaning up deprecated functionality, and some minor
default changes. The config processing is TOML compliant now. Atomics and
complex numbers are directly supported, along with other container improvements.
A new version flag option has finally been added. Subcommands are significantly
improved with new features and bugfixes for corner cases. This release contains
a lot of backend cleanup, including a complete overhaul of the testing system
and single file generation system.

- Built-in config format is TOML compliant now [#435][]
  - Support multiline TOML [#528][]
  - Support for configurable quotes [#599][]
  - Support short/positional options in config mode [#443][]
- More powerful containers, support for `%%` separator [#423][]
- Support atomic types [#520][] and complex types natively [#423][]
- Add a type validator `CLI::TypeValidator<TYPE>` [#526][]
- Add a version flag easily [#452][], with help message [#601][]
- Support `->silent()` on subcommands. [#529][]
- Add alias section to help for subcommands [#545][]
- Allow quotes to specify a program name [#605][]
- Backend: redesigned MakeSingleFiles to have a higher level of manual control,
  to support future features. [#546][]
- Backend: moved testing from GTest to Catch2 [#574][]
- Bugfix: avoid duplicated and missed calls to the final callback [#584][]
- Bugfix: support embedded newlines in more places [#592][]
- Bugfix: avoid listing helpall as a required flag [#530][]
- Bugfix: avoid a clash with WINDOWS define [#563][]
- Bugfix: the help flag didn't get processed when a config file was required
  [#606][]
- Bugfix: fix description of non-configurable subcommands in config [#604][]
- Build: support pkg-config [#523][]

> ### Converting from CLI11 1.9
>
> - Removed deprecated set commands, use validators instead. [#565][]
> - The final "defaulted" bool has been removed, use `->capture_default_str()`
>   instead. Use `app.option_defaults()->always_capture_default()` to set this
>   for all future options. [#597][]
> - Use `add_option` on a complex number instead of `add_complex`, which has
>   been removed.

[#423]: https://github.com/CLIUtils/CLI11/pull/423
[#435]: https://github.com/CLIUtils/CLI11/pull/435
[#443]: https://github.com/CLIUtils/CLI11/pull/443
[#452]: https://github.com/CLIUtils/CLI11/pull/452
[#520]: https://github.com/CLIUtils/CLI11/pull/520
[#523]: https://github.com/CLIUtils/CLI11/pull/523
[#526]: https://github.com/CLIUtils/CLI11/pull/526
[#528]: https://github.com/CLIUtils/CLI11/pull/528
[#529]: https://github.com/CLIUtils/CLI11/pull/529
[#530]: https://github.com/CLIUtils/CLI11/pull/530
[#545]: https://github.com/CLIUtils/CLI11/pull/545
[#546]: https://github.com/CLIUtils/CLI11/pull/546
[#563]: https://github.com/CLIUtils/CLI11/pull/563
[#565]: https://github.com/CLIUtils/CLI11/pull/565
[#574]: https://github.com/CLIUtils/CLI11/pull/574
[#584]: https://github.com/CLIUtils/CLI11/pull/584
[#592]: https://github.com/CLIUtils/CLI11/pull/592
[#597]: https://github.com/CLIUtils/CLI11/pull/597
[#599]: https://github.com/CLIUtils/CLI11/pull/599
[#601]: https://github.com/CLIUtils/CLI11/pull/601
[#604]: https://github.com/CLIUtils/CLI11/pull/604
[#605]: https://github.com/CLIUtils/CLI11/pull/605
[#606]: https://github.com/CLIUtils/CLI11/pull/606

## Version 1.9: Config files and cleanup

Config file handling was revamped to fix common issues, and now supports reading
[TOML](https://github.com/toml-lang/toml).

Adding options is significantly more powerful with support for things like
`std::tuple` and `std::array`, including with transforms. Several new
configuration options were added to facilitate a wider variety of apps. GCC 4.7
is no longer supported.

- Config files refactored, supports TOML (may become default output in 2.0)
  [#362][]
- Added two template parameter form of `add_option`, allowing `std::optional` to
  be supported without a special import [#285][]
- `string_view` now supported in reasonable places [#300][], [#285][]
- `immediate_callback`, `final_callback`, and `parse_complete_callback` added to
  support controlling the App callback order [#292][], [#313][]
- Multiple positional arguments maintain order if `positionals_at_end` is set.
  [#306][]
- Pair/tuple/array now supported, and validators indexed to specific components
  in the objects [#307][], [#310][]
- Footer callbacks supported [#309][]
- Subcommands now support needs (including nameless subcommands) [#317][]
- More flexible type size, more useful `add_complex` [#325][], [#370][]
- Added new validators `CLI::NonNegativeNumber` and `CLI::PositiveNumber`
  [#342][]
- Transform now supports arrays [#349][]
- Option groups can be hidden [#356][]
- Add `CLI::deprecate_option` and `CLI::retire_option` functions [#358][]
- More flexible and safer Option `default_val` [#387][]
- Backend: Cleaner type traits [#286][]
- Backend: File checking updates [#341][]
- Backend: Using pre-commit to format, checked in GitHub Actions [#336][]
- Backend: Clang-tidy checked again, CMake option now `CL11_CLANG_TIDY` [#390][]
- Backend: Warning cleanup, more checks from klocwork [#350][], Effective C++
  [#354][], clang-tidy [#360][], CUDA NVCC [#365][], cross compile [#373][],
  sign conversion [#382][], and cpplint [#400][]
- Docs: CLI11 Tutorial now hosted in the same repository [#304][], [#318][],
  [#374][]
- Bugfix: Fixed undefined behavior in `checked_multiply` [#290][]
- Bugfix: `->check()` was adding the name to the wrong validator [#320][]
- Bugfix: Resetting config option works properly [#301][]
- Bugfix: Hidden flags were showing up in error printout [#333][]
- Bugfix: Enum conversion no longer broken if stream operator added [#348][]
- Build: The meson build system supported [#299][]
- Build: GCC 4.7 is no longer supported, due mostly to GoogleTest. GCC 4.8+ is
  now required. [#160][]
- Build: Restructured significant portions of CMake build system [#394][]

> ### Converting from CLI11 1.8
>
> - Some deprecated methods dropped
>   - `add_set*` should be replaced with `->check`/`->transform` and
>     `CLI::IsMember` since 1.8
>   - `get_defaultval` was replaced by `get_default_str` in 1.8
> - The true/false 4th argument to `add_option` is expected to be removed in
>   2.0, use `->capture_default_str()` since 1.8

[#160]: https://github.com/CLIUtils/CLI11/pull/160
[#285]: https://github.com/CLIUtils/CLI11/pull/285
[#286]: https://github.com/CLIUtils/CLI11/pull/286
[#290]: https://github.com/CLIUtils/CLI11/pull/290
[#292]: https://github.com/CLIUtils/CLI11/pull/292
[#299]: https://github.com/CLIUtils/CLI11/pull/299
[#300]: https://github.com/CLIUtils/CLI11/pull/300
[#301]: https://github.com/CLIUtils/CLI11/pull/301
[#304]: https://github.com/CLIUtils/CLI11/pull/304
[#306]: https://github.com/CLIUtils/CLI11/pull/306
[#307]: https://github.com/CLIUtils/CLI11/pull/307
[#309]: https://github.com/CLIUtils/CLI11/pull/309
[#310]: https://github.com/CLIUtils/CLI11/pull/310
[#313]: https://github.com/CLIUtils/CLI11/pull/313
[#317]: https://github.com/CLIUtils/CLI11/pull/317
[#318]: https://github.com/CLIUtils/CLI11/pull/318
[#320]: https://github.com/CLIUtils/CLI11/pull/320
[#325]: https://github.com/CLIUtils/CLI11/pull/325
[#333]: https://github.com/CLIUtils/CLI11/pull/333
[#336]: https://github.com/CLIUtils/CLI11/pull/336
[#341]: https://github.com/CLIUtils/CLI11/pull/341
[#342]: https://github.com/CLIUtils/CLI11/pull/342
[#348]: https://github.com/CLIUtils/CLI11/pull/348
[#349]: https://github.com/CLIUtils/CLI11/pull/349
[#350]: https://github.com/CLIUtils/CLI11/pull/350
[#354]: https://github.com/CLIUtils/CLI11/pull/354
[#356]: https://github.com/CLIUtils/CLI11/pull/356
[#358]: https://github.com/CLIUtils/CLI11/pull/358
[#360]: https://github.com/CLIUtils/CLI11/pull/360
[#362]: https://github.com/CLIUtils/CLI11/pull/362
[#365]: https://github.com/CLIUtils/CLI11/pull/365
[#370]: https://github.com/CLIUtils/CLI11/pull/370
[#373]: https://github.com/CLIUtils/CLI11/pull/373
[#374]: https://github.com/CLIUtils/CLI11/pull/374
[#382]: https://github.com/CLIUtils/CLI11/pull/382
[#387]: https://github.com/CLIUtils/CLI11/pull/387
[#390]: https://github.com/CLIUtils/CLI11/pull/390
[#394]: https://github.com/CLIUtils/CLI11/pull/394
[#400]: https://github.com/CLIUtils/CLI11/pull/400

### Version 1.9.1: Backporting fixes

This is a patch version that backports fixes from the development of 2.0.

- Support relative inclusion [#475][]
- Fix cases where spaces in paths could break CMake support [#471][]
- Fix an issue with string conversion [#421][]
- Cross-compiling improvement for Conan.io [#430][]
- Fix option group default propagation [#450][]
- Fix for C++20 [#459][]
- Support compiling with RTTI off [#461][]

[#421]: https://github.com/CLIUtils/CLI11/pull/421
[#430]: https://github.com/CLIUtils/CLI11/pull/430
[#450]: https://github.com/CLIUtils/CLI11/pull/450
[#459]: https://github.com/CLIUtils/CLI11/pull/459
[#461]: https://github.com/CLIUtils/CLI11/pull/461
[#471]: https://github.com/CLIUtils/CLI11/pull/471
[#475]: https://github.com/CLIUtils/CLI11/pull/475

## Version 1.8: Transformers, default strings, and flags

Set handling has been completely replaced by a new backend that works as a
Validator or Transformer. This provides a single interface instead of the 16
different functions in App. It also allows ordered collections to be used,
custom functions for filtering, and better help and error messages. You can also
use a collection of pairs (like `std::map`) to transform the match into an
output. Also new are inverted flags, which can cancel or reduce the count of
flags, and can also support general flag types. A new `add_option_fn` lets you
more easily program CLI11 options with the types you choose. Vector options now
support a custom separator. Apps can now be composed with unnamed subcommand
support. The final bool "defaults" flag when creating options has been replaced
by `->capture_default_str()` (ending an old limitation in construction made this
possible); the old method is still available but may be removed in future
versions.

- Replaced default help capture: `.add_option("name", value, "", True)` becomes
  `.add_option("name", value)->capture_default_str()` [#242][]
- Added `.always_capture_default()` [#242][]
- New `CLI::IsMember` validator replaces set validation [#222][]
- `IsMember` also supports container of pairs, transform allows modification of
  result [#228][]
- Added new Transformers, `CLI::AsNumberWithUnit` and `CLI::AsSizeValue`
  [#253][]
- Much more powerful flags with different values [#211][], general types
  [#235][]
- `add_option` now supports bool due to unified bool handling [#211][]
- Support for composable unnamed subcommands [#216][]
- Reparsing is better supported with `.remaining_for_passthrough()` [#265][]
- Custom vector separator using `->delimiter(char)` [#209][], [#221][], [#240][]
- Validators added for IP4 addresses and positive numbers [#210][] and numbers
  [#262][]
- Minimum required Boost for optional Optionals has been corrected to 1.61
  [#226][]
- Positionals can stop options from being parsed with `app.positionals_at_end()`
  [#223][]
- Added `validate_positionals` [#262][]
- Positional parsing is much more powerful [#251][], duplicates supported
  [#247][]
- Validators can be negated with `!` [#230][], and now handle tname functions
  [#228][]
- Better enum support and streaming helper [#233][] and [#228][]
- Cleanup for shadow warnings [#232][]
- Better alignment on multiline descriptions [#269][]
- Better support for aarch64 [#266][]
- Respect `BUILD_TESTING` only if CLI11 is the main project; otherwise,
  `CLI11_TESTING` must be used [#277][]
- Drop auto-detection of experimental optional and boost::optional; must be
  enabled explicitly (too fragile) [#277][] [#279][]

> ### Converting from CLI11 1.7
>
> - `.add_option(..., true)` should be replaced by
>   `.add_option(...)->capture_default_str()` or
>   `app.option_defaults()->always_capture_default()` can be used
> - `app.add_set("--name", value, {"choice1", "choice2"})` should become
>   `app.add_option("--name", value)->check(CLI::IsMember({"choice1", "choice2"}))`
> - The `_ignore_case` version of this can be replaced by adding
>   `CLI::ignore_case` to the argument list in `IsMember`
> - The `_ignore_underscore` version of this can be replaced by adding
>   `CLI::ignore_underscore` to the argument list in `IsMember`
> - The `_ignore_case_underscore` version of this can be replaced by adding both
>   functions listed above to the argument list in `IsMember`
> - If you want an exact match to the original choice after one of the modifier
>   functions matches, use `->transform` instead of `->check`
> - The `_mutable` versions of this can be replaced by passing a pointer or
>   shared pointer into `IsMember`
> - An error with sets now produces a `ValidationError` instead of a
>   `ConversionError`

[#209]: https://github.com/CLIUtils/CLI11/pull/209
[#210]: https://github.com/CLIUtils/CLI11/pull/210
[#211]: https://github.com/CLIUtils/CLI11/pull/211
[#216]: https://github.com/CLIUtils/CLI11/pull/216
[#221]: https://github.com/CLIUtils/CLI11/pull/221
[#222]: https://github.com/CLIUtils/CLI11/pull/222
[#223]: https://github.com/CLIUtils/CLI11/pull/223
[#226]: https://github.com/CLIUtils/CLI11/pull/226
[#228]: https://github.com/CLIUtils/CLI11/pull/228
[#230]: https://github.com/CLIUtils/CLI11/pull/230
[#232]: https://github.com/CLIUtils/CLI11/pull/232
[#233]: https://github.com/CLIUtils/CLI11/pull/233
[#235]: https://github.com/CLIUtils/CLI11/pull/235
[#240]: https://github.com/CLIUtils/CLI11/pull/240
[#242]: https://github.com/CLIUtils/CLI11/pull/242
[#247]: https://github.com/CLIUtils/CLI11/pull/247
[#251]: https://github.com/CLIUtils/CLI11/pull/251
[#253]: https://github.com/CLIUtils/CLI11/pull/253
[#262]: https://github.com/CLIUtils/CLI11/pull/262
[#265]: https://github.com/CLIUtils/CLI11/pull/265
[#266]: https://github.com/CLIUtils/CLI11/pull/266
[#269]: https://github.com/CLIUtils/CLI11/pull/269
[#277]: https://github.com/CLIUtils/CLI11/pull/277
[#279]: https://github.com/CLIUtils/CLI11/pull/279

## Version 1.7: Parse breakup

The parsing procedure now maps much more sensibly to complex, nested subcommand
structures. Each phase of the parsing happens on all subcommands before moving
on with the next phase of the parse. This allows several features, like required
environment variables, to work properly even through subcommand boundaries.
Passing the same subcommand multiple times is better supported. Several new
features were added as well, including Windows style option support, parsing
strings directly, and ignoring underscores in names. Adding a set that you plan
to change later must now be done with `add_mutable_set`.

- Support Windows style options with `->allow_windows_style_options`. [#187][]
  On by default on Windows. [#190][]
- Added `parse(string)` to split up and parse a command-line style string
  directly. [#186][]
- Added `ignore_underscore` and related functions, to ignore underscores when
  matching names. [#185][]
- The default INI Config will now add quotes to strings with spaces [#195][]
- The default message now will mention the help-all flag also if present
  [#197][]
- Added `->description` to set Option descriptions [#199][]
- Mutating sets (introduced in Version 1.6) now have a clear add method,
  `add_mutable_set*`, since the set reference should not expire [#200][]
- Subcommands now track how many times they were parsed in a parsing process.
  `count()` with no arguments will return the number of times a subcommand was
  encountered. [#178][]
- Parsing is now done in phases: `shortcurcuits`, `ini`, `env`, `callbacks`, and
  `requirements`; all subcommands complete a phase before moving on. [#178][]
- Calling parse multiple times is now officially supported without `clear`
  (automatic). [#178][]
- Dropped the mostly undocumented `short_circuit` property, as help flag parsing
  is a bit more complex, and the default callback behavior of options now works
  properly. [#179][]
- Use the standard `BUILD_TESTING` over `CLI11_TESTING` if defined [#183][]
- Cleanup warnings [#191][]
- Remove deprecated names: `set_footer`, `set_name`, `set_callback`, and
  `set_type_name`. Use without the `set_` instead. [#192][]

> ### Converting from CLI11 1.6
>
> - `->short_circuit()` is no longer needed, just remove it if you were using
>   it - raising an exception will happen in the proper place now without it.
> - `->add_set*` becomes `->add_mutable_set*` if you were using the editable set
>   feature
> - `footer`, `name`, `callback`, and `type_name` must be used instead of the
>   `set_*` versions (deprecated previously).

[#178]: https://github.com/CLIUtils/CLI11/pull/178
[#183]: https://github.com/CLIUtils/CLI11/pull/183
[#185]: https://github.com/CLIUtils/CLI11/pull/185
[#186]: https://github.com/CLIUtils/CLI11/pull/186
[#187]: https://github.com/CLIUtils/CLI11/pull/187
[#190]: https://github.com/CLIUtils/CLI11/pull/190
[#191]: https://github.com/CLIUtils/CLI11/pull/191
[#192]: https://github.com/CLIUtils/CLI11/pull/192
[#197]: https://github.com/CLIUtils/CLI11/pull/197
[#195]: https://github.com/CLIUtils/CLI11/issues/195
[#199]: https://github.com/CLIUtils/CLI11/pull/199
[#200]: https://github.com/CLIUtils/CLI11/pull/200

### Version 1.7.1: Quick patch

This version provides a quick patch for a (correct) warning from GCC 8 for the
windows options code.

- Fix for Windows style option parsing [#201][]
- Improve `add_subcommand` when throwing an exception [#204][]
- Better metadata for Conan package [#202][]

[#201]: https://github.com/CLIUtils/CLI11/pull/201
[#202]: https://github.com/CLIUtils/CLI11/pull/202
[#204]: https://github.com/CLIUtils/CLI11/pull/204

## Version 1.6: Formatting help

Added a new formatting system [#109][]. You can now set the formatter on Apps.
This has also simplified the internals of Apps and Options a bit by separating
most formatting code.

- Added `CLI::Formatter` and `formatter` slot for apps, inherited.
- `FormatterBase` is the minimum required.
- `FormatterLambda` provides for the easy addition of an arbitrary function.
- Added `help_all` support (not added by default).

Changes to the help system (most normal users will not notice this):

- Renamed `single_name` to `get_name(false, false)` (the default).
- The old `get_name()` is now `get_name(false, true)`.
- The old `get_pname()` is now `get_name(true, false)`.
- Removed `help_*` functions.
- Protected function `_has_help_positional` removed.
- `format_help` can now be chained.
- Added getters for the missing parts of options (help no longer uses any
  private parts).
- Help flags now use new `short_circuit` property to simplify parsing. [#121][]

New for Config file reading and writing [#121][]:

- Overridable, bidirectional Config.
- ConfigINI provided and used by default.
- Renamed ini to config in many places.
- Has `config_formatter()` and `get_config_formatter()`.
- Dropped prefix argument from `config_to_str`.
- Added `ConfigItem`.
- Added an example of a custom config format using [nlohmann/json][]. [#138][]

Validators are now much more powerful [#118][], all built in validators upgraded
to the new form:

- A subclass of `CLI::Validator` is now also accepted.
- They now can set the type name to things like `PATH` and `INT in [1-4]`.
- Validators can be combined with `&` and `|`.
- Old form simple validators are still accepted.

Other changes:

- Fixing `parse(args)`'s `args` setting and ordering after parse. [#141][]
- Replaced `set_custom_option` with `type_name` and `type_size` instead of
  `set_custom_option`. Methods return `this`. [#136][]
- Dropped `set_` on Option's `type_name`, `default_str`, and `default_val`.
  [#136][]
- Removed `set_` from App's `failure_message`, `footer`, `callback`, and `name`.
  [#136][]
- Fixed support `N<-1` for `type_size`. [#140][]
- Added `->each()` to make adding custom callbacks easier. [#126][]
- Allow empty options `add_option("-n",{})` to be edited later with `each`
  [#142][]
- Added filter argument to `get_subcommands`, `get_options`; use empty filter
  `{}` to avoid filtering.
- Added `get_groups()` to get groups.
- Better support for manual options with `get_option`, `set_results`, and
  `empty`. [#119][]
- `lname` and `sname` have getters, added `const get_parent`. [#120][]
- Using `add_set` will now capture L-values for sets, allowing further
  modification. [#113][]
- Dropped duplicate way to run `get_type_name` (`get_typeval`).
- Removed `requires` in favor of `needs` (deprecated in last version). [#112][]
- Const added to argv. [#126][]

Backend and testing changes:

- Internally, `type_name` is now a lambda function; for sets, this reads the set
  live. [#116][]
- Cleaner tests without `app.reset()` (and `reset` is now `clear`). [#141][]
- Better CMake policy handling. [#110][]
- Includes are properly sorted. [#120][]
- Testing (only) now uses submodules. [#111][]

[#109]: https://github.com/CLIUtils/CLI11/pull/109
[#110]: https://github.com/CLIUtils/CLI11/pull/110
[#111]: https://github.com/CLIUtils/CLI11/pull/111
[#112]: https://github.com/CLIUtils/CLI11/pull/112
[#113]: https://github.com/CLIUtils/CLI11/issues/113
[#116]: https://github.com/CLIUtils/CLI11/pull/116
[#118]: https://github.com/CLIUtils/CLI11/pull/118
[#119]: https://github.com/CLIUtils/CLI11/pull/119
[#120]: https://github.com/CLIUtils/CLI11/pull/120
[#121]: https://github.com/CLIUtils/CLI11/pull/121
[#126]: https://github.com/CLIUtils/CLI11/pull/126
[#136]: https://github.com/CLIUtils/CLI11/pull/136
[#138]: https://github.com/CLIUtils/CLI11/pull/138
[#140]: https://github.com/CLIUtils/CLI11/pull/140
[#141]: https://github.com/CLIUtils/CLI11/pull/141
[#142]: https://github.com/CLIUtils/CLI11/pull/142
[nlohmann/json]: https://github.com/nlohmann/json

### Version 1.6.1: Platform fixes

This version provides a few fixes for special cases, such as mixing with
`Windows.h` and better defaults for systems like Hunter. The one new feature is
the ability to produce "branded" single file output for providing custom
namespaces or custom macro names.

- Added fix and test for including Windows.h [#145][]
- No longer build single file by default if main project, supports systems stuck
  on Python 2.6 [#149][], [#151][]
- Branding support for single file output [#150][]

[#145]: https://github.com/CLIUtils/CLI11/pull/145
[#149]: https://github.com/CLIUtils/CLI11/pull/149
[#150]: https://github.com/CLIUtils/CLI11/pull/150
[#151]: https://github.com/CLIUtils/CLI11/pull/151

### Version 1.6.2: Help-all

This version fixes some formatting bugs with help-all. It also adds fixes for
several warnings, including an experimental optional error on Clang 7. Several
smaller fixes.

- Fixed help-all formatting [#163][]
  - Printing help-all on nested command now fixed (App)
  - Missing space after help-all restored (Default formatter)
  - More detail printed on help all (Default formatter)
  - Help-all subcommands get indented with inner blank lines removed (Default
    formatter)
  - `detail::find_and_replace` added to utilities
- Fixed CMake install as subproject with `CLI11_INSTALL` flag. [#156][]
- Fixed warning about local variable hiding class member with MSVC [#157][]
- Fixed compile error with default settings on Clang 7 and libc++ [#158][]
- Fixed special case of `--help` on subcommands (general fix planned for 1.7)
  [#168][]
- Removing an option with links [#179][]

[#156]: https://github.com/CLIUtils/CLI11/issues/156
[#157]: https://github.com/CLIUtils/CLI11/issues/157
[#158]: https://github.com/CLIUtils/CLI11/issues/158
[#163]: https://github.com/CLIUtils/CLI11/pull/163
[#168]: https://github.com/CLIUtils/CLI11/issues/168
[#179]: https://github.com/CLIUtils/CLI11/pull/179

## Version 1.5: Optionals

This version introduced support for optionals, along with clarification and
examples of custom conversion overloads. Enums now have been dropped from the
automatic conversion system, allowing explicit protection for out-of-range ints
(or a completely custom conversion). This version has some internal cleanup and
improved support for the newest compilers. Several bugs were fixed, as well.

Note: This is the final release with `requires`, please switch to `needs`.

- Fix unlimited short options eating two values before checking for positionals
  when no space present [#90][]
- Symmetric exclude text when excluding options, exclude can be called multiple
  times [#64][]
- Support for `std::optional`, `std::experimental::optional`, and
  `boost::optional` added if `__has_include` is supported [#95][]
- All macros/CMake variables now start with `CLI11_` instead of just `CLI_`
  [#95][]
- The internal stream was not being cleared before use in some cases. Fixed.
  [#95][]
- Using an enum now requires explicit conversion overload [#97][]
- The separator `--` now is removed when it ends unlimited arguments [#100][]

Other, non-user facing changes:

- Added `Macros.hpp` with better C++ mode discovery [#95][]
- Deprecated macros added for all platforms
- C++17 is now tested on supported platforms [#95][]
- Informational printout now added to CTest [#95][]
- Better single file generation [#95][]
- Added support for GTest on MSVC 2017 (but not in C++17 mode, will need next
  version of GTest)
- Types now have a specific size, separate from the expected number - cleaner
  and more powerful internally [#92][]
- Examples now run as part of testing [#99][]

[#64]: https://github.com/CLIUtils/CLI11/issues/64
[#90]: https://github.com/CLIUtils/CLI11/issues/90
[#92]: https://github.com/CLIUtils/CLI11/issues/92
[#95]: https://github.com/CLIUtils/CLI11/pull/95
[#97]: https://github.com/CLIUtils/CLI11/pull/97
[#99]: https://github.com/CLIUtils/CLI11/pull/99
[#100]: https://github.com/CLIUtils/CLI11/pull/100

### Version 1.5.1: Access

This patch release adds better access to the App programmatically, to assist
with writing custom converters to other formats. It also improves the help
output, and uses a new feature in CLI11 1.5 to fix an old "quirk" in the way
unlimited options and positionals interact.

- Make mixing unlimited positionals and options more intuitive [#102][]
- Add missing getters `get_options` and `get_description` to App [#105][]
- The app name now can be set, and will override the auto name if present
  [#105][]
- Add `(REQUIRED)` for required options [#104][]
- Print simple name for Needs/Excludes [#104][]
- Use Needs instead of Requires in help print [#104][]
- Groups now are listed in the original definition order [#106][]

[#102]: https://github.com/CLIUtils/CLI11/issues/102
[#104]: https://github.com/CLIUtils/CLI11/pull/104
[#105]: https://github.com/CLIUtils/CLI11/pull/105
[#106]: https://github.com/CLIUtils/CLI11/pull/106

### Version 1.5.2: LICENSE in single header mode

This is a quick patch release that makes LICENSE part of the single header file,
making it easier to include. Minor cleanup from codacy. No significant code
changes from 1.5.1.

### Version 1.5.3: Compiler compatibility

This version fixes older AppleClang compilers by removing the optimization for
casting. The minimum version of Boost Optional supported has been clarified to
be 1.58. CUDA 7.0 NVCC is now supported.

### Version 1.5.4: Optionals

This version fixes the optional search in the single file version; some macros
were not yet defined when it did the search. You can define the
`CLI11_*_OPTIONAL` macros to 0 if needed to eliminate the search.

## Version 1.4: More feedback

This version adds lots of smaller fixes and additions after the refactor in
version 1.3. More ways to download and use CLI11 in CMake have been added. INI
files have improved support.

- Lexical cast is now more strict than before [#68][] and fails on overflow
  [#84][]
- Added `get_parent()` to access the parent from a subcommand
- Added `ExistingPath` validator [#73][]
- `app.allow_ini_extras()` added to allow extras in INI files [#70][]
- Multiline INI comments now supported
- Descriptions can now be written with `config_to_str` [#66][]
- Double printing of error message fixed [#77][]
- Renamed `requires` to `needs` to avoid C++20 keyword [#75][], [#82][]
- MakeSingleHeader now works if outside of git [#78][]
- Adding install support for CMake [#79][], improved support for `find_package`
  [#83][], [#84][]
- Added support for Conan.io [#83][]

[#70]: https://github.com/CLIUtils/CLI11/issues/70
[#75]: https://github.com/CLIUtils/CLI11/issues/75
[#84]: https://github.com/CLIUtils/CLI11/pull/84
[#83]: https://github.com/CLIUtils/CLI11/pull/83
[#82]: https://github.com/CLIUtils/CLI11/pull/82
[#79]: https://github.com/CLIUtils/CLI11/pull/79
[#78]: https://github.com/CLIUtils/CLI11/pull/78
[#77]: https://github.com/CLIUtils/CLI11/pull/77
[#73]: https://github.com/CLIUtils/CLI11/pull/73
[#68]: https://github.com/CLIUtils/CLI11/pull/68
[#66]: https://github.com/CLIUtils/CLI11/pull/66

## Version 1.3: Refactor

This version focused on refactoring several key systems to ensure correct
behavior in the interaction of different settings. Most caveats about features
only working on the main App have been addressed, and extra arguments have been
reworked. Inheritance of defaults makes configuring CLI11 much easier without
having to subclass. Policies add new ways to handle multiple arguments to match
your favorite CLI programs. Error messages and help messages are better and more
flexible. Several bugs and odd behaviors in the parser have been fixed.

- Added a version macro, `CLI11_VERSION`, along with `*_MAJOR`, `*_MINOR`, and
  `*_PATCH`, for programmatic access to the version.
- Reworked the way defaults are set and inherited; explicit control given to
  user with `->option_defaults()`
  [#48](https://github.com/CLIUtils/CLI11/pull/48)
- Hidden options now are based on an empty group name, instead of special
  "hidden" keyword [#48](https://github.com/CLIUtils/CLI11/pull/48)
- `parse` no longer returns (so `CLI11_PARSE` is always usable)
  [#37](https://github.com/CLIUtils/CLI11/pull/37)
- Added `remaining()` and `remaining_size()`
  [#37](https://github.com/CLIUtils/CLI11/pull/37)
- `allow_extras` and `prefix_command` are now valid on subcommands
  [#37](https://github.com/CLIUtils/CLI11/pull/37)
- Added `take_last` to only take last value passed
  [#40](https://github.com/CLIUtils/CLI11/pull/40)
- Added `multi_option_policy` and shortcuts to provide more control than just a
  take last policy [#59](https://github.com/CLIUtils/CLI11/pull/59)
- More detailed error messages in a few cases
  [#41](https://github.com/CLIUtils/CLI11/pull/41)
- Footers can be added to help [#42](https://github.com/CLIUtils/CLI11/pull/42)
- Help flags are easier to customize
  [#43](https://github.com/CLIUtils/CLI11/pull/43)
- Subcommand now support groups [#46](https://github.com/CLIUtils/CLI11/pull/46)
- `CLI::RuntimeError` added, for easy exit with error codes
  [#45](https://github.com/CLIUtils/CLI11/pull/45)
- The clang-format script is now no longer "hidden"
  [#48](https://github.com/CLIUtils/CLI11/pull/48)
- The order is now preserved for subcommands (list and callbacks)
  [#49](https://github.com/CLIUtils/CLI11/pull/49)
- Tests now run individually, utilizing CMake 3.10 additions if possible
  [#50](https://github.com/CLIUtils/CLI11/pull/50)
- Failure messages are now customizable, with a shorter default
  [#52](https://github.com/CLIUtils/CLI11/pull/52)
- Some improvements to error codes
  [#53](https://github.com/CLIUtils/CLI11/pull/53)
- `require_subcommand` now offers a two-argument form and negative values on the
  one-argument form are more useful
  [#51](https://github.com/CLIUtils/CLI11/pull/51)
- Subcommands no longer match after the max required number is obtained
  [#51](https://github.com/CLIUtils/CLI11/pull/51)
- Unlimited options no longer prioritize over remaining/unlimited positionals
  [#51](https://github.com/CLIUtils/CLI11/pull/51)
- Added `->transform` which modifies the string parsed
  [#54](https://github.com/CLIUtils/CLI11/pull/54)
- Changed of API in validators to `void(std::string &)` (const for users),
  throwing providing nicer errors
  [#54](https://github.com/CLIUtils/CLI11/pull/54)
- Added `CLI::ArgumentMismatch` [#56](https://github.com/CLIUtils/CLI11/pull/56)
  and fixed missing failure if one arg expected
  [#55](https://github.com/CLIUtils/CLI11/issues/55)
- Support for minimum unlimited expected arguments
  [#56](https://github.com/CLIUtils/CLI11/pull/56)
- Single internal arg parse function
  [#56](https://github.com/CLIUtils/CLI11/pull/56)
- Allow options to be disabled from INI file, rename `add_config` to
  `set_config` [#60](https://github.com/CLIUtils/CLI11/pull/60)

> ### Converting from CLI11 1.2
>
> - `app.parse` no longer returns a vector. Instead, use `app.remaining(true)`.
> - `"hidden"` is no longer a special group name, instead use `""`
> - Validators API has changed to return an error string; use `.empty()` to get
>   the old bool back
> - Use `.set_help_flag` instead of accessing the help pointer directly
>   (discouraged, but not removed yet)
> - `add_config` has been renamed to `set_config`
> - Errors thrown in some cases are slightly more specific

## Version 1.2: Stability

This release focuses on making CLI11 behave properly in corner cases, and with
config files on the command line. This includes fixes for a variety of reported
issues. A few features were added to make life easier, as well; such as a new
flag callback and a macro for the parse command.

- Added functional form of flag
  [#33](https://github.com/CLIUtils/CLI11/pull/33), automatic on C++14
- Fixed Config file search if passed on command line
  [#30](https://github.com/CLIUtils/CLI11/issues/30)
- Added `CLI11_PARSE(app, argc, argv)` macro for simple parse commands (does not
  support returning arg)
- The name string can now contain spaces around commas
  [#29](https://github.com/CLIUtils/CLI11/pull/29)
- `set_default_str` now only sets string, and `set_default_val` will evaluate
  the default string given [#26](https://github.com/CLIUtils/CLI11/issues/26)
- Required positionals now take priority over subcommands
  [#23](https://github.com/CLIUtils/CLI11/issues/23)
- Extra requirements enforced by Travis

## Version 1.1: Feedback

This release incorporates feedback from the release announcement. The examples
are slowly being expanded, some corner cases improved, and some new
functionality for tricky parsing situations.

- Added simple support for enumerations, allow non-printable objects
  [#12](https://github.com/CLIUtils/CLI11/issues/12)
- Added `app.parse_order()` with original parse order
  ([#13](https://github.com/CLIUtils/CLI11/issues/13),
  [#16](https://github.com/CLIUtils/CLI11/pull/16))
- Added `prefix_command()`, which is like `allow_extras` but ceases processing
  and puts all remaining args in the remaining_args structure.
  [#8](https://github.com/CLIUtils/CLI11/issues/8),
  [#17](https://github.com/CLIUtils/CLI11/pull/17))
- Removed Windows warning ([#10](https://github.com/CLIUtils/CLI11/issues/10),
  [#20](https://github.com/CLIUtils/CLI11/pull/20))
- Some improvements to CMake, detect Python and no dependencies on Python 2
  (like Python 3) ([#18](https://github.com/CLIUtils/CLI11/issues/18),
  [#21](https://github.com/CLIUtils/CLI11/pull/21))

## Version 1.0: Official release

This is the first stable release for CLI11. Future releases will try to remain
backward compatible and will follow semantic versioning if possible. There were
a few small changes since version 0.9:

- Cleanup using `clang-tidy` and `clang-format`
- Small improvements to Timers, easier to subclass Error
- Move to 3-Clause BSD license

## Version 0.9: Polish

This release focused on cleaning up the most exotic compiler warnings, fixing a
few oddities of the config parser, and added a more natural method to check
subcommands.

- Better CMake named target (CLI11)
- More warnings added, fixed
- Ini output now includes `=false` when `default_also` is true
- Ini no longer lists the help pointer
- Added test for inclusion in multiple files and linking, fixed issues (rarely
  needed for CLI, but nice for tools)
- Support for complex numbers
- Subcommands now test true/false directly or with `->parsed()`, cleaner parse

## Version 0.8: CLIUtils

This release moved the repository to the CLIUtils main organization.

- Moved to CLIUtils on GitHub
- Fixed docs build and a few links

## Version 0.7: Code coverage 100%

Lots of small bugs fixed when adding code coverage, better in edge cases. Much
more powerful ini support.

- Allow comments in ini files (lines starting with `;`)
- Ini files support flags, vectors, subcommands
- Added CodeCov code coverage reports
- Lots of small bugfixes related to adding tests to increase coverage to 100%
- Error handling now uses scoped enum in errors
- Reparsing rules changed a little to accommodate Ini files. Callbacks are now
  called when parsing INI, and reset any time results are added.
- Adding extra utilities in full version only, `Timer` (not needed for parsing,
  but useful for general CLI applications).
- Better support for custom `add_options` like functions.

## Version 0.6: Cleanup

Lots of cleanup and docs additions made it into this release. Parsing is simpler
and more robust; fall through option added and works as expected; much more
consistent variable names internally.

- Simplified parsing to use `vector<string>` only
- Fixed fallthrough, made it optional as well (default: off): `.fallthrough()`.
- Added string versions of `->requires()` and `->excludes()` for consistency.
- Renamed protected members for internal consistency, grouped docs.
- Added the ability to add a number to `.require_subcommand()`.

## Version 0.5: Windows support

- Allow `Hidden` options.
- Throw `OptionAlreadyAdded` errors for matching subcommands or options, with
  ignore-case included, tests
- `->ignore_case()` added to subcommands, options, and `add_set_ignore_case`.
  Subcommands inherit setting from parent App on creation.
- Subcommands now can be "chained", that is, left over arguments can now include
  subcommands that then get parsed. Subcommands are now a list
  (`get_subcommands`). Added `got_subcommand(App_or_name)` to check for
  subcommands.
- Added `.allow_extras()` to disable error on failure. Parse returns a vector of
  leftover options. Renamed error to `ExtrasError`, and now triggers on extra
  options too.
- Added `require_subcommand` to `App`, to simplify forcing subcommands. Do
  **not** do `add_subcommand()->require_subcommand`, since that is the
  subcommand, not the main `App`.
- Added printout of ini file text given parsed options, skips flags.
- Support for quotes and spaces in ini files
- Fixes to allow support for Windows (added Appveyor) (Uses `-`, not `/` syntax)

## Version 0.4: Ini support

- Updates to help print
- Removed `run`, please use `parse` unless you subclass and add it
- Supports ini files mixed with command line, tested
- Added Range for further Plumbum compatibility
- Added function to print out ini file

## Version 0.3: Plumbum compatibility

- Added `->requires`, `->excludes`, and `->envname` from
  [Plumbum](http://plumbum.readthedocs.io/en/latest/)
- Supports `->mandatory` from Plumbum
- More tests for help strings, improvements in formatting
- Support type and set syntax in positionals help strings
- Added help groups, with `->group("name")` syntax
- Added initial support for ini file reading with `add_config` option.
- Supports GCC 4.7 again
- Clang 3.5 now required for tests due to googlemock usage, 3.4 should still
  work otherwise
- Changes `setup` for an explicit help bool in constructor/`add_subcommand`

## Version 0.2: Leaner and meaner

- Moved to simpler syntax, where `Option` pointers are returned and operated on
- Removed `make_` style options
- Simplified Validators, now only requires `->check(function)`
- Removed Combiners
- Fixed pointers to Options, stored in `unique_ptr` now
- Added `Option_p` and `App_p`, mostly for internal use
- Startup sequence, including help flag, can be modified by subclasses

## Version 0.1: First release

First release before major cleanup. Still has make syntax and combiners; very
clever syntax but not the best or most commonly expected way to work.
