// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

#include "Encoding.hpp"
#include "StringTools.hpp"

namespace CLI {
// [CLI11:type_tools_hpp:verbatim]

// Type tools

// Utilities for type enabling
namespace detail {
// Based generally on https://rmf.io/cxx11/almost-static-if
/// Simple empty scoped class
enum class enabler {};

/// An instance to use in EnableIf
constexpr enabler dummy = {};
}  // namespace detail

/// A copy of enable_if_t from C++14, compatible with C++11.
///
/// We could check to see if C++14 is being used, but it does not hurt to redefine this
/// (even Google does this: https://github.com/google/skia/blob/main/include/private/SkTLogic.h)
/// It is not in the std namespace anyway, so no harm done.
template <bool B, class T = void> using enable_if_t = typename std::enable_if<B, T>::type;

/// A copy of std::void_t from C++17 (helper for C++11 and C++14)
template <typename... Ts> struct make_void {
    using type = void;
};

/// A copy of std::void_t from C++17 - same reasoning as enable_if_t, it does not hurt to redefine
template <typename... Ts> using void_t = typename make_void<Ts...>::type;

/// A copy of std::conditional_t from C++14 - same reasoning as enable_if_t, it does not hurt to redefine
template <bool B, class T, class F> using conditional_t = typename std::conditional<B, T, F>::type;

/// Check to see if something is bool (fail check by default)
template <typename T> struct is_bool : std::false_type {};

/// Check to see if something is bool (true if actually a bool)
template <> struct is_bool<bool> : std::true_type {};

/// Check to see if something is a shared pointer
template <typename T> struct is_shared_ptr : std::false_type {};

/// Check to see if something is a shared pointer (True if really a shared pointer)
template <typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

/// Check to see if something is a shared pointer (True if really a shared pointer)
template <typename T> struct is_shared_ptr<const std::shared_ptr<T>> : std::true_type {};

/// Check to see if something is copyable pointer
template <typename T> struct is_copyable_ptr {
    static bool const value = is_shared_ptr<T>::value || std::is_pointer<T>::value;
};

/// This can be specialized to override the type deduction for IsMember.
template <typename T> struct IsMemberType {
    using type = T;
};

/// The main custom type needed here is const char * should be a string.
template <> struct IsMemberType<const char *> {
    using type = std::string;
};

namespace adl_detail {
/// Check for existence of user-supplied lexical_cast.
///
/// This struct has to be in a separate namespace so that it doesn't see our lexical_cast overloads in CLI::detail.
/// Standard says it shouldn't see them if it's defined before the corresponding lexical_cast declarations, but this
/// requires a working implementation of two-phase lookup, and not all compilers can boast that (msvc, ahem).
template <typename T, typename S = std::string> class is_lexical_castable {
    template <typename TT, typename SS>
    static auto test(int) -> decltype(lexical_cast(std::declval<const SS &>(), std::declval<TT &>()), std::true_type());

    template <typename, typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<T, S>(0))::value;
};
}  // namespace adl_detail

namespace detail {

// These are utilities for IsMember and other transforming objects

/// Handy helper to access the element_type generically. This is not part of is_copyable_ptr because it requires that
/// pointer_traits<T> be valid.

/// not a pointer
template <typename T, typename Enable = void> struct element_type {
    using type = T;
};

template <typename T> struct element_type<T, typename std::enable_if<is_copyable_ptr<T>::value>::type> {
    using type = typename std::pointer_traits<T>::element_type;
};

/// Combination of the element type and value type - remove pointer (including smart pointers) and get the value_type of
/// the container
template <typename T> struct element_value_type {
    using type = typename element_type<T>::type::value_type;
};

/// Adaptor for set-like structure: This just wraps a normal container in a few utilities that do almost nothing.
template <typename T, typename _ = void> struct pair_adaptor : std::false_type {
    using value_type = typename T::value_type;
    using first_type = typename std::remove_const<value_type>::type;
    using second_type = typename std::remove_const<value_type>::type;

    /// Get the first value (really just the underlying value)
    template <typename Q> static auto first(Q &&pair_value) -> decltype(std::forward<Q>(pair_value)) {
        return std::forward<Q>(pair_value);
    }
    /// Get the second value (really just the underlying value)
    template <typename Q> static auto second(Q &&pair_value) -> decltype(std::forward<Q>(pair_value)) {
        return std::forward<Q>(pair_value);
    }
};

/// Adaptor for map-like structure (true version, must have key_type and mapped_type).
/// This wraps a mapped container in a few utilities access it in a general way.
template <typename T>
struct pair_adaptor<
    T,
    conditional_t<false, void_t<typename T::value_type::first_type, typename T::value_type::second_type>, void>>
    : std::true_type {
    using value_type = typename T::value_type;
    using first_type = typename std::remove_const<typename value_type::first_type>::type;
    using second_type = typename std::remove_const<typename value_type::second_type>::type;

    /// Get the first value (really just the underlying value)
    template <typename Q> static auto first(Q &&pair_value) -> decltype(std::get<0>(std::forward<Q>(pair_value))) {
        return std::get<0>(std::forward<Q>(pair_value));
    }
    /// Get the second value (really just the underlying value)
    template <typename Q> static auto second(Q &&pair_value) -> decltype(std::get<1>(std::forward<Q>(pair_value))) {
        return std::get<1>(std::forward<Q>(pair_value));
    }
};

// Warning is suppressed due to "bug" in gcc<5.0 and gcc 7.0 with c++17 enabled that generates a -Wnarrowing warning
// in the unevaluated context even if the function that was using this wasn't used.  The standard says narrowing in
// brace initialization shouldn't be allowed but for backwards compatibility gcc allows it in some contexts.  It is a
// little fuzzy what happens in template constructs and I think that was something GCC took a little while to work out.
// But regardless some versions of gcc generate a warning when they shouldn't from the following code so that should be
// suppressed
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif
// check for constructibility from a specific type and copy assignable used in the parse detection
template <typename T, typename C> class is_direct_constructible {
    template <typename TT, typename CC>
    static auto test(int, std::true_type) -> decltype(
// NVCC warns about narrowing conversions here
#ifdef __CUDACC__
#ifdef __NVCC_DIAG_PRAGMA_SUPPORT__
#pragma nv_diag_suppress 2361
#else
#pragma diag_suppress 2361
#endif
#endif
        TT{std::declval<CC>()}
#ifdef __CUDACC__
#ifdef __NVCC_DIAG_PRAGMA_SUPPORT__
#pragma nv_diag_default 2361
#else
#pragma diag_default 2361
#endif
#endif
        ,
        std::is_move_assignable<TT>());

    template <typename TT, typename CC> static auto test(int, std::false_type) -> std::false_type;

    template <typename, typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<T, C>(0, typename std::is_constructible<T, C>::type()))::value;
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// Check for output streamability
// Based on https://stackoverflow.com/questions/22758291/how-can-i-detect-if-a-type-can-be-streamed-to-an-stdostream

template <typename T, typename S = std::ostringstream> class is_ostreamable {
    template <typename TT, typename SS>
    static auto test(int) -> decltype(std::declval<SS &>() << std::declval<TT>(), std::true_type());

    template <typename, typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<T, S>(0))::value;
};

