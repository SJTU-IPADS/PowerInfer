# Accepting configure files

## Reading a configure file

You can tell your app to allow configure files with `set_config("--config")`.
There are arguments: the first is the option name. If empty, it will clear the
config flag. The second item is the default file name. If that is specified, the
config will try to read that file. The third item is the help string, with a
reasonable default, and the final argument is a boolean (default: false) that
indicates that the configuration file is required and an error will be thrown if
the file is not found and this is set to true. The option pointer returned by
`set_config` is the same type as returned by `add_option` and all modifiers
including validators, and checks are valid.

### Adding a default path

if it is desired that config files be searched for a in a default path the
`CLI::FileOnDefaultPath` transform can be used.

```cpp
app.set_config("--config")->transform(CLI::FileOnDefaultPath("/default_path/"));
```

This will allow specified files to either exist as given or on a specified
default path.

```cpp
app.set_config("--config")
    ->transform(CLI::FileOnDefaultPath("/default_path/"))
    ->transform(CLI::FileOnDefaultPath("/default_path2/",false));
```

Multiple default paths can be specified through this mechanism. The last
transform given is executed first so the error return must be disabled so it can
be chained to the first. The same effect can be achieved though the or(`|`)
operation with validators

```cpp
app.set_config("--config")
    ->transform(CLI::FileOnDefaultPath("/default_path2/") | CLI::FileOnDefaultPath("/default_path/"));
```

### Extra fields

Sometimes configuration files are used for multiple purposes so CLI11 allows
options on how to deal with extra fields

```cpp
app.allow_config_extras(true);
```

will allow capture the extras in the extras field of the app. (NOTE: This also
sets the `allow_extras` in the app to true)

```cpp
app.allow_config_extras(false);
```

will generate an error if there are any extra fields for slightly finer control
there is a scoped enumeration of the modes or

```cpp
app.allow_config_extras(CLI::config_extras_mode::ignore);
```

will completely ignore extra parameters in the config file. This mode is the
default.

```cpp
app.allow_config_extras(CLI::config_extras_mode::capture);
```

will store the unrecognized options in the app extras fields. This option is the
closest equivalent to `app.allow_config_extras(true);` with the exception that
it does not also set the `allow_extras` flag so using this option without also
setting `allow_extras(true)` will generate an error which may or may not be the
desired behavior.

```cpp
app.allow_config_extras(CLI::config_extras_mode::error);
```

is equivalent to `app.allow_config_extras(false);`

```cpp
app.allow_config_extras(CLI::config_extras_mode::ignore_all);
```

will completely ignore any mismatches, extras, or other issues with the config
file

Config file extras are stored in the remaining output as two components. The
first is the name of the field including subcommands using dot notation the
second (or more) are the argument fields.

### Getting the used configuration file name

If it is needed to get the configuration file name used this can be obtained via
`app.get_config_ptr()->as<std::string>()` or
`app["--config"]->as<std::string>()` assuming `--config` was the configuration
option name.

### Order of precedence

By default if multiple configuration files are given they are read in reverse
order. With the last one given taking precedence over the earlier ones. This
behavior can be changed through the `multi_option_policy`. For example:

```cpp
app.set_config("--config")
    ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);
```

will read the files in the order given, which may be useful in some
circumstances. Using `CLI::MultiOptionPolicy::TakeLast` would work similarly
getting the last `N` files given. The default policy for config options is
`CLI::MultiOptionPolicy::Reverse` which takes the last expected `N` and reverses
them so the last option given is given precedence.

## Configure file format

