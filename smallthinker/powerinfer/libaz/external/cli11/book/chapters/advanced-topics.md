# Advanced topics

## Environment variables

Environment variables can be used to fill in the value of an option:

```cpp
std::string opt;
app.add_option("--my_option", opt)->envname("MY_OPTION");
```

If not given on the command line, the environment variable will be checked and
read from if it exists. All the standard tools, like default and required, work
as expected. If passed on the command line, this will ignore the environment
variable.

## Needs/excludes

You can set a network of requirements. For example, if flag a needs flag b but
cannot be given with flag c, that would be:

```cpp
auto a = app.add_flag("-a");
auto b = app.add_flag("-b");
auto c = app.add_flag("-c");

a->needs(b);
a->excludes(c);
```

CLI11 will make sure your network of requirements makes sense, and will throw an
error immediately if it does not.

## Custom option callbacks

You can make a completely generic option with a custom callback. For example, if
you wanted to add a complex number (already exists, so please don't actually do
this):

```cpp
CLI::Option *
add_option(CLI::App &app, std::string name, cx &variable, std::string description = "", bool defaulted = false) {
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        double x, y;
        bool worked = CLI::detail::lexical_cast(res[0], x) && CLI::detail::lexical_cast(res[1], y);
        if(worked)
            variable = cx(x, y);
        return worked;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->set_custom_option("COMPLEX", 2);
    if(defaulted) {
        std::stringstream out;
        out << variable;
        opt->set_default_str(out.str());
    }
    return opt;
}
```

Then you could use it like this:

```cpp
std::complex<double> comp{0, 0};
add_option(app, "-c,--complex", comp);
```

## Custom converters

You can add your own converters to allow CLI11 to accept more option types in
the standard calls. These can only be used for "single" size options (so
complex, vector, etc. are a separate topic). If you set up a custom
`istringstream& operator <<` overload before include CLI11, you can support
different conversions. If you place this in the CLI namespace, you can even keep
this from affecting the rest of your code. Here's how you could add
`boost::optional` for a compiler that does not have `__has_include`:

```cpp
// CLI11 already does this if __has_include is defined
#ifndef __has_include

#include <boost/optional.hpp>

// Use CLI namespace to avoid the conversion leaking into your other code
namespace CLI {

template <typename T> std::istringstream &operator>>(std::istringstream &in, boost::optional<T> &val) {
    T v;
    in >> v;
    val = v;
    return in;
}

}

#endif

#include <CLI11.hpp>
```

This is an example of how to use the system only; if you are just looking for a
way to activate `boost::optional` support on older compilers, you should define
`CLI11_BOOST_OPTIONAL` before including a CLI11 file, you'll get the
`boost::optional` support.

## Custom converters and type names: std::chrono example

An example of adding a custom converter and typename for `std::chrono` follows:

```cpp
namespace CLI
{
    template <typename T, typename R>
    std::istringstream &operator>>(std::istringstream &in, std::chrono::duration<T,R> &val)
    {
        T v;
        in >> v;
        val = std::chrono::duration<T,R>(v);
        return in;
    }

    template <typename T, typename R>
    std::stringstream &operator<<(std::stringstream &in, std::chrono::duration<T,R> &val)
    {
        in << val.count();
        return in;
    }
 }

#include <CLI/CLI.hpp>

namespace CLI
{
    namespace detail
    {
        template <>
        constexpr const char *type_name<std::chrono::hours>()
        {
            return "TIME [H]";
        }

        template <>
        constexpr const char *type_name<std::chrono::minutes>()
        {
            return "TIME [MIN]";
        }
        }
}
```

Thanks to Olivier Hartmann for the example.