/// Check for input streamability
template <typename T, typename S = std::istringstream> class is_istreamable {
    template <typename TT, typename SS>
    static auto test(int) -> decltype(std::declval<SS &>() >> std::declval<TT &>(), std::true_type());

    template <typename, typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<T, S>(0))::value;
};

/// Check for complex
template <typename T> class is_complex {
    template <typename TT>
    static auto test(int) -> decltype(std::declval<TT>().real(), std::declval<TT>().imag(), std::true_type());

    template <typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

/// Templated operation to get a value from a stream
template <typename T, enable_if_t<is_istreamable<T>::value, detail::enabler> = detail::dummy>
bool from_stream(const std::string &istring, T &obj) {
    std::istringstream is;
    is.str(istring);
    is >> obj;
    return !is.fail() && !is.rdbuf()->in_avail();
}

template <typename T, enable_if_t<!is_istreamable<T>::value, detail::enabler> = detail::dummy>
bool from_stream(const std::string & /*istring*/, T & /*obj*/) {
    return false;
}

// check to see if an object is a mutable container (fail by default)
template <typename T, typename _ = void> struct is_mutable_container : std::false_type {};

/// type trait to test if a type is a mutable container meaning it has a value_type, it has an iterator, a clear, and
/// end methods and an insert function.  And for our purposes we exclude std::string and types that can be constructed
/// from a std::string
template <typename T>
struct is_mutable_container<
    T,
    conditional_t<false,
                  void_t<typename T::value_type,
                         decltype(std::declval<T>().end()),
                         decltype(std::declval<T>().clear()),
                         decltype(std::declval<T>().insert(std::declval<decltype(std::declval<T>().end())>(),
                                                           std::declval<const typename T::value_type &>()))>,
                  void>> : public conditional_t<std::is_constructible<T, std::string>::value ||
                                                    std::is_constructible<T, std::wstring>::value,
                                                std::false_type,
                                                std::true_type> {};

// check to see if an object is a mutable container (fail by default)
template <typename T, typename _ = void> struct is_readable_container : std::false_type {};

/// type trait to test if a type is a container meaning it has a value_type, it has an iterator, and an end
/// method.
template <typename T>
struct is_readable_container<
    T,
    conditional_t<false, void_t<decltype(std::declval<T>().end()), decltype(std::declval<T>().begin())>, void>>
    : public std::true_type {};

// check to see if an object is a wrapper (fail by default)
template <typename T, typename _ = void> struct is_wrapper : std::false_type {};

// check if an object is a wrapper (it has a value_type defined)
template <typename T>
struct is_wrapper<T, conditional_t<false, void_t<typename T::value_type>, void>> : public std::true_type {};

// Check for tuple like types, as in classes with a tuple_size type trait
// Even though in C++26 std::complex gains a std::tuple interface, for our purposes we treat is as NOT a tuple
template <typename S> class is_tuple_like {
    template <typename SS, enable_if_t<!is_complex<SS>::value, detail::enabler> = detail::dummy>
    // static auto test(int)
    //     -> decltype(std::conditional<(std::tuple_size<SS>::value > 0), std::true_type, std::false_type>::type());
    static auto test(int) -> decltype(std::tuple_size<typename std::decay<SS>::type>::value, std::true_type{});
    template <typename> static auto test(...) -> std::false_type;

  public:
    static constexpr bool value = decltype(test<S>(0))::value;
};

/// This will only trigger for actual void type
template <typename T, typename Enable = void> struct type_count_base {
    static const int value{0};
};

/// Type size for regular object types that do not look like a tuple
template <typename T>
struct type_count_base<T,
                       typename std::enable_if<!is_tuple_like<T>::value && !is_mutable_container<T>::value &&
                                               !std::is_void<T>::value>::type> {
    static constexpr int value{1};
};

/// the base tuple size
template <typename T>
struct type_count_base<T, typename std::enable_if<is_tuple_like<T>::value && !is_mutable_container<T>::value>::type> {
    static constexpr int value{// cppcheck-suppress unusedStructMember
                               std::tuple_size<typename std::decay<T>::type>::value};
};

/// Type count base for containers is the type_count_base of the individual element
template <typename T> struct type_count_base<T, typename std::enable_if<is_mutable_container<T>::value>::type> {
    static constexpr int value{type_count_base<typename T::value_type>::value};
};

/// Convert an object to a string (directly forward if this can become a string)
template <typename T, enable_if_t<std::is_convertible<T, std::string>::value, detail::enabler> = detail::dummy>
auto to_string(T &&value) -> decltype(std::forward<T>(value)) {
    return std::forward<T>(value);
}

/// Construct a string from the object
template <typename T,
          enable_if_t<std::is_constructible<std::string, T>::value && !std::is_convertible<T, std::string>::value,
                      detail::enabler> = detail::dummy>
std::string to_string(T &&value) {
    return std::string(value);  // NOLINT(google-readability-casting)
}

/// Convert an object to a string (streaming must be supported for that type)
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          is_ostreamable<T>::value,
                      detail::enabler> = detail::dummy>
std::string to_string(T &&value) {
    std::stringstream stream;
    stream << value;
    return stream.str();
}

// additional forward declarations

/// Print tuple value string for tuples of size ==1
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value == 1,
                      detail::enabler> = detail::dummy>
inline std::string to_string(T &&value);

/// Print tuple value string for tuples of size > 1
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value >= 2,
                      detail::enabler> = detail::dummy>
inline std::string to_string(T &&value);

/// If conversion is not supported, return an empty string (streaming is not supported for that type)
template <
    typename T,
    enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                    !is_ostreamable<T>::value && !is_readable_container<typename std::remove_const<T>::type>::value &&
                    !is_tuple_like<T>::value,
                detail::enabler> = detail::dummy>
inline std::string to_string(T &&) {
    return {};
}

/// convert a readable container to a string
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          !is_ostreamable<T>::value && is_readable_container<T>::value,
                      detail::enabler> = detail::dummy>
inline std::string to_string(T &&variable) {
    auto cval = variable.begin();
    auto end = variable.end();
    if(cval == end) {
        return {"{}"};
    }
    std::vector<std::string> defaults;
    while(cval != end) {
        defaults.emplace_back(CLI::detail::to_string(*cval));
        ++cval;
    }
    return {"[" + detail::join(defaults) + "]"};
}

/// Convert a tuple like object to a string

/// forward declarations for tuple_value_strings
template <typename T, std::size_t I>
inline typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_value_string(T && /*value*/);

/// Recursively generate the tuple value string
template <typename T, std::size_t I>
inline typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_value_string(T &&value);

/// Print tuple value string for tuples of size ==1
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value == 1,
                      detail::enabler>>
inline std::string to_string(T &&value) {
    return to_string(std::get<0>(value));
}

/// Print tuple value string for tuples of size > 1
template <typename T,
          enable_if_t<!std::is_convertible<T, std::string>::value && !std::is_constructible<std::string, T>::value &&
                          !is_ostreamable<T>::value && is_tuple_like<T>::value && type_count_base<T>::value >= 2,
                      detail::enabler>>
inline std::string to_string(T &&value) {
    auto tname = std::string(1, '[') + tuple_value_string<T, 0>(value);
    tname.push_back(']');
    return tname;
}

/// Empty string if the index > tuple size
template <typename T, std::size_t I>
inline typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_value_string(T && /*value*/) {
    return std::string{};
}

/// Recursively generate the tuple value string
template <typename T, std::size_t I>
inline typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_value_string(T &&value) {
    auto str = std::string{to_string(std::get<I>(value))} + ',' + tuple_value_string<T, I + 1>(value);
    if(str.back() == ',')
        str.pop_back();
    return str;
}

/// special template overload
template <typename T1,
          typename T2,
          typename T,
          enable_if_t<std::is_same<T1, T2>::value, detail::enabler> = detail::dummy>
auto checked_to_string(T &&value) -> decltype(to_string(std::forward<T>(value))) {
    return to_string(std::forward<T>(value));
}

/// special template overload
template <typename T1,
          typename T2,
          typename T,
          enable_if_t<!std::is_same<T1, T2>::value, detail::enabler> = detail::dummy>
std::string checked_to_string(T &&) {
    return std::string{};
}
/// get a string as a convertible value for arithmetic types
template <typename T, enable_if_t<std::is_arithmetic<T>::value, detail::enabler> = detail::dummy>
std::string value_string(const T &value) {
    return std::to_string(value);
}
/// get a string as a convertible value for enumerations
template <typename T, enable_if_t<std::is_enum<T>::value, detail::enabler> = detail::dummy>
std::string value_string(const T &value) {
    return std::to_string(static_cast<typename std::underlying_type<T>::type>(value));
}
/// for other types just use the regular to_string function
template <typename T,
          enable_if_t<!std::is_enum<T>::value && !std::is_arithmetic<T>::value, detail::enabler> = detail::dummy>
auto value_string(const T &value) -> decltype(to_string(value)) {
    return to_string(value);
}

/// template to get the underlying value type if it exists or use a default
template <typename T, typename def, typename Enable = void> struct wrapped_type {
    using type = def;
};

/// Type size for regular object types that do not look like a tuple
template <typename T, typename def> struct wrapped_type<T, def, typename std::enable_if<is_wrapper<T>::value>::type> {
    using type = typename T::value_type;
};

/// Set of overloads to get the type size of an object

/// forward declare the subtype_count structure
template <typename T> struct subtype_count;

/// forward declare the subtype_count_min structure
template <typename T> struct subtype_count_min;

/// This will only trigger for actual void type
template <typename T, typename Enable = void> struct type_count {
    static const int value{0};
};

/// Type size for regular object types that do not look like a tuple
template <typename T>
struct type_count<T,
                  typename std::enable_if<!is_wrapper<T>::value && !is_tuple_like<T>::value && !is_complex<T>::value &&
                                          !std::is_void<T>::value>::type> {
    static constexpr int value{1};
};

/// Type size for complex since it sometimes looks like a wrapper
template <typename T> struct type_count<T, typename std::enable_if<is_complex<T>::value>::type> {
    static constexpr int value{2};
};

/// Type size of types that are wrappers,except complex and tuples(which can also be wrappers sometimes)
template <typename T> struct type_count<T, typename std::enable_if<is_mutable_container<T>::value>::type> {
    static constexpr int value{subtype_count<typename T::value_type>::value};
};

/// Type size of types that are wrappers,except containers complex and tuples(which can also be wrappers sometimes)
template <typename T>
struct type_count<T,
                  typename std::enable_if<is_wrapper<T>::value && !is_complex<T>::value && !is_tuple_like<T>::value &&
                                          !is_mutable_container<T>::value>::type> {
    static constexpr int value{type_count<typename T::value_type>::value};
};

/// 0 if the index > tuple size
template <typename T, std::size_t I>
constexpr typename std::enable_if<I == type_count_base<T>::value, int>::type tuple_type_size() {
    return 0;
}

/// Recursively generate the tuple type name
template <typename T, std::size_t I>
    constexpr typename std::enable_if < I<type_count_base<T>::value, int>::type tuple_type_size() {
    return subtype_count<typename std::tuple_element<I, T>::type>::value + tuple_type_size<T, I + 1>();
}

/// Get the type size of the sum of type sizes for all the individual tuple types
template <typename T> struct type_count<T, typename std::enable_if<is_tuple_like<T>::value>::type> {
    static constexpr int value{tuple_type_size<T, 0>()};
};

/// definition of subtype count
template <typename T> struct subtype_count {
    static constexpr int value{is_mutable_container<T>::value ? expected_max_vector_size : type_count<T>::value};
};

/// This will only trigger for actual void type
template <typename T, typename Enable = void> struct type_count_min {
    static const int value{0};
};

/// Type size for regular object types that do not look like a tuple
template <typename T>
struct type_count_min<
    T,
    typename std::enable_if<!is_mutable_container<T>::value && !is_tuple_like<T>::value && !is_wrapper<T>::value &&
                            !is_complex<T>::value && !std::is_void<T>::value>::type> {
    static constexpr int value{type_count<T>::value};
};

/// Type size for complex since it sometimes looks like a wrapper
template <typename T> struct type_count_min<T, typename std::enable_if<is_complex<T>::value>::type> {
    static constexpr int value{1};
};

/// Type size min of types that are wrappers,except complex and tuples(which can also be wrappers sometimes)
template <typename T>
struct type_count_min<
    T,
    typename std::enable_if<is_wrapper<T>::value && !is_complex<T>::value && !is_tuple_like<T>::value>::type> {
    static constexpr int value{subtype_count_min<typename T::value_type>::value};
};

/// 0 if the index > tuple size
template <typename T, std::size_t I>
constexpr typename std::enable_if<I == type_count_base<T>::value, int>::type tuple_type_size_min() {
    return 0;
}

/// Recursively generate the tuple type name
template <typename T, std::size_t I>
    constexpr typename std::enable_if < I<type_count_base<T>::value, int>::type tuple_type_size_min() {
    return subtype_count_min<typename std::tuple_element<I, T>::type>::value + tuple_type_size_min<T, I + 1>();
}

/// Get the type size of the sum of type sizes for all the individual tuple types
template <typename T> struct type_count_min<T, typename std::enable_if<is_tuple_like<T>::value>::type> {
    static constexpr int value{tuple_type_size_min<T, 0>()};
};

/// definition of subtype count
template <typename T> struct subtype_count_min {
    static constexpr int value{is_mutable_container<T>::value
                                   ? ((type_count<T>::value < expected_max_vector_size) ? type_count<T>::value : 0)
                                   : type_count_min<T>::value};
};

/// This will only trigger for actual void type
template <typename T, typename Enable = void> struct expected_count {
    static const int value{0};
};

/// For most types the number of expected items is 1
template <typename T>
struct expected_count<T,
                      typename std::enable_if<!is_mutable_container<T>::value && !is_wrapper<T>::value &&
                                              !std::is_void<T>::value>::type> {
    static constexpr int value{1};
};
/// number of expected items in a vector
template <typename T> struct expected_count<T, typename std::enable_if<is_mutable_container<T>::value>::type> {
    static constexpr int value{expected_max_vector_size};
};

/// number of expected items in a vector
template <typename T>
struct expected_count<T, typename std::enable_if<!is_mutable_container<T>::value && is_wrapper<T>::value>::type> {
    static constexpr int value{expected_count<typename T::value_type>::value};
};

// Enumeration of the different supported categorizations of objects
enum class object_category : int {
    char_value = 1,
    integral_value = 2,
    unsigned_integral = 4,
    enumeration = 6,
    boolean_value = 8,
    floating_point = 10,
    number_constructible = 12,
    double_constructible = 14,
    integer_constructible = 16,
    // string like types
    string_assignable = 23,
    string_constructible = 24,
    wstring_assignable = 25,
    wstring_constructible = 26,
    other = 45,
    // special wrapper or container types
    wrapper_value = 50,
    complex_number = 60,
    tuple_value = 70,
    container_value = 80,

};

/// Set of overloads to classify an object according to type

/// some type that is not otherwise recognized
template <typename T, typename Enable = void> struct classify_object {
    static constexpr object_category value{object_category::other};
};

/// Signed integers
template <typename T>
struct classify_object<
    T,
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value && std::is_signed<T>::value &&
                            !is_bool<T>::value && !std::is_enum<T>::value>::type> {
    static constexpr object_category value{object_category::integral_value};
};

/// Unsigned integers
template <typename T>
struct classify_object<T,
                       typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value &&
                                               !std::is_same<T, char>::value && !is_bool<T>::value>::type> {
    static constexpr object_category value{object_category::unsigned_integral};
};

/// single character values
template <typename T>
struct classify_object<T, typename std::enable_if<std::is_same<T, char>::value && !std::is_enum<T>::value>::type> {
    static constexpr object_category value{object_category::char_value};
};

/// Boolean values
template <typename T> struct classify_object<T, typename std::enable_if<is_bool<T>::value>::type> {
    static constexpr object_category value{object_category::boolean_value};
};

/// Floats
template <typename T> struct classify_object<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
    static constexpr object_category value{object_category::floating_point};
};
#if defined _MSC_VER
// in MSVC wstring should take precedence if available this isn't as useful on other compilers due to the broader use of
// utf-8 encoding
#define WIDE_STRING_CHECK                                                                                              \
    !std::is_assignable<T &, std::wstring>::value && !std::is_constructible<T, std::wstring>::value
