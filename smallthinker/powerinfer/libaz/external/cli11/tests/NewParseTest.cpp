// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"

#include <complex>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using cx = std::complex<double>;

TEST_CASE_METHOD(TApp, "ComplexOption", "[newparse]") {
    cx comp{1, 2};
    app.add_option("-c,--complex", comp)->capture_default_str();

    args = {"-c", "4", "3"};

    std::string help = app.help();
    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, Contains("2"));
    CHECK_THAT(help, Contains("COMPLEX"));

    CHECK(comp.real() == Approx(1));
    CHECK(comp.imag() == Approx(2));

    run();

    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(3));
}

TEST_CASE_METHOD(TApp, "ComplexFloatOption", "[newparse]") {
    std::complex<float> comp{1, 2};
    app.add_option("-c,--complex", comp)->capture_default_str();

    args = {"-c", "4", "3"};

    std::string help = app.help();
    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, Contains("2"));
    CHECK_THAT(help, Contains("COMPLEX"));

    CHECK(comp.real() == Approx(1));
    CHECK(comp.imag() == Approx(2));

    run();

    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(3));
}

TEST_CASE_METHOD(TApp, "ComplexWithDelimiterOption", "[newparse]") {
    cx comp{1, 2};
    app.add_option("-c,--complex", comp)->capture_default_str()->delimiter('+');

    args = {"-c", "4+3i"};

    std::string help = app.help();
    CHECK_THAT(help, Contains("1"));
    CHECK_THAT(help, Contains("2"));
    CHECK_THAT(help, Contains("COMPLEX"));

    CHECK(comp.real() == Approx(1));
    CHECK(comp.imag() == Approx(2));

    run();

    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(3));

    args = {"-c", "5+-3i"};
    run();

    CHECK(comp.real() == Approx(5));
    CHECK(comp.imag() == Approx(-3));

    args = {"-c", "6", "-4i"};
    run();

    CHECK(comp.real() == Approx(6));
    CHECK(comp.imag() == Approx(-4));
}

TEST_CASE_METHOD(TApp, "ComplexIgnoreIOption", "[newparse]") {
    cx comp{1, 2};
    app.add_option("-c,--complex", comp);

    args = {"-c", "4", "3i"};

    run();

    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(3));
}

TEST_CASE_METHOD(TApp, "ComplexSingleArgOption", "[newparse]") {
    cx comp{1, 2};
    app.add_option("-c,--complex", comp);

    args = {"-c", "4"};
    run();
    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(0));

    args = {"-c", "4-2i"};
    run();
    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(-2));
    args = {"-c", "4+2i"};
    run();
    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(2));

    args = {"-c", "-4+2j"};
    run();
    CHECK(comp.real() == Approx(-4));
    CHECK(comp.imag() == Approx(2));

    args = {"-c", "-4.2-2j"};
    run();
    CHECK(comp.real() == Approx(-4.2));
    CHECK(comp.imag() == Approx(-2));

    args = {"-c", "-4.2-2.7i"};
    run();
    CHECK(comp.real() == Approx(-4.2));
    CHECK(comp.imag() == Approx(-2.7));
}

TEST_CASE_METHOD(TApp, "ComplexSingleImagOption", "[newparse]") {
    cx comp{1, 2};
    app.add_option("-c,--complex", comp);

    args = {"-c", "4j"};
    run();
    CHECK(comp.real() == Approx(0));
    CHECK(comp.imag() == Approx(4));

    args = {"-c", "-4j"};
    run();
    CHECK(comp.real() == Approx(0));
    CHECK(comp.imag() == Approx(-4));
    args = {"-c", "-4"};
    run();
    CHECK(comp.real() == Approx(-4));
    CHECK(comp.imag() == Approx(0));
    args = {"-c", "+4"};
    run();
    CHECK(comp.real() == Approx(4));
    CHECK(comp.imag() == Approx(0));
}

/// Simple class containing two strings useful for testing lexical cast and conversions
class spair {
  public:
    spair() = default;
    spair(std::string s1, std::string s2) : first(std::move(s1)), second(std::move(s2)) {}
    std::string first{};
    std::string second{};
};

// Example of a custom converter that can be used to add new parsing options.
// It will be found via argument-dependent lookup, so should be in the same namespace as the `spair` type.
bool lexical_cast(const std::string &input, spair &output) {
    auto sep = input.find_first_of(':');
    if((sep == std::string::npos) && (sep > 0)) {
        return false;
    }
    output = {input.substr(0, sep), input.substr(sep + 1)};
    return true;
}

