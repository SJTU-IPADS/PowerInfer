# CLI11: An introduction

This gitbook is designed to provide an introduction to using the CLI11 library
to write your own command line programs. The library is designed to be clean,
intuitive, but powerful. There are no requirements beyond C++11 support (and
even `<regex>` support not required). It works on Mac, Linux, and Windows, and
has 100% test coverage on all three systems. You can simply drop in a single
header file (`CLI11.hpp` available in [releases][]) to use CLI11 in your own
application. Other ways to integrate it into a build system are listed in the
[README][].

The library was inspired the Python libraries [Plumbum][] and [Click][], and
incorporates many of their user friendly features. The library is extensively
documented, with a [friendly introduction][readme], this tutorial book, and more
technical [API docs][].

> Feel free to contribute to [this documentation here][cli11tutorial] if
> something can be improved!

The syntax is simple and scales from a basic application to a massive physics
analysis with multiple models and many parameters and switches. For example,
this is a simple program that has an optional parameter that defaults to 0:

```term
gitbook $ ./a.out
Parameter value: 0

gitbook $ ./a.out -p 4
Parameter value: 4

gitbook $ ./a.out --help
App description
Usage: ./a.out [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -p INT                      Parameter
```

Like any good command line application, help is provided. This program can be
implemented in 10 lines:

[include](code/intro.cpp)

[Source code](https://github.com/CLIUtils/CLI11/blob/main/book/code/intro.cpp)

Unlike some other libraries, this is enough to exit correctly and cleanly if
help is requested or if incorrect arguments are passed. You can try this example
out for yourself. To compile with GCC:

```term
gitbook:examples $ c++ -std=c++11 intro.cpp
```

Much more complicated options are handled elegantly:

```cpp
std::string file;
app.add_option("-f,--file", file, "Require an existing file")
  ->required()
  ->check(CLI::ExistingFile);
```

You can use any valid type; the above example could have used a
`boost::file_system` file instead of a `std::string`. The value is a real value
and does not require any special lookups to access. You do not have to risk
typos by repeating the values after parsing like some libraries require. The
library also handles positional arguments, flags, fixed or unlimited repeating
options, interdependent options, flags, custom validators, help groups, and
more.

You can use subcommands, as well. Subcommands support callback lambda functions
when parsed, or they can be checked later. You can infinitely nest subcommands,
and each is a full `App` instance, supporting everything listed above.

Reading/producing `.ini` files for configuration is also supported, as is using
environment variables as input. The base `App` can be subclassed and customized
for use in a toolkit (like [GooFit][]). All the standard shell idioms, like
`--`, work as well.

CLI11 was developed at the [University of Cincinnati][] in support of the
[GooFit][] library under [NSF Award 1414736][nsf 1414736]. It was featured in a
[DIANA/HEP][] meeting at CERN. Please give it a try! Feedback is always welcome.

[goofit]: https://github.com/GooFit/GooFit
[diana/hep]: https://diana-hep.org
[cli11tutorial]: https://cliutils.github.io/CLI11/book
[releases]: https://github.com/CLIUtils/CLI11/releases
[api docs]: https://cliutils.github.io/CLI11
[readme]: https://github.com/CLIUtils/CLI11/blob/main/README.md
[nsf 1414736]: https://nsf.gov/awardsearch/showAward?AWD_ID=1414736
[university of cincinnati]: https://www.uc.edu
[plumbum]: https://plumbum.readthedocs.io/en/latest/
[click]: https://click.palletsprojects.com
