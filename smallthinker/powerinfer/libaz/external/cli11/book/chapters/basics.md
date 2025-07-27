# The Basics

The simplest CLI11 program looks like this:

[include](../code/simplest.cpp)

The first line includes the library; this explicitly uses the single file
edition (see [Selecting an edition](/chapters/installation)).

After entering the main function, you'll see that a `CLI::App` object is
created. This is the basis for all interactions with the library. You could
optionally provide a description for your app here.

A normal CLI11 application would define some flags and options next. This is a
simplest possible example, so we'll go on.

The macro `CLI11_PARSE` just runs five simple lines. This internally runs
`app.parse(argc, argv)`, which takes the command line info from C++ and parses
it. If there is an error, it throws a `ParseError`; if you catch it, you can use
`app.exit` with the error as an argument to print a nice message and produce the
correct return code for your application.

If you just use `app.parse` directly, your application will still work, but the
stack will not be correctly unwound since you have an uncaught exception, and
the command line output will be cluttered, especially for help.

For this (and most of the examples in this book) we will assume that we have the
`CLI11.hpp` file in the current directory and that we are creating an output
executable `a.out` on a macOS or Linux system. The commands to compile and test
this example would be:

```term
gitbook:examples $ g++ -std=c++11 simplest.cpp
gitbook:examples $ ./a.out -h
Usage: ./a.out [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
```