#define STRING_CHECK true
#else
#define WIDE_STRING_CHECK true
#define STRING_CHECK !std::is_assignable<T &, std::string>::value && !std::is_constructible<T, std::string>::value
#endif

/// String and similar direct assignment
template <typename T>
struct classify_object<
    T,
    typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value && WIDE_STRING_CHECK &&
                            std::is_assignable<T &, std::string>::value>::type> {
    static constexpr object_category value{object_category::string_assignable};
};

/// String and similar constructible and copy assignment
template <typename T>
struct classify_object<
    T,
    typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                            !std::is_assignable<T &, std::string>::value && (type_count<T>::value == 1) &&
                            WIDE_STRING_CHECK && std::is_constructible<T, std::string>::value>::type> {
    static constexpr object_category value{object_category::string_constructible};
};

/// Wide strings
template <typename T>
struct classify_object<T,
                       typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                                               STRING_CHECK && std::is_assignable<T &, std::wstring>::value>::type> {
    static constexpr object_category value{object_category::wstring_assignable};
};

template <typename T>
struct classify_object<
    T,
    typename std::enable_if<!std::is_floating_point<T>::value && !std::is_integral<T>::value &&
                            !std::is_assignable<T &, std::wstring>::value && (type_count<T>::value == 1) &&
                            STRING_CHECK && std::is_constructible<T, std::wstring>::value>::type> {
    static constexpr object_category value{object_category::wstring_constructible};
};