Here is an example configuration file, in
[TOML](https://github.com/toml-lang/toml) format:

```toml
# Comments are supported, using a #
# The default section is [default], case-insensitive

value = 1
str = "A string"
vector = [1,2,3]

# Section map to subcommands
[subcommand]
in_subcommand = Wow
[subcommand.sub]
subcommand = true # could also be give as sub.subcommand=true
```

Spaces before and after the name and argument are ignored. Multiple arguments
are separated by spaces. One set of quotes will be removed, preserving spaces
(the same way the command line works). Boolean options can be `true`, `on`, `1`,
`y`, `t`, `+`, `yes`, `enable`; or `false`, `off`, `0`, `no`, `n`, `f`, `-`,
`disable`, (case-insensitive). Sections (and `.` separated names) are treated as
subcommands (note: this does not necessarily mean that subcommand was passed, it
just sets the "defaults". If a subcommand is set to `configurable` then passing
the subcommand using `[sub]` in a configuration file will trigger the
subcommand.)

CLI11 also supports configuration file in INI format.

```ini
; Comments are supported, using a ;
; The default section is [default], case-insensitive

value = 1
str = "A string"
vector = 1 2 3

; Section map to subcommands
[subcommand]
in_subcommand = Wow
sub.subcommand = true
```

The main differences are in vector notation and comment character. Note: CLI11
is not a full TOML parser as it just reads values as strings. It is possible
(but not recommended) to mix notation.

### Multi-line strings

The default config file parser supports multi-line strings like the toml
standard [TOML](https://toml.io/en/). It also supports multiline comments like
python doc strings.

```toml
"""
this is a multiline
comment
"""

""" this is also
a multiline comment"""

''' and so is
this
'''

value = 1
str = """
this is a multiline string value
the first \n is removed and so is the last
"""

str2 = ''' this is also a mu-
ltiline value '''

str3 = """\
    a line continuation \
    will skip \
    all white space between the '\' \
    and the next non-whitespace character \
    making this into a single line
"""

```

The key is that the closing of the multiline string must be at the end of a line
and match the starting 3 quote sequence. Multiline sequences using `"""` allow
escape sequences. Following [TOML](https://toml.io/en/v1.0.0#string) with the
addition of allowing '\0' for a null character, and binary Strings described in
the next section. This same formatting also applies to single line strings.
Multiline strings are not allowed as part of an array.

### Binary Strings

Config files have a binary conversion capability, this is mainly to support
writing config files but can be used by user generated files as well. Strings
with the form `B"(XXXXX)"` will convert any characters inside the parenthesis
with the form `\xHH` to the equivalent binary value. The HH are hexadecimal
characters. Characters not in this form will be translated as given. If argument
values with unprintable characters are used to generate a config file this
binary form will be used in the output string.

### vector of vector inputs

It is possible to specify vector of vector inputs in config file. This can be
done in a couple different ways

```toml
# Examples of vector of vector inputs in config

# this example is how config_to_str writes it out
vector1 = [1,2,3,"",4,5,6]

# alternative with vector separator sequence
vector2 = [1,2,3,"%%",4,5,6]

# multiline format
vector3 = [1,2,3]
vector3 = [4,5,6]

```

The `%%` is ignored in multiline format if the inject_separator modifier on the
option is set to false, thus for vector 3 if the option is storing to a single
vector all the elements will be in that vector.

For config file multiple sequential duplicate variable names are treated as if
they are a vector input, with possible separator insertion in the case of
multiple input vectors.

The config parser has a modifier

```C++
 app.get_config_formatter_base()->allowDuplicateFields();
```

This modification will insert the separator between each line even if not
sequential. This allows an input option to be configured with multiple lines.

```toml
# Examples of vector of vector inputs in config

# this example is how config_to_str writes it out
vector1 = [a,v,"[]"]
```

The field insertion has a special processing for duplicate characters starting
with "[[" in which case the `"[]"` gets translated to `[[]]` before getting
passed into the option which converts it back into the correct string. This can
also be used on the command line to handle unusual parsing situation with
brackets.

### Argument With Brackets

There is an edge case with actual strings that are surrounded by brackets. For
example if the string "[]" needed to be passed. this would normally trigger the
bracket processing and result in an empty vector. In this case it can be
enclosed in quotes and should be handled correctly.

## Multiple configuration files

If it is desired that multiple configuration be allowed. Use

```cpp
app.set_config("--config")->expected(1, X);
```

Where X is some positive integer and will allow up to `X` configuration files to
be specified by separate `--config` arguments.

## Writing out a configure file

To print a configuration file from the passed arguments, use
`.config_to_str(default_also=false, write_description=false)`, where
`default_also` will also show any defaulted arguments, and `write_description`
will include option descriptions and the App description

```cpp
 CLI::App app;
 app.add_option(...);
    // several other options
 CLI11_PARSE(app, argc, argv);
 //the config printout should be after the parse to capture the given arguments
 std::cout<<app.config_to_str(true,true);
```

if a prefix is needed to print before the options, for example to print a config
for just a subcommand, the config formatter can be obtained directly.

```cpp
  auto fmtr=app.get_config_formatter();
  //std::string to_config(const App *app, bool default_also, bool write_description, std::string prefix)
  fmtr->to_config(&app,true,true,"sub.");
  //prefix can be used to set a prefix before each argument,  like "sub."
```

### Customization of configure file output

The default config parser/generator has some customization points that allow
variations on the TOML format. The default formatter has a base configuration
that matches the TOML format. It defines 5 characters that define how different
aspects of the configuration are handled. You must use
`get_config_formatter_base()` to have access to these fields

```cpp
/// the character used for comments
char commentChar = '#';
/// the character used to start an array '\0' is a default to not use
char arrayStart = '[';
/// the character used to end an array '\0' is a default to not use
char arrayEnd = ']';
/// the character used to separate elements in an array
char arraySeparator = ',';
/// the character used separate the name from the value
char valueDelimiter = '=';
/// the character to use around strings
char stringQuote = '"';
/// the character to use around single characters and literal strings
char literalQuote = '\'';
/// the maximum number of layers to allow
uint8_t maximumLayers{255};
/// the separator used to separator parent layers
char parentSeparatorChar{'.'};
/// comment default values
bool commentDefaultsBool = false;
/// specify the config reader should collapse repeated field names to a single vector
bool allowMultipleDuplicateFields{false};
/// Specify the configuration index to use for arrayed sections
uint16_t configIndex{0};
/// Specify the configuration section that should be used
std::string configSection;
```

These can be modified via setter functions

- `ConfigBase *comment(char cchar)`: Specify the character to start a comment
  block
- `ConfigBase *arrayBounds(char aStart, char aEnd)`: Specify the start and end
  characters for an array
- `ConfigBase *arrayDelimiter(char aSep)`: Specify the delimiter character for
  an array
- `ConfigBase *valueSeparator(char vSep)`: Specify the delimiter between a name
  and value
- `ConfigBase *quoteCharacter(char qString, char literalChar)` :specify the
  characters to use around strings and single characters
- `ConfigBase *commentDefaults(bool comDef)` : set to true to comment lines with
  a default value
- `ConfigBase *allowDuplicateFields(bool value)` :set to true to allow duplicate
  fields to be merged even if not sequential
- `ConfigBase *maxLayers(uint8_t layers)` : specify the maximum number of parent
  layers to process. This is useful to limit processing for larger config files
- `ConfigBase *parentSeparator(char sep)` : specify the character to separate
  parent layers from options
- `ConfigBase *section(const std::string &sectionName)` : specify the section
  name to use to get the option values, only this section will be processed
- `ConfigBase *index(uint16_t sectionIndex)` : specify an index section to use
  for processing if multiple TOML sections of the same name are present
  `[[section]]`

For example, to specify reading a configure file that used `:` to separate name
and values:

```cpp
auto config_base=app.get_config_formatter_base();
config_base->valueSeparator(':');
```

The default configuration file will read INI files, but will write out files in
the TOML format. To specify outputting INI formatted files use

```cpp
app.config_formatter(std::make_shared<CLI::ConfigINI>());
```

which makes use of a predefined modification of the ConfigBase class which TOML
also uses. If a custom formatter is used that is not inheriting from the from
ConfigBase class `get_config_formatter_base() will return a nullptr if RTTI is
on (usually the default), or garbage if RTTI is off, so some care must be
exercised in its use with custom configurations.

## Custom formats

You can invent a custom format and set that instead of the default INI
formatter. You need to inherit from `CLI::Config` and implement the following
two functions:

```cpp
std::string to_config(const CLI::App *app, bool default_also, bool, std::string) const;
std::vector<CLI::ConfigItem> from_config(std::istream &input) const;
```

The `CLI::ConfigItem`s that you return are simple structures with a name, a
vector of parents, and a vector of results. A optionally customizable `to_flag`
method on the formatter lets you change what happens when a ConfigItem turns
into a flag.

Finally, set your new class as new config formatter:

```cpp
app.config_formatter(std::make_shared<NewConfig>());
```

See
[`examples/json.cpp`](https://github.com/CLIUtils/CLI11/blob/main/examples/json.cpp)
for a complete JSON config example.

### Trivial JSON configuration example

```JSON
{
   "test": 56,
   "testb": "test",
   "flag": true
}
```

The parser can handle these structures with only a minor tweak

```cpp
app.get_config_formatter_base()->valueSeparator(':');
```

The open and close brackets must be on a separate line and the comma gets
interpreted as an array separator but since no values are after the comma they
get ignored as well. This will not support multiple layers or sections or any
other moderately complex JSON, but can work if the input file is simple.

## Triggering Subcommands

Configuration files can be used to trigger subcommands if a subcommand is set to
configure. By default configuration file just set the default values of a
subcommand. But if the `configure()` option is set on a subcommand then the if
the subcommand is utilized via a `[subname]` block in the configuration file it
will act as if it were called from the command line. Subsubcommands can be
triggered via `[subname.subsubname]`. Using the `[[subname]]` will be as if the
subcommand were triggered multiple times from the command line. This
functionality can allow the configuration file to act as a scripting file.

For custom configuration files this behavior can be triggered by specifying the
parent subcommands in the structure and `++` as the name to open a new
subcommand scope and `--` to close it. These names trigger the different
callbacks of configurable subcommands.

## Stream parsing

In addition to the regular parse functions a
`parse_from_stream(std::istream &input)` is available to directly parse a stream
operator. For example to process some arguments in an already open file stream.
The stream is fed directly in the config parser so bypasses the normal command
line parsing.

## Implementation Notes

The config file input works with any form of the option given: Long, short,
positional, or the environment variable name. When generating a config file it
will create an option name in following priority.

1. First long name
2. First short name
3. Positional name
4. Environment name

In config files the name will be enclosed in quotes if there is any potential
ambiguities in parsing the name.
