# Options

## Simple options

The most versatile addition to a command line program is an option. This is like
a flag, but it takes an argument. CLI11 handles all the details for many types
of options for you, based on their type. To add an option:

```cpp
int int_option{0};
app.add_option("-i", int_option, "Optional description");
```

This will bind the option `-i` to the integer `int_option`. On the command line,
a single value that can be converted to an integer will be expected. Non-integer
results will fail. If that option is not given, CLI11 will not touch the initial
value. This allows you to set up defaults by simply setting your value
beforehand. If you want CLI11 to display your default value, you can add
`->capture_default_str()` after the option.

```cpp
int int_option{0};
app.add_option("-i", int_option, "Optional description")->capture_default_str();
```

You can use any C++ int-like type, not just `int`. CLI11 understands the
following categories of types:

| Type           | CLI11                                                                                                                                                                                                                                                                    |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| number like    | Integers, floats, bools, or any type that can be constructed from an integer or floating point number. Accepts common numerical strings like `0xFF` as well as octal[\0755, or \o755], decimal, and binary(0b011111100), supports value separators including `_` and `'` |
| string-like    | std::string, or anything that can be constructed from or assigned a std::string                                                                                                                                                                                          |
| char           | For a single char, single string values are accepted, otherwise longer strings are treated as integral values and a conversion is attempted                                                                                                                              |
| complex-number | std::complex or any type which has a real(), and imag() operations available, will allow 1 or 2 string definitions like "1+2j" or two arguments "1","2"                                                                                                                  |
| enumeration    | any enum or enum class type is supported through conversion from the underlying type(typically int, though it can be specified otherwise)                                                                                                                                |
| container-like | a container(like vector) of any available types including other containers                                                                                                                                                                                               |
| wrapper        | any other object with a `value_type` static definition where the type specified by `value_type` is one of the type in this list, including `std::atomic<>`                                                                                                               |
| tuple          | a tuple, pair, or array, or other type with a tuple size and tuple_type operations defined and the members being a type contained in this list                                                                                                                           |
| function       | A function that takes an array of strings and returns a string that describes the conversion failure or empty for success. May be the empty function. (`{}`)                                                                                                             |
| streamable     | any other type with a `<<` operator will also work                                                                                                                                                                                                                       |

By default, CLI11 will assume that an option is optional, and one value is
expected if you do not use a vector. You can change this on a specific option
using option modifiers. An option name may start with any character except ('-',
' ', '\n', and '!'). For long options, after the first character all characters
are allowed except ('=',':','{',' ', '\n'). Names are given as a comma separated
string, with the dash or dashes. An option can have as many names as you want,
and afterward, using `count`, you can use any of the names, with dashes as
needed, to count the options. One of the names is allowed to be given without
proceeding dash(es); if present the option is a positional option, and that name
will be used on the help line for its positional form.

## Positional options and aliases

When you give an option on the command line without a name, that is a positional
option. Positional options are accepted in the same order they are defined. So,
for example:

```term
gitbook:examples $ ./a.out one --two three four
```

The string `one` would have to be the first positional option. If `--two` is a
flag, then the remaining two strings are positional. If `--two` is a
one-argument option, then `four` is the second positional. If `--two` accepts
two or more arguments, then there are no more positionals.

To make a positional option, you simply give CLI11 one name that does not start
with a dash. You can have as many (non-overlapping) names as you want for an
option, but only one positional name. So the following name string is valid:

```cpp
"-a,-b,--alpha,--beta,mypos"
```

This would make two short option aliases, two long option alias, and the option
would be also be accepted as a positional.

## Containers of options

If you use a vector or other container instead of a plain option, you can accept
more than one value on the command line. By default, a container accepts as many
options as possible, until the next value that could be a valid option name. You
can specify a set number using an option modifier `->expected(N)`. (The default
unlimited behavior on vectors is restored with `N=-1`) CLI11 does not
differentiate between these two methods for unlimited acceptance options.

| Separate names    | Combined names |
| ----------------- | -------------- |
| `--vec 1 --vec 2` | `--vec 1 2`    |

It is also possible to specify a minimum and maximum number through
`->expected(Min,Max)`. It is also possible to specify a min and max type size
for the elements of the container. It most cases these values will be
automatically determined but a user can manually restrict them.

An example of setting up a vector option:

