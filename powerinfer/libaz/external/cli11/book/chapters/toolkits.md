# Using CLI11 in a Toolkit

CLI11 was designed to be integrate into a toolkit, providing a native experience
for users. This was used in GooFit to provide `GooFit::Application`, an class
designed to make ROOT users feel at home.

## Custom namespace

If you want to provide CLI11 in a custom namespace, you'll want to at least put
`using CLI::App` in your namespace. You can also include Option, some errors,
and validators. You can also put `using namespace CLI` inside your namespace to
import everything.

You may also want to make your own copy of the `CLI11_PARSE` macro. Something
like:

```cpp
 #define MYPACKAGE_PARSE(app, argv, argc)    \
     try {                                   \
         app.parse(argv, argc);              \
     } catch(const CLI::ParseError &e) {     \
         return app.exit(e);                 \
     }
```

## Subclassing App

If you subclass `App`, you'll just need to do a few things. You'll need a
constructor; calling the base `App` constructor is a good idea, but not
necessary (it just sets a description and adds a help flag.

You can call anything you would like to configure in the constructor, like
`option_defaults()->take_last()` or `fallthrough()`, and it will be set on all
user instances. You can add flags and options, as well.

## Virtual functions provided

You are given a few virtual functions that you can change (only on the main
App). `pre_callback` runs right before the callbacks run, letting you print out
custom messages at the top of your app.
