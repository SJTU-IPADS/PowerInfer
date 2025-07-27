# Installation

## Single file edition

```cpp
#include <CLI11.hpp>
```

This example uses the single file edition of CLI11. You can download `CLI11.hpp`
from the latest release and put it into the same folder as your source code,
then compile this with C++ enabled. For a larger project, you can just put this
in an include folder and you are set. This is the simplest and most
straightforward means of including CLI11 with a project.

## Full edition

```cpp
#include <CLI/CLI.hpp>
```

If you want to use CLI11 in its full form, you can also use the original
multiple file edition. This has an extra utility (`Timer`), and is does not
require that you use a release. The only change to your code would be the
include shown above.

### CMake support for the full edition

If you use CMake 3.5+ for your project (highly recommended), CLI11 comes with a
powerful CMakeLists.txt file that was designed to also be used with
`add_subproject`. You can add the repository to your code (preferably as a git
submodule), then add the following line to your project (assuming your folder is
called CLI11):

```cmake
add_subdirectory(CLI11)
```

Then, you will have a target `CLI11::CLI11` that you can link to with
`target_link_libraries`. It will provide the include paths you need for the
library. This is the way [GooFit](https://github.com/GooFit/GooFit) uses CLI11,
for example.

You can also configure and optionally install CLI11, and CMake will create the
necessary `lib/cmake/CLI11/CLI11Config.cmake` files, so
`find_package(CLI11 CONFIG REQUIRED)` also works.

If you use conan.io, CLI11 supports that too. CLI11 also supports Meson and
pkg-config if you are not using CMake.

If the CMake option `CLI11_PRECOMPILED` is set then the library is compiled into
a static library. This can be used to improve compile times if CLI11 is included
in many different parts of a project.

#### Global Headers

Use `CLI/*.hpp` files stored in a shared folder. You could check out the git
repository to a system-wide folder, for example `/opt/`. With CMake, you could
add to the include path via:

```bash
if(NOT DEFINED CLI11_DIR)
set (CLI11_DIR "/opt/CLI11" CACHE STRING "CLI11 git repository")
endif()
include_directories(${CLI11_DIR}/include)
```

And then in the source code (adding several headers might be needed to prevent
linker errors):

```cpp
#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"
```

#### Global Headers with Target

configuring and installing the project is required for linking CLI11 to your
project in the same way as you would do with any other external library. With
CMake, this step allows using `find_package(CLI11 CONFIG REQUIRED)` and then
using the `CLI11::CLI11` target when linking. If `CMAKE_INSTALL_PREFIX` was
changed during install to a specific folder like `/opt/CLI11`, then you have to
pass `-DCLI11_DIR=/opt/CLI11` when building your current project. You can also
use [Conan.io](https://conan.io/center/cli11) or
[Hunter](https://docs.hunter.sh/en/latest/packages/pkg/CLI11.html). (These are
just conveniences to allow you to use your favorite method of managing packages;
it's just header only so including the correct path and using C++11 is all you
really need.)

#### Using Fetchcontent

If you do not want to add cmake as a submodule or include it with your code the
project can be added using `FetchContent`. This capability requires CMake 3.14+
(or 3.11+ with more work).

An example CMake file would include:

```cmake
include(FetchContent)
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
)

FetchContent_MakeAvailable(cli11_proj)

# And now you can use it
target_link_libraries(<your project> PRIVATE CLI11::CLI11)
```

And use

```c++
#include <CLI/CLI.hpp>
```

in your project. It is highly recommended that you use the git hash for
`GIT_TAG` instead of a tag or branch, as that will both be more secure, as well
as faster to reconfigure - CMake will not have to reach out to the internet to
see if the tag moved. You can also download just the single header file from the
releases using `file(DOWNLOAD)`.

### Running tests on the full edition

CLI11 has examples and tests that can be accessed using a CMake build on any
platform. Simply build and run ctest to run the 200+ tests to ensure CLI11 works
on your system.

As an example of the build system, the following code will download and test
CLI11 in a simple Alpine Linux docker container [^1]:

```term
gitbook:~ $ docker run -it alpine
root:/ # apk add --no-cache g++ cmake make git
fetch ...
root:/ # git clone https://github.com/CLIUtils/CLI11.git
Cloning into 'CLI11' ...
root:/ # cd CLI11
root:CLI11 # mkdir build
root:CLI11 # cd build
root:build # cmake ..
-- The CXX compiler identification is GNU 6.3.0 ...
root:build # make
Scanning dependencies ...
root:build # make test
[warning]Running tests...
Test project /CLI11/build
      Start  1: HelpersTest
 1/10 Test  #1: HelpersTest ......................   Passed    0.01 sec
      Start  2: IniTest
 2/10 Test  #2: IniTest ..........................   Passed    0.01 sec
      Start  3: SimpleTest
 3/10 Test  #3: SimpleTest .......................   Passed    0.01 sec
      Start  4: AppTest
 4/10 Test  #4: AppTest ..........................   Passed    0.02 sec
      Start  5: CreationTest
 5/10 Test  #5: CreationTest .....................   Passed    0.01 sec
      Start  6: SubcommandTest
 6/10 Test  #6: SubcommandTest ...................   Passed    0.01 sec
      Start  7: HelpTest
 7/10 Test  #7: HelpTest .........................   Passed    0.01 sec
      Start  8: NewParseTest
 8/10 Test  #8: NewParseTest .....................   Passed    0.01 sec
      Start  9: TimerTest
 9/10 Test  #9: TimerTest ........................   Passed    0.24 sec
      Start 10: link_test_2
10/10 Test #10: link_test_2 ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 10

Total Test time (real) =   0.34 sec
```

For the curious, the CMake options and defaults are listed below. Most options
default to off if CLI11 is used as a subdirectory in another project.

| Option                         | Description                                                                      |
| ------------------------------ | -------------------------------------------------------------------------------- |
| `CLI11_SINGLE_FILE=ON`         | Build the `CLI11.hpp` file from the sources. Requires Python (version 3 or 2.7). |
| `CLI11_PRECOMPILED=OFF`        | generate a precompiled static library instead of header-only                     |
| `CLI11_SINGLE_FILE_TESTS=OFF`  | Run the tests on the generated single file version as well                       |
| `CLI11_BUILD_DOCS=ON`          | build CLI11 documentation and book                                               |
| `CLI11_BUILD_EXAMPLES=ON`      | Build the example programs.                                                      |
| `CLI11_BUILD_EXAMPLES_JSON=ON` | Build some additional example using json libraries                               |
| `CLI11_INSTALL=ON`             | install CLI11 to the install folder during the install process                   |
| `CLI11_FORCE_LIBCXX=OFF`       | use libc++ instead of libstdc++ if building with clang on linux                  |
| `CLI11_CUDA_TESTS=OFF`         | build the tests with NVCC                                                        |
| `CLI11_BUILD_TESTS=ON`         | Build the tests.                                                                 |

[^1]:
    Docker is being used to create a pristine disposable environment; there is
    nothing special about this container. Alpine is being used because it is
    small, modern, and fast. Commands are similar on any other platform.

## Meson support

### Global Headers from pkg-config

If CLI11 is installed globally, then nothing more than `dependency('CLI11')` is
required. If it installed in a non-default search path, then setting the
`PKG_CONFIG_PATH` environment variable of the `--pkg-config-path` option to
`meson setup` is all that's required.

### Using Meson's subprojects

Meson has a system called
[wraps](https://mesonbuild.com/Wrap-dependency-system-manual.html), which allow
Meson to fetch sources, configure, and build dependencies as part of a main
project. This is the mechanism that Meson recommends for projects to use, as it
allows updating the dependency transparently, and allows packagers to have fine
grained control on the use of subprojects vs system provided dependencies.
Simply run `meson wrap install cli11` to install the `cli11.wrap` file, and
commit it, if desired.

It is also possible to use git submodules. This is generally discouraged by
Meson upstream, but may be appropriate if a project needs to build with multiple
build systems and wishes to share subprojects between them. As long as the
submodule is in the parent project's subproject directory nothing additional is
needed.

## Installing cli11 using vcpkg

You can download and install cli11 using the
[vcpkg](https://github.com/Microsoft/vcpkg) dependency manager:

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install cli11
```

The cli11 port in vcpkg is kept up to date by Microsoft team members and
community contributors. If the version is out of date, please
[create an issue or pull request](https://github.com/Microsoft/vcpkg) on the
vcpkg repository.

## Installing CLI11 using Conan

You can install pre-built binaries for CLI11 or build it from source using
[Conan](https://conan.io/). Use the following command:

```bash
conan install --requires="cli11/[*]" --build=missing
```

The CLI11 Conan recipe is kept up to date by Conan maintainers and community
contributors. If the version is out of date, please
[create an issue or pull request](https://github.com/conan-io/conan-center-index)
on the ConanCenterIndex repository.

## Special instructions for GCC 8, Some clang, and WASI

If you are using GCC 8 and using it in C++17 mode with CLI11. CLI11 makes use of
the `<filesystem>` header if available, but specifically for this compiler, the
`filesystem` library is separate from the standard library and needs to be
linked separately. So it is available but CLI11 doesn't use it by default.

Specifically `libstdc++fs` needs to be added to the linking list and
`CLI11_HAS_FILESYSTEM=1` has to be defined. Then the filesystem variant of the
Validators could be used on GCC 8. GCC 9+ does not have this issue so the
`<filesystem>` is used by default.

There may also be other cases where a specific library needs to be linked.

Defining `CLI11_HAS_FILESYSTEM=0` which will remove the usage and hence any
linking issue.

In some cases certain clang compilations may require linking against `libc++fs`.
These situations have not been encountered so the specific situations requiring
them are unknown yet.

If building with WASI it is necessary to add the flag
`-lc-printscan-long-double` to the build to allow long double support. See #841
for more details.

## Default system packages on Linux

If you are not worried about latest features or recent bug fixes, you can
install a stable version of CLI11 using:

`sudo apt install libcli11-dev` for Ubuntu, or: `sudo dnf install cli11-devel`
on Fedora/Almalinux.

Then, in your CMake project, just call:

```cmake
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE CLI11::CLI11)
```

and in your C++ file:

```cpp
#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

int main(int argc, char** argv)) {
    CLI::App app{"MyApp"};
    // Here your flags / options
    CLI11_PARSE(app, argc, argv);
}
```