/// Enumerations
template <typename T> struct classify_object<T, typename std::enable_if<std::is_enum<T>::value>::type> {
    static constexpr object_category value{object_category::enumeration};
};

template <typename T> struct classify_object<T, typename std::enable_if<is_complex<T>::value>::type> {
    static constexpr object_category value{object_category::complex_number};
};

/// Handy helper to contain a bunch of checks that rule out many common types (integers, string like, floating point,
/// vectors, and enumerations
template <typename T> struct uncommon_type {
    using type = typename std::conditional<
        !std::is_floating_point<T>::value && !std::is_integral<T>::value &&
            !std::is_assignable<T &, std::string>::value && !std::is_constructible<T, std::string>::value &&
            !std::is_assignable<T &, std::wstring>::value && !std::is_constructible<T, std::wstring>::value &&
            !is_complex<T>::value && !is_mutable_container<T>::value && !std::is_enum<T>::value,
        std::true_type,
        std::false_type>::type;
    static constexpr bool value = type::value;
};

/// wrapper type
template <typename T>
struct classify_object<T,
                       typename std::enable_if<(!is_mutable_container<T>::value && is_wrapper<T>::value &&
                                                !is_tuple_like<T>::value && uncommon_type<T>::value)>::type> {
    static constexpr object_category value{object_category::wrapper_value};
};

/// Assignable from double or int
template <typename T>
struct classify_object<T,
                       typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 &&
                                               !is_wrapper<T>::value && is_direct_constructible<T, double>::value &&
                                               is_direct_constructible<T, int>::value>::type> {
    static constexpr object_category value{object_category::number_constructible};
};

/// Assignable from int
template <typename T>
struct classify_object<T,
                       typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 &&
                                               !is_wrapper<T>::value && !is_direct_constructible<T, double>::value &&
                                               is_direct_constructible<T, int>::value>::type> {
    static constexpr object_category value{object_category::integer_constructible};
};

/// Assignable from double
template <typename T>
struct classify_object<T,
                       typename std::enable_if<uncommon_type<T>::value && type_count<T>::value == 1 &&
                                               !is_wrapper<T>::value && is_direct_constructible<T, double>::value &&
                                               !is_direct_constructible<T, int>::value>::type> {
    static constexpr object_category value{object_category::double_constructible};
};

/// Tuple type
template <typename T>
struct classify_object<
    T,
    typename std::enable_if<is_tuple_like<T>::value &&
                            ((type_count<T>::value >= 2 && !is_wrapper<T>::value) ||
                             (uncommon_type<T>::value && !is_direct_constructible<T, double>::value &&
                              !is_direct_constructible<T, int>::value) ||
                             (uncommon_type<T>::value && type_count<T>::value >= 2))>::type> {
    static constexpr object_category value{object_category::tuple_value};
    // the condition on this class requires it be like a tuple, but on some compilers (like Xcode) tuples can be
    // constructed from just the first element so tuples of <string, int,int> can be constructed from a string, which
    // could lead to issues so there are two variants of the condition, the first isolates things with a type size >=2
    // mainly to get tuples on Xcode with the exception of wrappers, the second is the main one and just separating out
    // those cases that are caught by other object classifications
};

/// container type
template <typename T> struct classify_object<T, typename std::enable_if<is_mutable_container<T>::value>::type> {
    static constexpr object_category value{object_category::container_value};
};

// Type name print

/// Was going to be based on
///  http://stackoverflow.com/questions/1055452/c-get-name-of-type-in-template
/// But this is cleaner and works better in this case

template <typename T,
          enable_if_t<classify_object<T>::value == object_category::char_value, detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "CHAR";
}

template <typename T,
          enable_if_t<classify_object<T>::value == object_category::integral_value ||
                          classify_object<T>::value == object_category::integer_constructible,
                      detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "INT";
}

template <typename T,
          enable_if_t<classify_object<T>::value == object_category::unsigned_integral, detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "UINT";
}

template <typename T,
          enable_if_t<classify_object<T>::value == object_category::floating_point ||
                          classify_object<T>::value == object_category::number_constructible ||
                          classify_object<T>::value == object_category::double_constructible,
                      detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "FLOAT";
}