TEST_CASE_METHOD(TApp, "custom_string_converter", "[newparse]") {
    spair val;
    app.add_option("-d,--dual_string", val);

    args = {"-d", "string1:string2"};

    run();
    CHECK("string1" == val.first);
    CHECK("string2" == val.second);
}

TEST_CASE_METHOD(TApp, "custom_string_converterFail", "[newparse]") {
    spair val;
    app.add_option("-d,--dual_string", val);

    args = {"-d", "string2"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

/// Wrapper with an inconvenient interface
template <class T> class badlywrapped {
  public:
    badlywrapped() : value() {}

    CLI11_NODISCARD T get() const { return value; }

    void set(T val) { value = val; }

  private:
    T value;
};

// Example of a custom converter for a template type.
// It will be found via argument-dependent lookup, so should be in the same namespace as the `badlywrapped` type.
template <class T> bool lexical_cast(const std::string &input, badlywrapped<T> &output) {
    // This using declaration lets us use an unqualified call to lexical_cast below. This is important because
    // unqualified call finds the proper overload via argument-dependent lookup, and thus it will be able to find
    // an overload for `spair` type, which is not in `CLI::detail`.
    using CLI::detail::lexical_cast;

    T value;
    if(!lexical_cast(input, value))
        return false;
    output.set(value);
    return true;
}

TEST_CASE_METHOD(TApp, "custom_string_converter_flag", "[newparse]") {
    badlywrapped<bool> val;
    std::vector<badlywrapped<bool>> vals;
    app.add_flag("-1", val);
    app.add_flag("-2", vals);

    val.set(false);
    args = {"-1"};
    run();
    CHECK(true == val.get());

    args = {"-2", "-2"};
    run();
    CHECK(2 == vals.size());
    CHECK(true == vals[0].get());
    CHECK(true == vals[1].get());
}

TEST_CASE_METHOD(TApp, "custom_string_converter_adl", "[newparse]") {
    // This test checks that the lexical_cast calls route as expected.
    badlywrapped<spair> val;

    app.add_option("-d,--dual_string", val);

    args = {"-d", "string1:string2"};

    run();
    CHECK("string1" == val.get().first);
    CHECK("string2" == val.get().second);
}

/// Another wrapper to test that specializing CLI::detail::lexical_cast works
struct anotherstring {
    anotherstring() = default;
    std::string s{};
};

// This is a custom converter done via specializing the CLI::detail::lexical_cast template. This was the recommended
// mechanism for extending the library before, so we need to test it. Don't do this in your code, use
// argument-dependent lookup as outlined in the examples for spair and template badlywrapped.
namespace CLI {
namespace detail {
template <> bool lexical_cast<anotherstring>(const std::string &input, anotherstring &output) {
    bool result = lexical_cast(input, output.s);
    if(result)
        output.s += "!";
    return result;
}
}  // namespace detail
}  // namespace CLI

TEST_CASE_METHOD(TApp, "custom_string_converter_specialize", "[newparse]") {
    anotherstring s;

    app.add_option("-s", s);

    args = {"-s", "something"};

    run();
    CHECK("something!" == s.s);
}

/// Yet another wrapper to test that overloading lexical_cast with enable_if works.
struct yetanotherstring {
    yetanotherstring() = default;
    std::string s{};
};

template <class T> struct is_my_lexical_cast_enabled : std::false_type {};

template <> struct is_my_lexical_cast_enabled<yetanotherstring> : std::true_type {};

template <class T, CLI::enable_if_t<is_my_lexical_cast_enabled<T>::value, CLI::detail::enabler> = CLI::detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    output.s = input;
    return true;
}

TEST_CASE_METHOD(TApp, "custom_string_converter_adl_enable_if", "[newparse]") {
    yetanotherstring s;

    app.add_option("-s", s);

    args = {"-s", "something"};

    run();
    CHECK("something" == s.s);
}

/// simple class to wrap another  with a very specific type constructor and assignment operators to test out some of the
/// option assignments
template <class X> class objWrapper {
  public:
    objWrapper() = default;
    explicit objWrapper(X obj) : val_{std::move(obj)} {};
    objWrapper(const objWrapper &ow) = default;
    template <class TT> objWrapper(const TT &obj) = delete;
    objWrapper &operator=(const objWrapper &) = default;
    // noexcept not allowed below by GCC 4.8
    objWrapper &operator=(objWrapper &&) = default;  // NOLINT(performance-noexcept-move-constructor)
    // delete all other assignment operators
    template <typename TT> void operator=(TT &&obj) = delete;

    CLI11_NODISCARD const X &value() const { return val_; }

  private:
    X val_{};
};

/// simple class to wrap another  with a very specific type constructor and assignment operators to test out some of the
/// option assignments
template <class X> class objWrapperRestricted {
  public:
    objWrapperRestricted() = default;
    explicit objWrapperRestricted(int val) : val_{val} {};
    objWrapperRestricted(const objWrapperRestricted &) = delete;
    objWrapperRestricted(objWrapperRestricted &&) = delete;
    objWrapperRestricted &operator=(const objWrapperRestricted &) = delete;
    objWrapperRestricted &operator=(objWrapperRestricted &&) = delete;

    objWrapperRestricted &operator=(int val) {
        val_ = val;
        return *this;
    }
    CLI11_NODISCARD const X &value() const { return val_; }

  private:
    X val_{};
};

// I think there is a bug with the is_assignable in visual studio 2015 it is fixed in later versions
// so this test will not compile in that compiler
#if !defined(_MSC_VER) || _MSC_VER >= 1910

static_assert(CLI::detail::is_direct_constructible<objWrapper<std::string>, std::string>::value,
              "string wrapper isn't properly constructible");

static_assert(!std::is_assignable<objWrapper<std::string>, std::string>::value,
              "string wrapper is improperly assignable");
TEST_CASE_METHOD(TApp, "stringWrapper", "[newparse]") {
    objWrapper<std::string> sWrapper;
    app.add_option("-v", sWrapper);
    args = {"-v", "string test"};

    run();

    CHECK("string test" == sWrapper.value());
}

static_assert(CLI::detail::is_direct_constructible<objWrapper<double>, double>::value,
              "double wrapper isn't properly assignable");

static_assert(!CLI::detail::is_direct_constructible<objWrapper<double>, int>::value,
              "double wrapper can be assigned from int");

static_assert(!CLI::detail::is_istreamable<objWrapper<double>>::value,
              "double wrapper is input streamable and it shouldn't be");

TEST_CASE_METHOD(TApp, "doubleWrapper", "[newparse]") {
    objWrapper<double> dWrapper;
    app.add_option("-v", dWrapper);
    args = {"-v", "2.36"};

    run();

    CHECK(2.36 == dWrapper.value());

    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "intWrapperRestricted", "[newparse]") {
    objWrapperRestricted<double> dWrapper;
    app.add_option("-v", dWrapper);
    args = {"-v", "4"};

    run();

    CHECK(4.0 == dWrapper.value());

    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-v", ""};

    run();

    CHECK(0.0 == dWrapper.value());
}

static_assert(CLI::detail::is_direct_constructible<objWrapper<int>, int>::value,
              "int wrapper is not constructible from int64");

static_assert(!CLI::detail::is_direct_constructible<objWrapper<int>, double>::value,
              "int wrapper is constructible from double");

static_assert(!CLI::detail::is_istreamable<objWrapper<int>>::value,
              "int wrapper is input streamable and it shouldn't be");

TEST_CASE_METHOD(TApp, "intWrapper", "[newparse]") {
    objWrapper<int> iWrapper;
    app.add_option("-v", iWrapper);
    args = {"-v", "45"};

    run();

    CHECK(45 == iWrapper.value());
    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

static_assert(!CLI::detail::is_direct_constructible<objWrapper<float>, int>::value,
              "float wrapper is constructible from int");
static_assert(!CLI::detail::is_direct_constructible<objWrapper<float>, double>::value,
              "float wrapper is constructible from double");

static_assert(!CLI::detail::is_istreamable<objWrapper<float>>::value,
              "float wrapper is input streamable and it shouldn't be");

TEST_CASE_METHOD(TApp, "floatWrapper", "[newparse]") {
    objWrapper<float> iWrapper;
    app.add_option<objWrapper<float>, float>("-v", iWrapper);
    args = {"-v", "45.3"};

    run();

    CHECK(45.3f == iWrapper.value());
    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

#endif
/// simple class to wrap another  with a very specific type constructor to test out some of the option assignments
class dobjWrapper {
  public:
    dobjWrapper() = default;
    explicit dobjWrapper(double obj) : dval_{obj} {};
    explicit dobjWrapper(int obj) : ival_{obj} {};

    CLI11_NODISCARD double dvalue() const { return dval_; }
    CLI11_NODISCARD int ivalue() const { return ival_; }

  private:
    double dval_{0.0};
    int ival_{0};
};

TEST_CASE_METHOD(TApp, "dobjWrapper", "[newparse]") {
    dobjWrapper iWrapper;
    app.add_option("-v", iWrapper);
    args = {"-v", "45"};

    run();

    CHECK(45 == iWrapper.ivalue());
    CHECK(0.0 == iWrapper.dvalue());

    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
    iWrapper = dobjWrapper{};

    args = {"-v", "45.1"};

    run();
    CHECK(0 == iWrapper.ivalue());
    CHECK(45.1 == iWrapper.dvalue());
}

/// simple class to wrap another  with a very specific type constructor and assignment operators to test out some of the
/// option assignments
template <class X> class AobjWrapper {
  public:
    AobjWrapper() = default;
    // delete all other constructors
    template <class TT> AobjWrapper(TT &&obj) = delete;
    // single assignment operator
    AobjWrapper &operator=(X val) {
        val_ = val;
        return *this;
    }
    // delete all other assignment operators
    template <typename TT> void operator=(TT &&obj) = delete;

    CLI11_NODISCARD const X &value() const { return val_; }

  private:
    X val_{};
};

static_assert(std::is_assignable<AobjWrapper<std::uint16_t> &, std::uint16_t>::value,
              "AobjWrapper not assignable like it should be ");

TEST_CASE_METHOD(TApp, "uint16Wrapper", "[newparse]") {
    AobjWrapper<std::uint16_t> sWrapper;
    app.add_option<AobjWrapper<std::uint16_t>, std::uint16_t>("-v", sWrapper);
    args = {"-v", "9"};

    run();

    CHECK(9u == sWrapper.value());
    args = {"-v", "thing"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-v", "72456245754"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);

    args = {"-v", "-3"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

template <class T> class SimpleWrapper {
  public:
    SimpleWrapper() = default;

    explicit SimpleWrapper(T initial) : val_{std::move(initial)} {};
    T &getRef() { return val_; }
    using value_type = T;

  private:
    T val_{};
};

TEST_CASE_METHOD(TApp, "wrapperInt", "[newparse]") {
    SimpleWrapper<int> wrap;
    app.add_option("--val", wrap);
    args = {"--val", "2"};

    run();
    CHECK(2 == wrap.getRef());
}

TEST_CASE_METHOD(TApp, "wrapperString", "[newparse]") {
    SimpleWrapper<std::string> wrap;
    app.add_option("--val", wrap);
    args = {"--val", "str"};

    run();
    CHECK("str" == wrap.getRef());
}

TEST_CASE_METHOD(TApp, "wrapperVector", "[newparse]") {
    SimpleWrapper<std::vector<int>> wrap;
    app.add_option("--val", wrap);
    args = {"--val", "1", "2", "3", "4"};

    run();
    auto v1 = wrap.getRef();
    auto v2 = std::vector<int>{1, 2, 3, 4};
    CHECK(v2 == v1);
}

TEST_CASE_METHOD(TApp, "wrapperwrapperString", "[newparse]") {
    SimpleWrapper<SimpleWrapper<std::string>> wrap;
    app.add_option("--val", wrap);
    args = {"--val", "arg"};

    run();
    auto v1 = wrap.getRef().getRef();
    const auto *v2 = "arg";
    CHECK(v2 == v1);
}

TEST_CASE_METHOD(TApp, "wrapperwrapperVector", "[newparse]") {
    SimpleWrapper<SimpleWrapper<std::vector<int>>> wrap;
    auto *opt = app.add_option("--val", wrap);
    args = {"--val", "1", "2", "3", "4"};

    run();
    auto v1 = wrap.getRef().getRef();
    auto v2 = std::vector<int>{1, 2, 3, 4};
    CHECK(v2 == v1);
    opt->type_size(0, 5);

    args = {"--val"};

    run();
    CHECK(wrap.getRef().getRef().empty());

    args = {"--val", "happy", "sad"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "wrapperComplex", "[newparse]") {
    SimpleWrapper<std::complex<double>> wrap;
    app.add_option("--val", wrap);
    args = {"--val", "1", "2"};

    run();
    auto &v1 = wrap.getRef();
    auto v2 = std::complex<double>{1, 2};
    CHECK(v2.real() == v1.real());
    CHECK(v2.imag() == v1.imag());
    args = {"--val", "1.4-4j"};

    run();
    v2 = std::complex<double>{1.4, -4};
    CHECK(v2.real() == v1.real());
    CHECK(v2.imag() == v1.imag());
}

TEST_CASE_METHOD(TApp, "vectorComplex", "[newparse]") {
    std::vector<std::complex<double>> vcomplex;
    app.add_option("--val", vcomplex);
    args = {"--val", "1", "2", "--val", "1.4-4j"};

    run();

    REQUIRE(2U == vcomplex.size());
    CHECK(1.0 == vcomplex[0].real());
    CHECK(2.0 == vcomplex[0].imag());
    CHECK(1.4 == vcomplex[1].real());
    CHECK(-4.0 == vcomplex[1].imag());
}
