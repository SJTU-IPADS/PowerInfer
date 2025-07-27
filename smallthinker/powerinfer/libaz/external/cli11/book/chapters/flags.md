# Adding Flags

The most basic addition to a command line program is a flag. This is simply
something that does not take any arguments. Adding a flag in CLI11 is done in
one of three ways.

## Boolean flags

The simplest way to add a flag is probably a boolean flag:

```cpp
bool my_flag{false};
app.add_flag("-f", my_flag, "Optional description");
```

This will bind the flag `-f` to the boolean `my_flag`. After the parsing step,
`my_flag` will be `false` if the flag was not found on the command line, or
`true` if it was. By default, it will be allowed any number of times, but if you
explicitly\[^1\] request `->take_last(false)`, it will only be allowed once;
passing something like `./my_app -f -f` or `./my_app -ff` will throw a
`ParseError` with a nice help description. A flag name may start with any
character except ('-', ' ', '\n', and '!'). For long flags, after the first
character all characters are allowed except ('=',':','{',' ', '\n'). Names are
given as a comma separated string, with the dash or dashes. A flag can have as
many names as you want, and afterward, using `count`, you can use any of the
names, with dashes as needed.

## Integer flags

If you want to allow multiple flags and count their value, simply use any
integral variables instead of a bool:

```cpp
int my_flag{0};
app.add_flag("-f", my_flag, "Optional description");
```

After the parsing step, `my_flag` will contain the number of times this flag was
found on the command line, including 0 if not found.

This behavior can also be controlled manually via
`->multi_option_policy(CLI::MultiOptionPolicy::Sum)` as of version 2.2.

## Arbitrary type flags

CLI11 allows the type of the variable to assign to in the `add_flag` function to
be any supported type. This is particularly useful in combination with
specifying default values for flags. The allowed types include bool, int, float,
vector, enum, or string-like.

### Default Flag Values

Flag options specified through the `add_flag*` functions allow a syntax for the
option names to default particular options to a false value or any other value
if some flags are passed. For example:

```cpp
app.add_flag("--flag,!--no-flag",result,"help for flag");
```

specifies that if `--flag` is passed on the command line result will be true or
contain a value of 1. If `--no-flag` is passed `result` will contain false or -1
if `result` is a signed integer type, or 0 if it is an unsigned type. An
alternative form of the syntax is more explicit: `"--flag,--no-flag{false}"`;
this is equivalent to the previous example. This also works for short form
options `"-f,!-n"` or `"-f,-n{false}"`. If `variable_to_bind_to` is anything but
an integer value the default behavior is to take the last value given, while if
`variable_to_bind_to` is an integer type the behavior will be to sum all the
given arguments and return the result. This can be modified if needed by
changing the `multi_option_policy` on each flag (this is not inherited). The
default value can be any value. For example if you wished to define a numerical
flag:

```cpp
app.add_flag("-1{1},-2{2},-3{3}",result,"numerical flag")
```

using any of those flags on the command line will result in the specified number
in the output. Similar things can be done for string values, and enumerations,
as long as the default value can be converted to the given type.

## Pure flags

Every command that starts with `add_`, such as the flag commands, return a
pointer to the internally stored `CLI::Option` that describes your addition. If
you prefer, you can capture this pointer and use it, and that allows you to skip
adding a variable to bind to entirely:

```cpp
CLI::Option* my_flag = app.add_flag("-f", "Optional description");
```

After parsing, you can use `my_flag->count()` to count the number of times this
was found. You can also directly use the value (`*my_flag`) as a bool.
`CLI::Option` will be discussed in more detail later.

## Callback flags

If you want to define a callback that runs when you make a flag, you can use
`add_flag_function` (C++11 or newer) or `add_flag` (C++14 or newer only) to add
a callback function. The function should have the signature `void(std::size_t)`.
This could be useful for a version printout, etc.

```cpp
auto callback = [](int count){std::cout << "This was called " << count << " times";};
app.add_flag_function("-c", callback, "Optional description");
```

## Aliases

The name string, the first item of every `add_` method, can contain as many
short and long names as you want, separated by commas. For example,
`"-a,--alpha,-b,--beta"` would allow any of those to be recognized on the
command line. If you use the same name twice, or if you use the same name in
multiple flags, CLI11 will immediately throw a `CLI::ConstructionError`
describing your problem (it will not wait until the parsing step).

If you want to make an option case-insensitive, you can use the
`->ignore_case()` method on the `CLI::Option` to do that. For example,

```cpp
bool flag{false};
app.add_flag("--flag", flag)
    ->ignore_case();
```

would allow the following to count as passing the flag:

```term
gitbook $ ./my_app --fLaG
```

## Example

The following program will take several flags:

[include:"define"](../code/flags.cpp)

The values would be used like this:

[include:"usage"](../code/flags.cpp)

[Source code](https://github.com/CLIUtils/CLI11/tree/main/book/code/flags.cpp)

If you compile and run:

```term
gitbook:examples $ g++ -std=c++11 flags.cpp
gitbook:examples $ ./a.out -h
Flag example program
Usage: ./a.out [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -b,--bool                   This is a bool flag
  -i,--int                    This is an int flag
  -p,--plain                  This is a plain flag

gitbook:examples $ ./a.out -bii --plain -i
The flags program
Bool flag passed
Flag int: 3
Flag plain: 1
```

\[^1\]: It will not inherit this from the parent defaults, since this is often
useful even if you don't want all options to allow multiple passed options.
