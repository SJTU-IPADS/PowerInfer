// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <complex>
#include <cstdint>
#include <string>

using cx = std::complex<double>;

CLI::Option *
add_option(CLI::App &app, std::string name, cx &variable, std::string description = "", bool defaulted = false) {
    CLI::callback_t fun = [&variable](CLI::results_t res) {
        double x = 0, y = 0;
        bool worked = CLI::detail::lexical_cast(res[0], x) && CLI::detail::lexical_cast(res[1], y);
        if(worked)
            variable = cx(x, y);
        return worked;
    };

    CLI::Option *opt = app.add_option(name, fun, description, defaulted);
    opt->type_name("COMPLEX")->type_size(2);
    if(defaulted) {
        std::stringstream out;
        out << variable;
        opt->default_str(out.str());
    }
    return opt;
}

TEST_CASE_METHOD(TApp, "AddingComplexParser", "[complex]") {

    cx comp{0, 0};
    add_option(app, "-c,--complex", comp);
    args = {"-c", "1.5", "2.5"};

    run();

    CHECK(comp.real() == Approx(1.5));
    CHECK(comp.imag() == Approx(2.5));
}

TEST_CASE_METHOD(TApp, "DefaultedComplex", "[complex]") {

    cx comp{1, 2};
    add_option(app, "-c,--complex", comp, "", true);
    args = {"-c", "4", "3"};

    std::string help = app.help();
    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, Contains("2"));

    CHECK(comp.real() == Approx(1));
    CHECK(comp.imag() == Approx(2));

    run();

    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(3));
}

// an example of custom complex number converter that can be used to add new parsing options
#if defined(__has_include)
#if __has_include(<regex>)
// an example of custom converter that can be used to add new parsing options
#define HAS_REGEX_INCLUDE
#endif
#endif

#ifdef HAS_REGEX_INCLUDE
// Gcc 4.8 and older and the corresponding standard libraries have a broken <regex> so this would
// fail.  And if a clang compiler is using libstd++ then this will generate an error as well so this is just a check to
// simplify compilation and prevent a much more complicated #if expression
#include <regex>
namespace CLI {
namespace detail {

// On MSVC and possibly some other new compilers this can be a free standing function without the template
// specialization but this is compiler dependent
template <> bool lexical_cast<std::complex<double>>(const std::string &input, std::complex<double> &output) {
    // regular expression to handle complex numbers of various formats
    static const std::regex creg(
        R"(([+-]?(\d+(\.\d+)?|\.\d+)([eE][+-]?\d+)?)\s*([+-]\s*(\d+(\.\d+)?|\.\d+)([eE][+-]?\d+)?)[ji]*)");

    std::smatch m;
    double x{0.0}, y{0.0};
    bool worked = false;
    std::regex_search(input, m, creg);
    if(m.size() == 9) {
        worked = CLI::detail::lexical_cast(m[1], x) && CLI::detail::lexical_cast(m[6], y);
        if(worked) {
            if(*m[5].first == '-') {
                y = -y;
            }
        }
    } else {
        if((input.back() == 'j') || (input.back() == 'i')) {
            auto strval = input.substr(0, input.size() - 1);
            CLI::detail::trim(strval);
            worked = CLI::detail::lexical_cast(strval, y);
        } else {
            std::string ival = input;
            CLI::detail::trim(ival);
            worked = CLI::detail::lexical_cast(ival, x);
        }
    }
    if(worked) {
        output = cx{x, y};
    }
    return worked;
}
}  // namespace detail
}  // namespace CLI

TEST_CASE_METHOD(TApp, "AddingComplexParserDetail", "[complex]") {

    bool skip_tests = false;
    try {  // check if the library actually supports regex,  it is possible to link against a non working regex in the
           // standard library
        std::smatch m;
        std::string input = "1.5+2.5j";
        static const std::regex creg(
            R"(([+-]?(\d+(\.\d+)?|\.\d+)([eE][+-]?\d+)?)\s*([+-]\s*(\d+(\.\d+)?|\.\d+)([eE][+-]?\d+)?)[ji]*)");

        auto rsearch = std::regex_search(input, m, creg);
        if(!rsearch) {
            skip_tests = true;
        } else {
            CHECK(9u == m.size());
        }

    } catch(...) {
        skip_tests = true;
    }
    static_assert(CLI::detail::is_complex<cx>::value, "complex should register as complex in this situation");
    if(!skip_tests) {
        cx comp{0, 0};

        app.add_option("-c,--complex", comp, "add a complex number option");
        args = {"-c", "1.5+2.5j"};

        run();

        CHECK(comp.real() == Approx(1.5));
        CHECK(comp.imag() == Approx(2.5));
        args = {"-c", "1.5-2.5j"};

        run();

        CHECK(comp.real() == Approx(1.5));
        CHECK(comp.imag() == Approx(-2.5));
    }
}
#endif
// defining a new complex class
class complex_new {
  public:
    complex_new() = default;
    complex_new(double v1, double v2) : val1_{v1}, val2_{v2} {};
    CLI11_NODISCARD double real() const { return val1_; }
    CLI11_NODISCARD double imag() const { return val2_; }

  private:
    double val1_{0.0};
    double val2_{0.0};
};

TEST_CASE_METHOD(TApp, "newComplex", "[complex]") {
    complex_new cval;
    static_assert(CLI::detail::is_complex<complex_new>::value, "complex new does not register as a complex type");
    static_assert(CLI::detail::classify_object<complex_new>::value == CLI::detail::object_category::complex_number,
                  "complex new does not result in complex number categorization");
    app.add_option("-c,--complex", cval, "add a complex number option");
    args = {"-c", "1.5+2.5j"};

    run();

    CHECK(cval.real() == Approx(1.5));
    CHECK(cval.imag() == Approx(2.5));
    args = {"-c", "1.5-2.5j"};

    run();

    CHECK(cval.real() == Approx(1.5));
    CHECK(cval.imag() == Approx(-2.5));
}