```cpp
std::vector<int> int_vec;
app.add_option("--vec", int_vec, "My vector option");
```

Vectors will be replaced by the parsed content if the option is given on the
command line.

A definition of a container for purposes of CLI11 is a type with a `end()`,
`insert(...)`, `clear()` and `value_type` definitions. This includes `vector`,
`set`, `deque`, `list`, `forward_iist`, `map`, `unordered_map` and a few others
from the standard library, and many other containers from the boost library.

### Empty containers

By default a container will never return an empty container. If it is desired to
allow an empty container to be returned, then the option must be modified with a
0 as the minimum expected value

```cpp
std::vector<int> int_vec;
app.add_option("--vec", int_vec, "Empty vector allowed")->expected(0,-1);
```

An empty vector can than be specified on the command line as `--vec {}`

To allow an empty vector from config file, the default must be set in addition
to the above modification.

```cpp
std::vector<int> int_vec;
app.add_option("--vec", int_vec, "Empty vector allowed")->expected(0,-1)->default_str("{}");
```

Then in the file

```toml
vec={}
```

or

```toml
vec=[]
```

will generate an empty vector in `int_vec`.

### Containers of containers

Containers of containers are also supported.

```cpp
std::vector<std::vector<int>> int_vec;
app.add_option("--vec", int_vec, "My vector of vectors option");
```

CLI11 inserts a separator sequence at the start of each argument call to
separate the vectors. So unless the separators are injected as part of the
command line each call of the option on the command line will result in a
separate element of the outer vector. This can be manually controlled via
`inject_separator(true|false)` but in nearly all cases this should be left to
the defaults. To insert of a separator from the command line add a `%%` where
the separation should occur.

```bash
cmd --vec_of_vec 1 2 3 4 %% 1 2
```

would then result in a container of size 2 with the first element containing 4
values and the second 2.

This separator is also the only way to get values into something like

```cpp
std::pair<std::vector<int>,std::vector<int>> two_vecs;
app.add_option("--vec", two_vecs, "pair of vectors");
```

without calling the argument twice.

Further levels of nesting containers should compile but intermediate layers will
only have a single element in the container, so is probably not that useful.

### Nested types

Types can be nested. For example:

```cpp
std::map<int, std::pair<int,std::string>> map;
app.add_option("--dict", map, "map of pairs");
```

will require 3 arguments for each invocation, and multiple sets of 3 arguments
can be entered for a single invocation on the command line.

```cpp
std::map<int, std::pair<int,std::vector<std::string>>> map;
app.add_option("--dict", map, "map of pairs");
```

will result in a requirement for 2 integers on each invocation and absorb an
unlimited number of strings including 0.

## Option modifiers

When you call `add_option`, you get a pointer to the added option. You can use
that to add option modifiers. A full listing of the option modifiers:

| Modifier                                                | Description                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `->required()`                                          | The program will quit if this option is not present. This is `mandatory` in Plumbum, but required options seems to be a more standard term. For compatibility, `->mandatory()` also works.                                                                                                                                                                                                                                                                |
| `->expected(N)`                                         | Take `N` values instead of as many as possible, mainly for vector args.                                                                                                                                                                                                                                                                                                                                                                                   |
| `->expected(Nmin,Nmax)`                                 | Take between `Nmin` and `Nmax` values.                                                                                                                                                                                                                                                                                                                                                                                                                    |
| `->type_size(N)`                                        | specify that each block of values would consist of N elements                                                                                                                                                                                                                                                                                                                                                                                             |
| `->type_size(Nmin,Nmax)`                                | specify that each block of values would consist of between Nmin and Nmax elements                                                                                                                                                                                                                                                                                                                                                                         |
| `->needs(opt)`                                          | This option requires another option to also be present, opt is an `Option` pointer or a string with the name of the option. Can be removed with `->remove_needs(opt)`                                                                                                                                                                                                                                                                                     |
| `->excludes(opt)`                                       | This option cannot be given with `opt` present, opt is an `Option` pointer or a string with the name of the option. Can be removed with `->remove_excludes(opt)`                                                                                                                                                                                                                                                                                          |
| `->envname(name)`                                       | Gets the value from the environment if present and not passed on the command line and passes any validators.                                                                                                                                                                                                                                                                                                                                              |
| `->group(name)`                                         | The help group to put the option in. No effect for positional options. Defaults to `"Options"`. Options given an empty string for the group name will not show up in the help print.                                                                                                                                                                                                                                                                      |
| `->description(string)`                                 | Set/change the description                                                                                                                                                                                                                                                                                                                                                                                                                                |
| `->ignore_case()`                                       | Ignore the case on the command line (also works on subcommands, does not affect arguments).                                                                                                                                                                                                                                                                                                                                                               |
| `->ignore_underscore()`                                 | Ignore any underscores on the command line (also works on subcommands, does not affect arguments).                                                                                                                                                                                                                                                                                                                                                        |
| `->allow_extra_args()`                                  | Allow extra argument values to be included when an option is passed. Enabled by default for vector options.                                                                                                                                                                                                                                                                                                                                               |
| `->disable_flag_override()`                             | specify that flag options cannot be overridden on the command line use `=<newval>`                                                                                                                                                                                                                                                                                                                                                                        |
| `->delimiter('<CH>')`                                   | specify a character that can be used to separate elements in a command line argument, default is <none>, common values are ',', and ';'                                                                                                                                                                                                                                                                                                                   |
| `->multi_option_policy( CLI::MultiOptionPolicy::Throw)` | Sets the policy for handling multiple arguments if the option was received on the command line several times. `Throw`ing an error is the default, but `TakeLast`, `TakeFirst`, `TakeAll`, `Join`, `Reverse`, and `Sum` are also available. See the next four lines for shortcuts to set this more easily.                                                                                                                                                 |
| `->take_last()`                                         | Only use the last option if passed several times. This is always true by default for bool options, regardless of the app default, but can be set to false explicitly with `->multi_option_policy()`.                                                                                                                                                                                                                                                      |
| `->take_first()`                                        | sets `->multi_option_policy(CLI::MultiOptionPolicy::TakeFirst)`                                                                                                                                                                                                                                                                                                                                                                                           |
| `->take_all()`                                          | sets `->multi_option_policy(CLI::MultiOptionPolicy::TakeAll)`                                                                                                                                                                                                                                                                                                                                                                                             |
| `->join()`                                              | sets `->multi_option_policy(CLI::MultiOptionPolicy::Join)`, which uses newlines or the specified delimiter to join all arguments into a single string output.                                                                                                                                                                                                                                                                                             |
| `->join(delim)`                                         | sets `->multi_option_policy(CLI::MultiOptionPolicy::Join)`, which uses `delim` to join all arguments into a single string output. this also sets the delimiter                                                                                                                                                                                                                                                                                            |
| `->check(Validator)`                                    | perform a check on the returned results to verify they meet some criteria. See [Validators](./validators.md) for more info                                                                                                                                                                                                                                                                                                                                |
| `->transform(Validator)`                                | Run a transforming validator on each value passed. See [Validators](./validators.md) for more info                                                                                                                                                                                                                                                                                                                                                        |
| `->each(void(std::string))`                             | Run a function on each parsed value, _in order_.                                                                                                                                                                                                                                                                                                                                                                                                          |
| `->default_str(string)`                                 | set a default string for use in the help and as a default value if no arguments are passed and a value is requested                                                                                                                                                                                                                                                                                                                                       |
| `->default_function(std::string())`                     | Advanced: Change the function that `capture_default_str()` uses.                                                                                                                                                                                                                                                                                                                                                                                          |
| `->default_val(value)`                                  | Generate the default string from a value and validate that the value is also valid. For options that assign directly to a value type the value in that type is also updated. Value must be convertible to a string(one of known types or have a stream operator).                                                                                                                                                                                         |
| `->capture_default_str()`                               | Store the current value attached and display it in the help string.                                                                                                                                                                                                                                                                                                                                                                                       |
| `->always_capture_default()`                            | Always run `capture_default_str()` when creating new options. Only useful on an App's `option_defaults`.                                                                                                                                                                                                                                                                                                                                                  |
| `->run_callback_for_default()`                          | Force the option callback to be executed or the variable set when the `default_val` is used.                                                                                                                                                                                                                                                                                                                                                              |
| `->force_callback()`                                    | Force the option callback to be executed regardless of whether the option was used or not. Will use the default_str if available, if no default is given the callback will be executed with an empty string as an argument, which will translate to a default initialized value, which can be compiler dependent                                                                                                                                          |
| `->trigger_on_parse()`                                  | Have the option callback be triggered when the value is parsed vs. at the end of all parsing, the option callback can potentially be executed multiple times. Generally only useful if you have a user defined callback or validation check. Or potentially if a vector input is given multiple times as it will clear the results when a repeat option is given via command line. It will trigger the callbacks once per option call on the command line |
| `->option_text(string)`                                 | Sets the text between the option name and description.                                                                                                                                                                                                                                                                                                                                                                                                    |

