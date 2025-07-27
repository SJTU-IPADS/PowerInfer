# Formatting help output

{% hint style='info' %} New in CLI11 1.6 {% endhint %}

## Customizing an existing formatter

In CLI11, you can control the output of the help printout in full or in part.
The default formatter was written in such a way as to be customizable. You can
use `app.get_formatter()` to get the current formatter. The formatter you set
will be inherited by subcommands that are created after you set the formatter.

There are several configuration options that you can set:

| Set method            | Description                      | Availability |
| --------------------- | -------------------------------- | ------------ |
| `column_width(width)` | The width of the columns         | Both         |
| `label(key, value)`   | Set a label to a different value | Both         |

Labels will map the built in names and type names from key to value if present.
For example, if you wanted to change the width of the columns to 40 and the
`REQUIRED` label from `(REQUIRED)` to `(MUST HAVE)`:

```cpp
app.get_formatter()->column_width(40);
app.get_formatter()->label("REQUIRED", "(MUST HAVE)");
```

## Subclassing

You can further configure pieces of the code while still keeping most of the
formatting intact by subclassing either formatter and replacing any of the
methods with your own. The formatters use virtual functions most places, so you
are free to add or change anything about them. For example, if you wanted to
remove the info that shows up between the option name and the description:

```cpp
class MyFormatter : public CLI::Formatter {
  public:
    std::string make_option_opts(const CLI::Option *) const override {return "";}
};
app.formatter(std::make_shared<MyFormatter>());
```

Look at the class definitions in `FormatterFwd.hpp` or the method definitions in
`Formatter.hpp` to see what methods you have access to and how they are put
together.

## Anatomy of a help message

This is a normal printout, with `<>` indicating the methods used to produce each
line.

```text
<make_description(app)>
<make_usage(app)>
<make_positionals(app)>
  <make_group(app, "Positionals", true, filter>
<make_groups(app, mode)>
  <make_group(app, "Option Group 1"), false, filter>
  <make_group(app, "Option Group 2"), false, filter>
  ...
<make_subcommands(app)>
  <make_subcommand(sub1, Mode::Normal)>
  <make_subcommand(sub2, Mode::Normal)>
<make_footer(app)>
```

`make_usage` calls `make_option_usage(opt)` on all the positionals to build that
part of the line. `make_subcommand` passes the subcommand as the app pointer.

The `make_groups` print the group name then call `make_option(o)` on the options
listed in that group. The normal printout for an option looks like this:

```text
        make_option_opts(o)
            ┌───┴────┐
 -n,--name  (REQUIRED)      This is a description
└────┬────┘                └──────────┬──────────┘
make_option_name(o,p)        make_option_desc(o)
```

Notes:

- `*1`: This signature depends on whether the call is from a positional or
  optional.
- `o` is opt pointer, `p` is true if positional.
