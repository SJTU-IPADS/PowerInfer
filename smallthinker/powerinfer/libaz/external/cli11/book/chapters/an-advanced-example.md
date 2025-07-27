# Making a git clone

Let's try our hand at a little `git` clone, called `geet`. It will just print
it's intent, rather than running actual code, since it's just a demonstration.
Let's start by adding an app and requiring 1 subcommand to run:

[include:"Intro"](../code/geet.cpp)

Now, let's define the first subcommand, `add`, along with a few options:

[include:"Add"](../code/geet.cpp)

Now, let's add `commit`:

[include:"Commit"](../code/geet.cpp)

All that's need now is the parse call. We'll print a little message after the
code runs, and then return:

[include:"Parse"](../code/geet.cpp)

[Source code](https://github.com/CLIUtils/CLI11/tree/main/book/code/geet.cpp)

If you compile and run:

```term
gitbook:examples $ c++ -std=c++11 geet.cpp -o geet
```

You'll see it behaves pretty much like `git`.

## Multi-file App parse code

This example could be made much nicer if it was split into files, one per
subcommand. If you simply use shared pointers instead of raw values in the
lambda capture, you can tie the lifetime to the lambda function lifetime. CLI11
has a
[multifile example](https://github.com/CLIUtils/CLI11/tree/main/examples/subcom_in_files)
in its example folder.