The `->check(...)` and `->transform(...)` modifiers can also take a callback
function of the form `bool function(std::string)` that runs on every value that
the option receives, and returns a value that tells CLI11 whether the check
passed or failed.

### Multi Option policy

The Multi option policy can be used to instruct CLI11 what to do when an option
is called multiple times and how to return those values in a meaningful way.
There are several options can be set through the
`->multi_option_policy( CLI::MultiOptionPolicy::Throw)` option modifier.
`Throw`ing an error is the default, but `TakeLast`, `TakeFirst`, `TakeAll`,
`Join`, `Reverse`, and `Sum`

| Value     | Description                                                                       |
| --------- | --------------------------------------------------------------------------------- |
| Throw     | Throws an error if more values are given then expected                            |
| TakeLast  | Selects the last expected number of values given                                  |
| TakeFirst | Selects the first expected number of of values given                              |
| Join      | Joins the strings together using the `delimiter` given                            |
| TakeAll   | Takes all the values                                                              |
| Sum       | If the values are numeric, it sums them and returns the result                    |
| Reverse   | Selects the last expected number of values given and return them in reverse order |

NOTE: For reverse, the index used for an indexed validator is also applied in
reverse order index 1 will be the last element and 2 second from last and so on.

## Using the `CLI::Option` pointer

Each of the option creation mechanisms returns a pointer to the internally
stored option. If you save that pointer, you can continue to access the option,
and change setting on it later. The Option object can also be converted to a
bool to see if it was passed, or `->count()` can be used to see how many times
the option was passed. Since flags are also options, the same methods work on
them.

```cpp
CLI::Option* opt = app.add_flag("--opt");

CLI11_PARSE(app, argv, argc);

if(* opt)
    std::cout << "Flag received " << opt->count() << " times." << '\n';
```

## Inheritance of defaults

One of CLI11's systems to allow customizability without high levels of verbosity
is the inheritance system. You can set default values on the parent `App`, and
all options and subcommands created from it remember the default values at the
point of creation. The default value for Options, specifically, are accessible
through the `option_defaults()` method. There are a number of settings that can
be set and inherited:

- `group`: The group name starts as "Options"
- `required`: If the option must be given. Defaults to `false`. Is ignored for
  flags.
- `multi_option_policy`: What to do if several copies of an option are passed
  and one value is expected. Defaults to `CLI::MultiOptionPolicy::Throw`. This
  is also used for bool flags, but they always are created with the value
  `CLI::MultiOptionPolicy::TakeLast` or `CLI::MultiOptionPolicy::Sum` regardless
  of the default, so that multiple bool flags does not cause an error. But you
  can override that setting by calling the `multi_option_policy` directly.
- `ignore_case`: Allow any mixture of cases for the option or flag name
- `ignore_underscore`: Allow any number of underscores in the option or flag
  name
- `configurable`: Specify whether an option can be configured through a config
  file
- `disable_flag_override`: do not allow flag values to be overridden on the
  command line
- `always_capture_default`: specify that the default values should be
  automatically captured.
- `delimiter`: A delimiter to use for capturing multiple values in a single
  command line string (e.g. --flag="flag,-flag2,flag3")

An example of usage:

```cpp
app.option_defaults()->ignore_case()->group("Required");

app.add_flag("--CaSeLeSs");
app.get_group() // is "Required"
```

Groups are mostly for visual organization, but an empty string for a group name
will hide the option.

### Windows style options

You can also set the app setting `app->allow_windows_style_options()` to allow
windows style options to also be recognized on the command line:

- `/a` (flag)
- `/f filename` (option)
- `/long` (long flag)
- `/file filename` (space)
- `/file:filename` (colon)
- `/long_flag:false` (long flag with : to override the default value)

Windows style options do not allow combining short options or values not
separated from the short option like with `-` options. You still specify option
names in the same manner as on Linux with single and double dashes when you use
the `add_*` functions, and the Linux style on the command line will still work.
If a long and a short option share the same name, the option will match on the
first one defined.