/// Print name for enumeration types
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::enumeration, detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "ENUM";
}

/// Print name for enumeration types
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::boolean_value, detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "BOOLEAN";
}

/// Print name for enumeration types
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::complex_number, detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "COMPLEX";
}

/// Print for all other types
template <typename T,
          enable_if_t<classify_object<T>::value >= object_category::string_assignable &&
                          classify_object<T>::value <= object_category::other,
                      detail::enabler> = detail::dummy>
constexpr const char *type_name() {
    return "TEXT";
}
/// typename for tuple value
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value >= 2,
                      detail::enabler> = detail::dummy>
std::string type_name();  // forward declaration

/// Generate type name for a wrapper or container value
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::container_value ||
                          classify_object<T>::value == object_category::wrapper_value,
                      detail::enabler> = detail::dummy>
std::string type_name();  // forward declaration

/// Print name for single element tuple types
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value == 1,
                      detail::enabler> = detail::dummy>
inline std::string type_name() {
    return type_name<typename std::decay<typename std::tuple_element<0, T>::type>::type>();
}

/// Empty string if the index > tuple size
template <typename T, std::size_t I>
inline typename std::enable_if<I == type_count_base<T>::value, std::string>::type tuple_name() {
    return std::string{};
}

/// Recursively generate the tuple type name
template <typename T, std::size_t I>
inline typename std::enable_if<(I < type_count_base<T>::value), std::string>::type tuple_name() {
    auto str = std::string{type_name<typename std::decay<typename std::tuple_element<I, T>::type>::type>()} + ',' +
               tuple_name<T, I + 1>();
    if(str.back() == ',')
        str.pop_back();
    return str;
}

/// Print type name for tuples with 2 or more elements
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::tuple_value && type_count_base<T>::value >= 2,
                      detail::enabler>>
inline std::string type_name() {
    auto tname = std::string(1, '[') + tuple_name<T, 0>();
    tname.push_back(']');
    return tname;
}

/// get the type name for a type that has a value_type member
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::container_value ||
                          classify_object<T>::value == object_category::wrapper_value,
                      detail::enabler>>
inline std::string type_name() {
    return type_name<typename T::value_type>();
}

// Lexical cast

/// Convert to an unsigned integral
template <typename T, enable_if_t<std::is_unsigned<T>::value, detail::enabler> = detail::dummy>
bool integral_conversion(const std::string &input, T &output) noexcept {
    if(input.empty() || input.front() == '-') {
        return false;
    }
    char *val{nullptr};
    errno = 0;
    std::uint64_t output_ll = std::strtoull(input.c_str(), &val, 0);
    if(errno == ERANGE) {
        return false;
    }
    output = static_cast<T>(output_ll);
    if(val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll) {
        return true;
    }
    val = nullptr;
    std::int64_t output_sll = std::strtoll(input.c_str(), &val, 0);
    if(val == (input.c_str() + input.size())) {
        output = (output_sll < 0) ? static_cast<T>(0) : static_cast<T>(output_sll);
        return (static_cast<std::int64_t>(output) == output_sll);
    }
    // remove separators
    if(input.find_first_of("_'") != std::string::npos) {
        std::string nstring = input;
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '_'), nstring.end());
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '\''), nstring.end());
        return integral_conversion(nstring, output);
    }
    if(std::isspace(static_cast<unsigned char>(input.back()))) {
        return integral_conversion(trim_copy(input), output);
    }
    if(input.compare(0, 2, "0o") == 0 || input.compare(0, 2, "0O") == 0) {
        val = nullptr;
        errno = 0;
        output_ll = std::strtoull(input.c_str() + 2, &val, 8);
        if(errno == ERANGE) {
            return false;
        }
        output = static_cast<T>(output_ll);
        return (val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll);
    }
    if(input.compare(0, 2, "0b") == 0 || input.compare(0, 2, "0B") == 0) {
        // LCOV_EXCL_START
        // In some new compilers including the coverage testing one binary strings are handled properly in strtoull
        // automatically so this coverage is missing but is well tested in other compilers
        val = nullptr;
        errno = 0;
        output_ll = std::strtoull(input.c_str() + 2, &val, 2);
        if(errno == ERANGE) {
            return false;
        }
        output = static_cast<T>(output_ll);
        return (val == (input.c_str() + input.size()) && static_cast<std::uint64_t>(output) == output_ll);
        // LCOV_EXCL_STOP
    }
    return false;
}

/// Convert to a signed integral
template <typename T, enable_if_t<std::is_signed<T>::value, detail::enabler> = detail::dummy>
bool integral_conversion(const std::string &input, T &output) noexcept {
    if(input.empty()) {
        return false;
    }
    char *val = nullptr;
    errno = 0;
    std::int64_t output_ll = std::strtoll(input.c_str(), &val, 0);
    if(errno == ERANGE) {
        return false;
    }
    output = static_cast<T>(output_ll);
    if(val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll) {
        return true;
    }
    if(input == "true") {
        // this is to deal with a few oddities with flags and wrapper int types
        output = static_cast<T>(1);
        return true;
    }
    // remove separators and trailing spaces
    if(input.find_first_of("_'") != std::string::npos) {
        std::string nstring = input;
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '_'), nstring.end());
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '\''), nstring.end());
        return integral_conversion(nstring, output);
    }
    if(std::isspace(static_cast<unsigned char>(input.back()))) {
        return integral_conversion(trim_copy(input), output);
    }
    if(input.compare(0, 2, "0o") == 0 || input.compare(0, 2, "0O") == 0) {
        val = nullptr;
        errno = 0;
        output_ll = std::strtoll(input.c_str() + 2, &val, 8);
        if(errno == ERANGE) {
            return false;
        }
        output = static_cast<T>(output_ll);
        return (val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll);
    }
    if(input.compare(0, 2, "0b") == 0 || input.compare(0, 2, "0B") == 0) {
        // LCOV_EXCL_START
        // In some new compilers including the coverage testing one binary strings are handled properly in strtoll
        // automatically so this coverage is missing but is well tested in other compilers
        val = nullptr;
        errno = 0;
        output_ll = std::strtoll(input.c_str() + 2, &val, 2);
        if(errno == ERANGE) {
            return false;
        }
        output = static_cast<T>(output_ll);
        return (val == (input.c_str() + input.size()) && static_cast<std::int64_t>(output) == output_ll);
        // LCOV_EXCL_STOP
    }
    return false;
}

/// Convert a flag into an integer value  typically binary flags sets errno to nonzero if conversion failed
inline std::int64_t to_flag_value(std::string val) noexcept {
    static const std::string trueString("true");
    static const std::string falseString("false");
    if(val == trueString) {
        return 1;
    }
    if(val == falseString) {
        return -1;
    }
    val = detail::to_lower(val);
    std::int64_t ret = 0;
    if(val.size() == 1) {
        if(val[0] >= '1' && val[0] <= '9') {
            return (static_cast<std::int64_t>(val[0]) - '0');
        }
        switch(val[0]) {
        case '0':
        case 'f':
        case 'n':
        case '-':
            ret = -1;
            break;
        case 't':
        case 'y':
        case '+':
            ret = 1;
            break;
        default:
            errno = EINVAL;
            return -1;
        }
        return ret;
    }
    if(val == trueString || val == "on" || val == "yes" || val == "enable") {
        ret = 1;
    } else if(val == falseString || val == "off" || val == "no" || val == "disable") {
        ret = -1;
    } else {
        char *loc_ptr{nullptr};
        ret = std::strtoll(val.c_str(), &loc_ptr, 0);
        if(loc_ptr != (val.c_str() + val.size()) && errno == 0) {
            errno = EINVAL;
        }
    }
    return ret;
}

