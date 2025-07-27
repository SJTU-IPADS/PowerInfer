# CLI11 Internals

## Callbacks

The library was designed to bind to existing variables without requiring typed
classes or inheritance. This is accomplished through lambda functions.

This looks like:

```cpp
Option* add_option(string name, T &item) {
    this->function = [&item](string value){
        return lexical_cast(value, item);
    }
}
```

Obviously, you can't access `T` after the `add_` method is over, so it stores
the string representation of the default value if it receives the special `true`
value as the final argument (not shown above).

## Parsing

Parsing follows the following procedure:

1. `_validate`: Make sure the defined options and subcommands are self
   consistent.
2. `_parse`: Main parsing routine. See below.
3. `_run_callback`: Run an App callback if present.

The parsing phase is the most interesting:

1. `_parse_single`: Run on each entry on the command line and fill the
   options/subcommands.
2. `_process`: Run the procedure listed below.
3. `_process_extra`: This throws an error if needed on extra arguments that
   didn't fit in the parse.

The `_process` procedure runs the following steps; each step is recursive and
completes all subcommands before moving to the next step (new in 1.7). This
ensures that interactions between options and subcommand options is consistent.

1. `_process_ini`: This reads an INI file and fills/changes options as needed.
2. `_process_env`: Look for environment variables.
3. `_process_callbacks`: Run the callback functions - this fills out the
   variables.
4. `_process_help_flags`: Run help flags if present (and quit).
5. `_process_requirements`: Make sure needs/excludes, required number of options
   present.

## Exceptions

The library immediately returns a C++ exception when it detects a problem, such
as an incorrect construction or a malformed command line.
