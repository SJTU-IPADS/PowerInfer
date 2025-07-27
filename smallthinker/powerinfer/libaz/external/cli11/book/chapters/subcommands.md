# Subcommands and the App

Subcommands are keyword that invoke a new set of options and features. For
example, the `git` command has a long series of subcommands, like `add` and
`commit`. Each can have its own options and implementations. This chapter will
focus on implementations that are contained in the same C++ application, though
the system git uses to extend the main command by calling other commands in
separate executables is supported too; that's called "Prefix commands" and is
included at the end of this chapter.

## The parent App

We'll start by discussing the parent `App`. You've already used it quite a bit,
to create options and set option defaults. There are several other things you
can do with an `App`, however.

You are given a lot of control the help output. You can set a footer with
`app.footer("My Footer")`. You can replace the default help print when a
`ParseError` is thrown with `app.failure_message(CLI::FailureMessage::help)`.
The default is `CLI:::FailureMessage::simple`, and you can easily define a new
one. Just make a (lambda) function that takes an App pointer and a reference to
an error code (even if you don't use them), and returns a string.

## Adding a subcommand

Subcommands can be added just like an option:

```cpp
CLI::App* sub = app.add_subcommand("sub", "This is a subcommand");
```

The subcommand should have a name as the first argument, and a little
description for the second argument. A pointer to the internally stored
subcommand is provided; you usually will be capturing that pointer and using it
later (though you can use callbacks if you prefer). As always, feel free to use
`auto sub = ...` instead of naming the type.

You can check to see if the subcommand was received on the command line several
ways:

```cpp
if(*sub) ...
if(sub->parsed()) ...
if(app.got_subcommand(sub)) ...
if(app.got_subcommand("sub")) ...
```

You can also get a list of subcommands with `get_subcommands()`, and they will
be in parsing order.

There are a lot of options that you can set on a subcommand; in fact,
subcommands have exactly the same options as your main app, since they are
actually the same class of object (as you may have guessed from the type above).
This has the pleasant side affect of making subcommands infinitely nestable.

## Required subcommands

Each App has controls to set the number of subcommands you expect. This is
controlled by:

```cpp
app.require_subcommand(/* min */ 0, /* max */ 1);
```

If you set the max to 0, CLI11 will allow an unlimited number of subcommands.
After the (non-unlimited) maximum is reached, CLI11 will stop trying to match
subcommands. So the if you pass "`one two`" to a command, and both `one` and
`two` are subcommands, it will depend on the maximum number as to whether the
"`two`" is a subcommand or an argument to the "`one`" subcommand.

As a shortcut, you can also call the `require_subcommand` method with one
argument; that will be the fixed number of subcommands if positive, it will be
the maximum number if negative. Calling it without an argument will set the
required subcommands to 1 or more.

The maximum number of subcommands is inherited by subcommands. This allows you
to set the maximum to 1 once at the beginning on the parent app if you only want
single subcommands throughout your app. You should keep this in mind, if you are
dealing with lots of nested subcommands.

## Using callbacks

You've already seen how to check to see what subcommands were given. It's often
much easier, however, to just define the code you want to run when you are
making your parser, and not run a bunch of code after `CLI11_PARSE` to analyse
the state (Procedural! Yuck!). You can do that with lambda functions. A
`std::function<void()>` callback `.callback()` is provided, and CLI11 ensures
that all options are prepared and usable by reference capture before entering
the callback. An example is shown below in the `geet` program.

## Inheritance of defaults

The following values are inherited when you add a new subcommand. This happens
at the point the subcommand is created:

- The name and description for the help flag
- The footer
- The failure message printer function
- Option defaults
- Allow extras
- Prefix command
- Ignore case
- Ignore underscore
- Allow Windows style options
- Fallthrough
- Group name
- Max required subcommands
- validate positional arguments
- validate optional arguments

## Special modes

There are several special modes for Apps and Subcommands.

### Allow extras

Normally CLI11 throws an error if you don't match all items given on the command
line. However, you can enable `allow_extras()` to instead store the extra values
in `.remaining()`. You can get all remaining options including those in
contained subcommands recursively in the original order with `.remaining(true)`.
`.remaining_size()` is also provided; this counts the size but ignores the `--`
special separator if present.

### Fallthrough

Fallthrough allows an option that does not match in a subcommand to "fall
through" to the parent command; if that parent allows that option, it matches
there instead. This was added to allow CLI11 to represent models:

```term
gitbook:code $ ./my_program my_model_1 --model_flag --shared_flag
```

Here, `--shared_flag` was set on the main app, and on the command line it "falls
through" `my_model_1` to match on the main app. This is set through
`->fallthrough()` on a subcommand.

#### Subcommand fallthrough

Subcommand fallthrough allows additional subcommands to be triggered after the
first subcommand. By default subcommand fallthrough is enabled, but it can be
turned off through `->subcommand_fallthrough(false)` on a subcommand. This will
prevent additional subcommands at the same inheritance level from triggering,
the strings would then be treated as positional values. As a technical note if
fallthrough is enabled but subcommand fallthrough disabled (this is not the
default in both cases), then subcommands on grandparents can still be triggered
from the grandchild subcommand, unless subcommand fallthrough is also disabled
on the parent. This is an unusual circumstance but may arise in some very
particular situations.

### Prefix command

This is a special mode that allows "prefix" commands, where the parsing
completely stops when it gets to an unknown option. Further unknown options are
ignored, even if they could match. Git is the traditional example for prefix
commands; if you run git with an unknown subcommand, like "`git thing`", it then
calls another command called "`git-thing`" with the remaining options intact.

### Silent subcommands

Subcommands can be modified by using the `silent` option. This will prevent the
subcommand from showing up in the get_subcommands list. This can be used to make
subcommands into modifiers. For example, a help subcommand might look like

```c++
    auto sub1 = app.add_subcommand("help")->silent();
    sub1->parse_complete_callback([]() { throw CLI::CallForHelp(); });
```

This would allow calling help such as:

```bash
./app help
./app help sub1
```

### Positional Validation

Some arguments supplied on the command line may be legitimately applied to more
than 1 positional argument. In this context enabling `positional_validation` on
the application or subcommand will check any validators before applying the
command line argument to the positional option. It is not an error to fail
validation in this context, positional arguments not matching any validators
will go into the `extra_args` field which may generate an error depending on
settings.

### Optional Argument Validation

Similar to positional validation, there are occasional contexts in which case it
might be ambiguous whether an argument should be applied to an option or a
positional option.

```c++
    std::vector<std::string> vec;
    std::vector<int> ivec;
    app.add_option("pos", vec);
    app.add_option("--args", ivec)->check(CLI::Number);
    app.validate_optional_arguments();
```

In this case a sequence of integers is expected for the argument and remaining
strings go to the positional string vector. Without the
`validate_optional_arguments()` active it would be impossible get any later
arguments into the positional if the `--args` option is used. The validator in
this context is used to make sure the optional arguments match with what the
argument is expecting and if not the `-args` option is closed, and remaining
arguments fall into the positional.