/// Integer conversion
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::integral_value ||
                          classify_object<T>::value == object_category::unsigned_integral,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    return integral_conversion(input, output);
}

/// char values
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::char_value, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    if(input.size() == 1) {
        output = static_cast<T>(input[0]);
        return true;
    }
    return integral_conversion(input, output);
}

/// Boolean values
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::boolean_value, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    errno = 0;
    auto out = to_flag_value(input);
    if(errno == 0) {
        output = (out > 0);
    } else if(errno == ERANGE) {
        output = (input[0] != '-');
    } else {
        return false;
    }
    return true;
}

/// Floats
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::floating_point, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    if(input.empty()) {
        return false;
    }
    char *val = nullptr;
    auto output_ld = std::strtold(input.c_str(), &val);
    output = static_cast<T>(output_ld);
    if(val == (input.c_str() + input.size())) {
        return true;
    }
    while(std::isspace(static_cast<unsigned char>(*val))) {
        ++val;
        if(val == (input.c_str() + input.size())) {
            return true;
        }
    }

    // remove separators
    if(input.find_first_of("_'") != std::string::npos) {
        std::string nstring = input;
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '_'), nstring.end());
        nstring.erase(std::remove(nstring.begin(), nstring.end(), '\''), nstring.end());
        return lexical_cast(nstring, output);
    }
    return false;
}

/// complex
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::complex_number, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    using XC = typename wrapped_type<T, double>::type;
    XC x{0.0}, y{0.0};
    auto str1 = input;
    bool worked = false;
    auto nloc = str1.find_last_of("+-");
    if(nloc != std::string::npos && nloc > 0) {
        worked = lexical_cast(str1.substr(0, nloc), x);
        str1 = str1.substr(nloc);
        if(str1.back() == 'i' || str1.back() == 'j')
            str1.pop_back();
        worked = worked && lexical_cast(str1, y);
    } else {
        if(str1.back() == 'i' || str1.back() == 'j') {
            str1.pop_back();
            worked = lexical_cast(str1, y);
            x = XC{0};
        } else {
            worked = lexical_cast(str1, x);
            y = XC{0};
        }
    }
    if(worked) {
        output = T{x, y};
        return worked;
    }
    return from_stream(input, output);
}

/// String and similar direct assignment
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::string_assignable, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    output = input;
    return true;
}

/// String and similar constructible and copy assignment
template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::string_constructible, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    output = T(input);
    return true;
}

/// Wide strings
template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::wstring_assignable, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    output = widen(input);
    return true;
}

template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::wstring_constructible, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    output = T{widen(input)};
    return true;
}

/// Enumerations
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::enumeration, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    typename std::underlying_type<T>::type val;
    if(!integral_conversion(input, val)) {
        return false;
    }
    output = static_cast<T>(val);
    return true;
}

/// wrapper types
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::wrapper_value &&
                          std::is_assignable<T &, typename T::value_type>::value,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    typename T::value_type val;
    if(lexical_cast(input, val)) {
        output = val;
        return true;
    }
    return from_stream(input, output);
}

template <typename T,
          enable_if_t<classify_object<T>::value == object_category::wrapper_value &&
                          !std::is_assignable<T &, typename T::value_type>::value && std::is_assignable<T &, T>::value,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    typename T::value_type val;
    if(lexical_cast(input, val)) {
        output = T{val};
        return true;
    }
    return from_stream(input, output);
}

/// Assignable from double or int
template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::number_constructible, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    int val = 0;
    if(integral_conversion(input, val)) {
        output = T(val);
        return true;
    }

    double dval = 0.0;
    if(lexical_cast(input, dval)) {
        output = T{dval};
        return true;
    }

    return from_stream(input, output);
}

/// Assignable from int
template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::integer_constructible, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    int val = 0;
    if(integral_conversion(input, val)) {
        output = T(val);
        return true;
    }
    return from_stream(input, output);
}

/// Assignable from double
template <
    typename T,
    enable_if_t<classify_object<T>::value == object_category::double_constructible, detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    double val = 0.0;
    if(lexical_cast(input, val)) {
        output = T{val};
        return true;
    }
    return from_stream(input, output);
}

/// Non-string convertible from an int
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::other && std::is_assignable<T &, int>::value,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    int val = 0;
    if(integral_conversion(input, val)) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4800)
#endif
        // with Atomic<XX> this could produce a warning due to the conversion but if atomic gets here it is an old style
        // so will most likely still work
        output = val;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        return true;
    }
    // LCOV_EXCL_START
    // This version of cast is only used for odd cases in an older compilers the fail over
    // from_stream is tested elsewhere an not relevant for coverage here
    return from_stream(input, output);
    // LCOV_EXCL_STOP
}

/// Non-string parsable by a stream
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::other && !std::is_assignable<T &, int>::value &&
                          is_istreamable<T>::value,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string &input, T &output) {
    return from_stream(input, output);
}

/// Fallback overload that prints a human-readable error for types that we don't recognize and that don't have a
/// user-supplied lexical_cast overload.
template <typename T,
          enable_if_t<classify_object<T>::value == object_category::other && !std::is_assignable<T &, int>::value &&
                          !is_istreamable<T>::value && !adl_detail::is_lexical_castable<T>::value,
                      detail::enabler> = detail::dummy>
bool lexical_cast(const std::string & /*input*/, T & /*output*/) {
    static_assert(!std::is_same<T, T>::value,  // Can't just write false here.
                  "option object type must have a lexical cast overload or streaming input operator(>>) defined, if it "
                  "is convertible from another type use the add_option<T, XC>(...) with XC being the known type");
    return false;
}

/// Assign a value through lexical cast operations
/// Strings can be empty so we need to do a little different
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<std::is_same<AssignTo, ConvertTo>::value &&
                          (classify_object<AssignTo>::value == object_category::string_assignable ||
                           classify_object<AssignTo>::value == object_category::string_constructible ||
                           classify_object<AssignTo>::value == object_category::wstring_assignable ||
                           classify_object<AssignTo>::value == object_category::wstring_constructible),
                      detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    return lexical_cast(input, output);
}

/// Assign a value through lexical cast operations
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<std::is_same<AssignTo, ConvertTo>::value && std::is_assignable<AssignTo &, AssignTo>::value &&
                          classify_object<AssignTo>::value != object_category::string_assignable &&
                          classify_object<AssignTo>::value != object_category::string_constructible &&
                          classify_object<AssignTo>::value != object_category::wstring_assignable &&
                          classify_object<AssignTo>::value != object_category::wstring_constructible,
                      detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    if(input.empty()) {
        output = AssignTo{};
        return true;
    }

    return lexical_cast(input, output);
}  // LCOV_EXCL_LINE

/// Assign a value through lexical cast operations
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<std::is_same<AssignTo, ConvertTo>::value && !std::is_assignable<AssignTo &, AssignTo>::value &&
                          classify_object<AssignTo>::value == object_category::wrapper_value,
                      detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    if(input.empty()) {
        typename AssignTo::value_type emptyVal{};
        output = emptyVal;
        return true;
    }
    return lexical_cast(input, output);
}