## Parse configuration

How an option and its arguments are parsed depends on a set of controls that are
part of the option structure. In most circumstances these controls are set
automatically based on the type or function used to create the option and the
type the arguments are parsed into. The variables define the size of the
underlying type (essentially how many strings make up the type), the expected
size (how many groups are expected) and a flag indicating if multiple groups are
allowed with a single option. And these interact with the `multi_option_policy`
when it comes time to parse.

### Examples

How options manage this is best illustrated through some examples.

```cpp
std::string val;
app.add_option("--opt",val,"description");
```

creates an option that assigns a value to a `std::string` When this option is
constructed it sets a type_size min and max of 1. Meaning that the assignment
uses a single string. The Expected size is also set to 1 by default, and
`allow_extra_args` is set to false. meaning that each time this option is called
1 argument is expected. This would also be the case if val were a `double`,
`int` or any other single argument types.

now for example

```cpp
std::pair<int, std::string> val;
app.add_option("--opt",val,"description");
```

In this case the typesize is automatically detected to be 2 instead of 1, so the
parsing would expect 2 arguments associated with the option.

```cpp
std::vector<int> val;
app.add_option("--opt",val,"description");
```

detects a type size of 1, since the underlying element type is a single string,
so the minimum number of strings is 1. But since it is a vector the expected
number can be very big. The default for a vector is (1<<30), and the
allow_extra_args is set to true. This means that at least 1 argument is expected
to follow the option, but arbitrary numbers of arguments may follow. These are
checked if they have the form of an option but if not they are added to the
argument.

```cpp
std::vector<std::tuple<int, double, std::string>> val;
app.add_option("--opt",val,"description");
```

gets into the complicated cases where the type size is now 3. and the expected
max is set to a large number and `allow_extra_args` is set to true. In this case
at least 3 arguments are required to follow the option, and subsequent groups
must come in groups of three, otherwise an error will result.

```cpp
bool val{false};
app.add_flag("--opt",val,"description");
```

Using the add_flag methods for creating options creates an option with an
expected size of 0, implying no arguments can be passed.

```cpp
std::complex<double> val;
app.add_option("--opt",val,"description");
```

triggers the complex number type which has a min of 1 and max of 2, so 1 or 2
strings can be passed. Complex number conversion supports arguments of the form
"1+2j" or "1","2", or "1" "2i". The imaginary number symbols `i` and `j` are
interchangeable in this context.

```cpp
std::vector<std::vector<int>> val;
app.add_option("--opt",val,"description");
```

has a type size of 1 to (1<<30).

### Customization

The `type_size(N)`, `type_size(Nmin, Nmax)`, `expected(N)`,
`expected(Nmin,Nmax)`, and `allow_extra_args()` can be used to customize an
option. For example

```cpp
std::string val;
auto opt=app.add_flag("--opt{vvv}",val,"description");
opt->expected(0,1);
```

will create a hybrid option, that can exist on its own in which case the value
"vvv" is used or if a value is given that value will be used.

There are some additional options that can be specified to modify an option for
specific cases:

- `->run_callback_for_default()` will specify that the callback should be
  executed when a default_val is set. This is set automatically when appropriate
  though it can be turned on or off and any user specified callback for an
  option will be executed when the default value for an option is set.

- `->force_callback()` will for the callback/value assignment to run at the
  conclusion of parsing regardless of whether the option was supplied or not.
  This can be used to force the default or execute some code.

- `->trigger_on_parse()` will trigger the callback or value assignment each time
  the argument is passed. The value is reset if the option is supplied multiple
  times.

## Unusual circumstances

There are a few cases where some things break down in the type system managing
options and definitions. Using the `add_option` method defines a lambda function
to extract a default value if required. In most cases this is either
straightforward or a failure is detected automatically and handled. But in a few
cases a streaming template is available that several layers down may not
actually be defined. This results in CLI11 not being able to detect this
circumstance automatically and will result in compile error. One specific known
case is `boost::optional` if the boost optional_io header is included. This
header defines a template for all boost optional values even if they do not
actually have a streaming operator. For example `boost::optional<std::vector>`
does not have a streaming operator but one is detected since it is part of a
template. For these cases a secondary method `app->add_option_no_stream(...)` is
provided that bypasses this operation completely and should compile in these
cases.