/// Assign a value through lexical cast operations for int compatible values
/// mainly for atomic operations on some compilers
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<std::is_same<AssignTo, ConvertTo>::value && !std::is_assignable<AssignTo &, AssignTo>::value &&
                          classify_object<AssignTo>::value != object_category::wrapper_value &&
                          std::is_assignable<AssignTo &, int>::value,
                      detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    if(input.empty()) {
        output = 0;
        return true;
    }
    int val{0};
    if(lexical_cast(input, val)) {
#if defined(__clang__)
/* on some older clang compilers */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
        output = val;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
        return true;
    }
    return false;
}

/// Assign a value converted from a string in lexical cast to the output value directly
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<!std::is_same<AssignTo, ConvertTo>::value && std::is_assignable<AssignTo &, ConvertTo &>::value,
                      detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    ConvertTo val{};
    bool parse_result = (!input.empty()) ? lexical_cast(input, val) : true;
    if(parse_result) {
        output = val;
    }
    return parse_result;
}

/// Assign a value from a lexical cast through constructing a value and move assigning it
template <
    typename AssignTo,
    typename ConvertTo,
    enable_if_t<!std::is_same<AssignTo, ConvertTo>::value && !std::is_assignable<AssignTo &, ConvertTo &>::value &&
                    std::is_move_assignable<AssignTo>::value,
                detail::enabler> = detail::dummy>
bool lexical_assign(const std::string &input, AssignTo &output) {
    ConvertTo val{};
    bool parse_result = input.empty() ? true : lexical_cast(input, val);
    if(parse_result) {
        output = AssignTo(val);  // use () form of constructor to allow some implicit conversions
    }
    return parse_result;
}

/// primary lexical conversion operation, 1 string to 1 type of some kind
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<classify_object<ConvertTo>::value <= object_category::other &&
                          classify_object<AssignTo>::value <= object_category::wrapper_value,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    return lexical_assign<AssignTo, ConvertTo>(strings[0], output);
}

/// Lexical conversion if there is only one element but the conversion type is for two, then call a two element
/// constructor
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<(type_count<AssignTo>::value <= 2) && expected_count<AssignTo>::value == 1 &&
                          is_tuple_like<ConvertTo>::value && type_count_base<ConvertTo>::value == 2,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    // the remove const is to handle pair types coming from a container
    using FirstType = typename std::remove_const<typename std::tuple_element<0, ConvertTo>::type>::type;
    using SecondType = typename std::tuple_element<1, ConvertTo>::type;
    FirstType v1;
    SecondType v2;
    bool retval = lexical_assign<FirstType, FirstType>(strings[0], v1);
    retval = retval && lexical_assign<SecondType, SecondType>((strings.size() > 1) ? strings[1] : std::string{}, v2);
    if(retval) {
        output = AssignTo{v1, v2};
    }
    return retval;
}

/// Lexical conversion of a container types of single elements
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                          type_count<ConvertTo>::value == 1,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    output.erase(output.begin(), output.end());
    if(strings.empty()) {
        return true;
    }
    if(strings.size() == 1 && strings[0] == "{}") {
        return true;
    }
    bool skip_remaining = false;
    if(strings.size() == 2 && strings[0] == "{}" && is_separator(strings[1])) {
        skip_remaining = true;
    }
    for(const auto &elem : strings) {
        typename AssignTo::value_type out;
        bool retval = lexical_assign<typename AssignTo::value_type, typename ConvertTo::value_type>(elem, out);
        if(!retval) {
            return false;
        }
        output.insert(output.end(), std::move(out));
        if(skip_remaining) {
            break;
        }
    }
    return (!output.empty());
}

/// Lexical conversion for complex types
template <class AssignTo, class ConvertTo, enable_if_t<is_complex<ConvertTo>::value, detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output) {

    if(strings.size() >= 2 && !strings[1].empty()) {
        using XC2 = typename wrapped_type<ConvertTo, double>::type;
        XC2 x{0.0}, y{0.0};
        auto str1 = strings[1];
        if(str1.back() == 'i' || str1.back() == 'j') {
            str1.pop_back();
        }
        auto worked = lexical_cast(strings[0], x) && lexical_cast(str1, y);
        if(worked) {
            output = ConvertTo{x, y};
        }
        return worked;
    }
    return lexical_assign<AssignTo, ConvertTo>(strings[0], output);
}

/// Conversion to a vector type using a particular single type as the conversion type
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && (expected_count<ConvertTo>::value == 1) &&
                          (type_count<ConvertTo>::value == 1),
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    bool retval = true;
    output.clear();
    output.reserve(strings.size());
    for(const auto &elem : strings) {

        output.emplace_back();
        retval = retval && lexical_assign<typename AssignTo::value_type, ConvertTo>(elem, output.back());
    }
    return (!output.empty()) && retval;
}

// forward declaration

/// Lexical conversion of a container types with conversion type of two elements
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                          type_count_base<ConvertTo>::value == 2,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(std::vector<std::string> strings, AssignTo &output);

/// Lexical conversion of a vector types with type_size >2 forward declaration
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                          type_count_base<ConvertTo>::value != 2 &&
                          ((type_count<ConvertTo>::value > 2) ||
                           (type_count<ConvertTo>::value > type_count_base<ConvertTo>::value)),
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output);

/// Conversion for tuples
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_tuple_like<AssignTo>::value && is_tuple_like<ConvertTo>::value &&
                          (type_count_base<ConvertTo>::value != type_count<ConvertTo>::value ||
                           type_count<ConvertTo>::value > 2),
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output);  // forward declaration

/// Conversion for operations where the assigned type is some class but the conversion is a mutable container or large
/// tuple
template <typename AssignTo,
          typename ConvertTo,
          enable_if_t<!is_tuple_like<AssignTo>::value && !is_mutable_container<AssignTo>::value &&
                          classify_object<ConvertTo>::value != object_category::wrapper_value &&
                          (is_mutable_container<ConvertTo>::value || type_count<ConvertTo>::value > 2),
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {

    if(strings.size() > 1 || (!strings.empty() && !(strings.front().empty()))) {
        ConvertTo val;
        auto retval = lexical_conversion<ConvertTo, ConvertTo>(strings, val);
        output = AssignTo{val};
        return retval;
    }
    output = AssignTo{};
    return true;
}

/// function template for converting tuples if the static Index is greater than the tuple size
template <class AssignTo, class ConvertTo, std::size_t I>
inline typename std::enable_if<(I >= type_count_base<AssignTo>::value), bool>::type
tuple_conversion(const std::vector<std::string> &, AssignTo &) {
    return true;
}

/// Conversion of a tuple element where the type size ==1 and not a mutable container
template <class AssignTo, class ConvertTo>
inline typename std::enable_if<!is_mutable_container<ConvertTo>::value && type_count<ConvertTo>::value == 1, bool>::type
tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output) {
    auto retval = lexical_assign<AssignTo, ConvertTo>(strings[0], output);
    strings.erase(strings.begin());
    return retval;
}

/// Conversion of a tuple element where the type size !=1 but the size is fixed and not a mutable container
template <class AssignTo, class ConvertTo>
inline typename std::enable_if<!is_mutable_container<ConvertTo>::value && (type_count<ConvertTo>::value > 1) &&
                                   type_count<ConvertTo>::value == type_count_min<ConvertTo>::value,
                               bool>::type
tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output) {
    auto retval = lexical_conversion<AssignTo, ConvertTo>(strings, output);
    strings.erase(strings.begin(), strings.begin() + type_count<ConvertTo>::value);
    return retval;
}

/// Conversion of a tuple element where the type is a mutable container or a type with different min and max type sizes
template <class AssignTo, class ConvertTo>
inline typename std::enable_if<is_mutable_container<ConvertTo>::value ||
                                   type_count<ConvertTo>::value != type_count_min<ConvertTo>::value,
                               bool>::type
tuple_type_conversion(std::vector<std::string> &strings, AssignTo &output) {

    std::size_t index{subtype_count_min<ConvertTo>::value};
    const std::size_t mx_count{subtype_count<ConvertTo>::value};
    const std::size_t mx{(std::min)(mx_count, strings.size() - 1)};

    while(index < mx) {
        if(is_separator(strings[index])) {
            break;
        }
        ++index;
    }
    bool retval = lexical_conversion<AssignTo, ConvertTo>(
        std::vector<std::string>(strings.begin(), strings.begin() + static_cast<std::ptrdiff_t>(index)), output);
    if(strings.size() > index) {
        strings.erase(strings.begin(), strings.begin() + static_cast<std::ptrdiff_t>(index) + 1);
    } else {
        strings.clear();
    }
    return retval;
}

/// Tuple conversion operation
template <class AssignTo, class ConvertTo, std::size_t I>
inline typename std::enable_if<(I < type_count_base<AssignTo>::value), bool>::type
tuple_conversion(std::vector<std::string> strings, AssignTo &output) {
    bool retval = true;
    using ConvertToElement = typename std::
        conditional<is_tuple_like<ConvertTo>::value, typename std::tuple_element<I, ConvertTo>::type, ConvertTo>::type;
    if(!strings.empty()) {
        retval = retval && tuple_type_conversion<typename std::tuple_element<I, AssignTo>::type, ConvertToElement>(
                               strings, std::get<I>(output));
    }
    retval = retval && tuple_conversion<AssignTo, ConvertTo, I + 1>(std::move(strings), output);
    return retval;
}

/// Lexical conversion of a container types with tuple elements of size 2
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                          type_count_base<ConvertTo>::value == 2,
                      detail::enabler>>
bool lexical_conversion(std::vector<std::string> strings, AssignTo &output) {
    output.clear();
    while(!strings.empty()) {

        typename std::remove_const<typename std::tuple_element<0, typename ConvertTo::value_type>::type>::type v1;
        typename std::tuple_element<1, typename ConvertTo::value_type>::type v2;
        bool retval = tuple_type_conversion<decltype(v1), decltype(v1)>(strings, v1);
        if(!strings.empty()) {
            retval = retval && tuple_type_conversion<decltype(v2), decltype(v2)>(strings, v2);
        }
        if(retval) {
            output.insert(output.end(), typename AssignTo::value_type{v1, v2});
        } else {
            return false;
        }
    }
    return (!output.empty());
}

/// lexical conversion of tuples with type count>2 or tuples of types of some element with a type size>=2
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_tuple_like<AssignTo>::value && is_tuple_like<ConvertTo>::value &&
                          (type_count_base<ConvertTo>::value != type_count<ConvertTo>::value ||
                           type_count<ConvertTo>::value > 2),
                      detail::enabler>>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    static_assert(
        !is_tuple_like<ConvertTo>::value || type_count_base<AssignTo>::value == type_count_base<ConvertTo>::value,
        "if the conversion type is defined as a tuple it must be the same size as the type you are converting to");
    return tuple_conversion<AssignTo, ConvertTo, 0>(strings, output);
}

/// Lexical conversion of a vector types for everything but tuples of two elements and types of size 1
template <class AssignTo,
          class ConvertTo,
          enable_if_t<is_mutable_container<AssignTo>::value && is_mutable_container<ConvertTo>::value &&
                          type_count_base<ConvertTo>::value != 2 &&
                          ((type_count<ConvertTo>::value > 2) ||
                           (type_count<ConvertTo>::value > type_count_base<ConvertTo>::value)),
                      detail::enabler>>
bool lexical_conversion(const std::vector<std ::string> &strings, AssignTo &output) {
    bool retval = true;
    output.clear();
    std::vector<std::string> temp;
    std::size_t ii{0};
    std::size_t icount{0};
    std::size_t xcm{type_count<ConvertTo>::value};
    auto ii_max = strings.size();
    while(ii < ii_max) {
        temp.push_back(strings[ii]);
        ++ii;
        ++icount;
        if(icount == xcm || is_separator(temp.back()) || ii == ii_max) {
            if(static_cast<int>(xcm) > type_count_min<ConvertTo>::value && is_separator(temp.back())) {
                temp.pop_back();
            }
            typename AssignTo::value_type temp_out;
            retval = retval &&
                     lexical_conversion<typename AssignTo::value_type, typename ConvertTo::value_type>(temp, temp_out);
            temp.clear();
            if(!retval) {
                return false;
            }
            output.insert(output.end(), std::move(temp_out));
            icount = 0;
        }
    }
    return retval;
}

/// conversion for wrapper types
template <typename AssignTo,
          class ConvertTo,
          enable_if_t<classify_object<ConvertTo>::value == object_category::wrapper_value &&
                          std::is_assignable<ConvertTo &, ConvertTo>::value,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output) {
    if(strings.empty() || strings.front().empty()) {
        output = ConvertTo{};
        return true;
    }
    typename ConvertTo::value_type val;
    if(lexical_conversion<typename ConvertTo::value_type, typename ConvertTo::value_type>(strings, val)) {
        output = ConvertTo{val};
        return true;
    }
    return false;
}

/// conversion for wrapper types
template <typename AssignTo,
          class ConvertTo,
          enable_if_t<classify_object<ConvertTo>::value == object_category::wrapper_value &&
                          !std::is_assignable<AssignTo &, ConvertTo>::value,
                      detail::enabler> = detail::dummy>
bool lexical_conversion(const std::vector<std::string> &strings, AssignTo &output) {
    using ConvertType = typename ConvertTo::value_type;
    if(strings.empty() || strings.front().empty()) {
        output = ConvertType{};
        return true;
    }
    ConvertType val;
    if(lexical_conversion<typename ConvertTo::value_type, typename ConvertTo::value_type>(strings, val)) {
        output = val;
        return true;
    }
    return false;
}

/// Sum a vector of strings
inline std::string sum_string_vector(const std::vector<std::string> &values) {
    double val{0.0};
    bool fail{false};
    std::string output;
    for(const auto &arg : values) {
        double tv{0.0};
        auto comp = lexical_cast(arg, tv);
        if(!comp) {
            errno = 0;
            auto fv = detail::to_flag_value(arg);
            fail = (errno != 0);
            if(fail) {
                break;
            }
            tv = static_cast<double>(fv);
        }
        val += tv;
    }
    if(fail) {
        for(const auto &arg : values) {
            output.append(arg);
        }
    } else {
        std::ostringstream out;
        out.precision(16);
        out << val;
        output = out.str();
    }
    return output;
}

}  // namespace detail
// [CLI11:type_tools_hpp:end]
}  // namespace CLI
