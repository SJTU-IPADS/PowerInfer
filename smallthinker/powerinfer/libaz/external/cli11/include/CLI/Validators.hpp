// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

#include "Error.hpp"
#include "Macros.hpp"
#include "StringTools.hpp"
#include "TypeTools.hpp"

// [CLI11:public_includes:set]
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

// [CLI11:validators_hpp_filesystem:verbatim]

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0
#include <filesystem>  // NOLINT(build/include)
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

// [CLI11:validators_hpp_filesystem:end]

namespace CLI {
// [CLI11:validators_hpp:verbatim]

class Option;

/// @defgroup validator_group Validators

/// @brief Some validators that are provided
///
/// These are simple `std::string(const std::string&)` validators that are useful. They return
/// a string if the validation fails. A custom struct is provided, as well, with the same user
/// semantics, but with the ability to provide a new type name.
/// @{

///
class Validator {
  protected:
    /// This is the description function, if empty the description_ will be used
    std::function<std::string()> desc_function_{[]() { return std::string{}; }};

    /// This is the base function that is to be called.
    /// Returns a string error message if validation fails.
    std::function<std::string(std::string &)> func_{[](std::string &) { return std::string{}; }};
    /// The name for search purposes of the Validator
    std::string name_{};
    /// A Validator will only apply to an indexed value (-1 is all elements)
    int application_index_ = -1;
    /// Enable for Validator to allow it to be disabled if need be
    bool active_{true};
    /// specify that a validator should not modify the input
    bool non_modifying_{false};

    Validator(std::string validator_desc, std::function<std::string(std::string &)> func)
        : desc_function_([validator_desc]() { return validator_desc; }), func_(std::move(func)) {}

  public:
    Validator() = default;
    /// Construct a Validator with just the description string
    explicit Validator(std::string validator_desc) : desc_function_([validator_desc]() { return validator_desc; }) {}
    /// Construct Validator from basic information
    Validator(std::function<std::string(std::string &)> op, std::string validator_desc, std::string validator_name = "")
        : desc_function_([validator_desc]() { return validator_desc; }), func_(std::move(op)),
          name_(std::move(validator_name)) {}
    /// Set the Validator operation function
    Validator &operation(std::function<std::string(std::string &)> op) {
        func_ = std::move(op);
        return *this;
    }
    /// This is the required operator for a Validator - provided to help
    /// users (CLI11 uses the member `func` directly)
    std::string operator()(std::string &str) const;

    /// This is the required operator for a Validator - provided to help
    /// users (CLI11 uses the member `func` directly)
    std::string operator()(const std::string &str) const {
        std::string value = str;
        return (active_) ? func_(value) : std::string{};
    }

    /// Specify the type string
    Validator &description(std::string validator_desc) {
        desc_function_ = [validator_desc]() { return validator_desc; };
        return *this;
    }
    /// Specify the type string
    CLI11_NODISCARD Validator description(std::string validator_desc) const;

    /// Generate type description information for the Validator
    CLI11_NODISCARD std::string get_description() const {
        if(active_) {
            return desc_function_();
        }
        return std::string{};
    }
    /// Specify the type string
    Validator &name(std::string validator_name) {
        name_ = std::move(validator_name);
        return *this;
    }
    /// Specify the type string
    CLI11_NODISCARD Validator name(std::string validator_name) const {
        Validator newval(*this);
        newval.name_ = std::move(validator_name);
        return newval;
    }
    /// Get the name of the Validator
    CLI11_NODISCARD const std::string &get_name() const { return name_; }
    /// Specify whether the Validator is active or not
    Validator &active(bool active_val = true) {
        active_ = active_val;
        return *this;
    }
    /// Specify whether the Validator is active or not
    CLI11_NODISCARD Validator active(bool active_val = true) const {
        Validator newval(*this);
        newval.active_ = active_val;
        return newval;
    }

    /// Specify whether the Validator can be modifying or not
    Validator &non_modifying(bool no_modify = true) {
        non_modifying_ = no_modify;
        return *this;
    }
    /// Specify the application index of a validator
    Validator &application_index(int app_index) {
        application_index_ = app_index;
        return *this;
    }
    /// Specify the application index of a validator
    CLI11_NODISCARD Validator application_index(int app_index) const {
        Validator newval(*this);
        newval.application_index_ = app_index;
        return newval;
    }
    /// Get the current value of the application index
    CLI11_NODISCARD int get_application_index() const { return application_index_; }
    /// Get a boolean if the validator is active
    CLI11_NODISCARD bool get_active() const { return active_; }

    /// Get a boolean if the validator is allowed to modify the input returns true if it can modify the input
    CLI11_NODISCARD bool get_modifying() const { return !non_modifying_; }

    /// Combining validators is a new validator. Type comes from left validator if function, otherwise only set if the
    /// same.
    Validator operator&(const Validator &other) const;

    /// Combining validators is a new validator. Type comes from left validator if function, otherwise only set if the
    /// same.
    Validator operator|(const Validator &other) const;

    /// Create a validator that fails when a given validator succeeds
    Validator operator!() const;

  private:
    void _merge_description(const Validator &val1, const Validator &val2, const std::string &merger);
};

/// Class wrapping some of the accessors of Validator
class CustomValidator : public Validator {
  public:
};
// The implementation of the built in validators is using the Validator class;
// the user is only expected to use the const (static) versions (since there's no setup).
// Therefore, this is in detail.
namespace detail {

/// CLI enumeration of different file types
enum class path_type { nonexistent, file, directory };

/// get the type of the path from a file name
CLI11_INLINE path_type check_path(const char *file) noexcept;

/// Check for an existing file (returns error message if check fails)
class ExistingFileValidator : public Validator {
  public:
    ExistingFileValidator();
};

/// Check for an existing directory (returns error message if check fails)
class ExistingDirectoryValidator : public Validator {
  public:
    ExistingDirectoryValidator();
};

/// Check for an existing path
class ExistingPathValidator : public Validator {
  public:
    ExistingPathValidator();
};

/// Check for an non-existing path
class NonexistentPathValidator : public Validator {
  public:
    NonexistentPathValidator();
};

/// Validate the given string is a legal ipv4 address
class IPV4Validator : public Validator {
  public:
    IPV4Validator();
};

class EscapedStringTransformer : public Validator {
  public:
    EscapedStringTransformer();
};

}  // namespace detail

// Static is not needed here, because global const implies static.

/// Check for existing file (returns error message if check fails)
const detail::ExistingFileValidator ExistingFile;

/// Check for an existing directory (returns error message if check fails)
const detail::ExistingDirectoryValidator ExistingDirectory;

/// Check for an existing path
const detail::ExistingPathValidator ExistingPath;

/// Check for an non-existing path
const detail::NonexistentPathValidator NonexistentPath;

/// Check for an IP4 address
const detail::IPV4Validator ValidIPV4;

/// convert escaped characters into their associated values
const detail::EscapedStringTransformer EscapedString;

/// Validate the input as a particular type
template <typename DesiredType> class TypeValidator : public Validator {
  public:
    explicit TypeValidator(const std::string &validator_name)
        : Validator(validator_name, [](std::string &input_string) {
              using CLI::detail::lexical_cast;
              auto val = DesiredType();
              if(!lexical_cast(input_string, val)) {
                  return std::string("Failed parsing ") + input_string + " as a " + detail::type_name<DesiredType>();
              }
              return std::string();
          }) {}
    TypeValidator() : TypeValidator(detail::type_name<DesiredType>()) {}
};

/// Check for a number
const TypeValidator<double> Number("NUMBER");

/// Modify a path if the file is a particular default location, can be used as Check or transform
/// with the error return optionally disabled
class FileOnDefaultPath : public Validator {
  public:
    explicit FileOnDefaultPath(std::string default_path, bool enableErrorReturn = true);
};

/// Produce a range (factory). Min and max are inclusive.
class Range : public Validator {
  public:
    /// This produces a range with min and max inclusive.
    ///
    /// Note that the constructor is templated, but the struct is not, so C++17 is not
    /// needed to provide nice syntax for Range(a,b).
    template <typename T>
    Range(T min_val, T max_val, const std::string &validator_name = std::string{}) : Validator(validator_name) {
        if(validator_name.empty()) {
            std::stringstream out;
            out << detail::type_name<T>() << " in [" << min_val << " - " << max_val << "]";
            description(out.str());
        }

        func_ = [min_val, max_val](std::string &input) {
            using CLI::detail::lexical_cast;
            T val;
            bool converted = lexical_cast(input, val);
            if((!converted) || (val < min_val || val > max_val)) {
                std::stringstream out;
                out << "Value " << input << " not in range [";
                out << min_val << " - " << max_val << "]";
                return out.str();
            }
            return std::string{};
        };
    }

    /// Range of one value is 0 to value
    template <typename T>
    explicit Range(T max_val, const std::string &validator_name = std::string{})
        : Range(static_cast<T>(0), max_val, validator_name) {}
};

/// Check for a non negative number
const Range NonNegativeNumber((std::numeric_limits<double>::max)(), "NONNEGATIVE");

/// Check for a positive valued number (val>0.0), <double>::min  here is the smallest positive number
const Range PositiveNumber((std::numeric_limits<double>::min)(), (std::numeric_limits<double>::max)(), "POSITIVE");

/// Produce a bounded range (factory). Min and max are inclusive.
class Bound : public Validator {
  public:
    /// This bounds a value with min and max inclusive.
    ///
    /// Note that the constructor is templated, but the struct is not, so C++17 is not
    /// needed to provide nice syntax for Range(a,b).
    template <typename T> Bound(T min_val, T max_val) {
        std::stringstream out;
        out << detail::type_name<T>() << " bounded to [" << min_val << " - " << max_val << "]";
        description(out.str());

        func_ = [min_val, max_val](std::string &input) {
            using CLI::detail::lexical_cast;
            T val;
            bool converted = lexical_cast(input, val);
            if(!converted) {
                return std::string("Value ") + input + " could not be converted";
            }
            if(val < min_val)
                input = detail::to_string(min_val);
            else if(val > max_val)
                input = detail::to_string(max_val);

            return std::string{};
        };
    }

    /// Range of one value is 0 to value
    template <typename T> explicit Bound(T max_val) : Bound(static_cast<T>(0), max_val) {}
};

namespace detail {
template <typename T,
          enable_if_t<is_copyable_ptr<typename std::remove_reference<T>::type>::value, detail::enabler> = detail::dummy>
auto smart_deref(T value) -> decltype(*value) {
    return *value;
}

template <
    typename T,
    enable_if_t<!is_copyable_ptr<typename std::remove_reference<T>::type>::value, detail::enabler> = detail::dummy>
typename std::remove_reference<T>::type &smart_deref(T &value) {
    return value;
}
/// Generate a string representation of a set
template <typename T> std::string generate_set(const T &set) {
    using element_t = typename detail::element_type<T>::type;
    using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;  // the type of the object pair
    std::string out(1, '{');
    out.append(detail::join(
        detail::smart_deref(set),
        [](const iteration_type_t &v) { return detail::pair_adaptor<element_t>::first(v); },
        ","));
    out.push_back('}');
    return out;
}

/// Generate a string representation of a map
template <typename T> std::string generate_map(const T &map, bool key_only = false) {
    using element_t = typename detail::element_type<T>::type;
    using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;  // the type of the object pair
    std::string out(1, '{');
    out.append(detail::join(
        detail::smart_deref(map),
        [key_only](const iteration_type_t &v) {
            std::string res{detail::to_string(detail::pair_adaptor<element_t>::first(v))};

            if(!key_only) {
                res.append("->");
                res += detail::to_string(detail::pair_adaptor<element_t>::second(v));
            }
            return res;
        },
        ","));
    out.push_back('}');
    return out;
}

template <typename C, typename V> struct has_find {
    template <typename CC, typename VV>
    static auto test(int) -> decltype(std::declval<CC>().find(std::declval<VV>()), std::true_type());
    template <typename, typename> static auto test(...) -> decltype(std::false_type());

    static const auto value = decltype(test<C, V>(0))::value;
    using type = std::integral_constant<bool, value>;
};

/// A search function
template <typename T, typename V, enable_if_t<!has_find<T, V>::value, detail::enabler> = detail::dummy>
auto search(const T &set, const V &val) -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))> {
    using element_t = typename detail::element_type<T>::type;
    auto &setref = detail::smart_deref(set);
    auto it = std::find_if(std::begin(setref), std::end(setref), [&val](decltype(*std::begin(setref)) v) {
        return (detail::pair_adaptor<element_t>::first(v) == val);
    });
    return {(it != std::end(setref)), it};
}

/// A search function that uses the built in find function
template <typename T, typename V, enable_if_t<has_find<T, V>::value, detail::enabler> = detail::dummy>
auto search(const T &set, const V &val) -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))> {
    auto &setref = detail::smart_deref(set);
    auto it = setref.find(val);
    return {(it != std::end(setref)), it};
}

/// A search function with a filter function
template <typename T, typename V>
auto search(const T &set, const V &val, const std::function<V(V)> &filter_function)
    -> std::pair<bool, decltype(std::begin(detail::smart_deref(set)))> {
    using element_t = typename detail::element_type<T>::type;
    // do the potentially faster first search
    auto res = search(set, val);
    if((res.first) || (!(filter_function))) {
        return res;
    }
    // if we haven't found it do the longer linear search with all the element translations
    auto &setref = detail::smart_deref(set);
    auto it = std::find_if(std::begin(setref), std::end(setref), [&](decltype(*std::begin(setref)) v) {
        V a{detail::pair_adaptor<element_t>::first(v)};
        a = filter_function(a);
        return (a == val);
    });
    return {(it != std::end(setref)), it};
}

// the following suggestion was made by Nikita Ofitserov(@himikof)
// done in templates to prevent compiler warnings on negation of unsigned numbers

/// Do a check for overflow on signed numbers
template <typename T>
inline typename std::enable_if<std::is_signed<T>::value, T>::type overflowCheck(const T &a, const T &b) {
    if((a > 0) == (b > 0)) {
        return ((std::numeric_limits<T>::max)() / (std::abs)(a) < (std::abs)(b));
    }
    return ((std::numeric_limits<T>::min)() / (std::abs)(a) > -(std::abs)(b));
}
/// Do a check for overflow on unsigned numbers
template <typename T>
inline typename std::enable_if<!std::is_signed<T>::value, T>::type overflowCheck(const T &a, const T &b) {
    return ((std::numeric_limits<T>::max)() / a < b);
}

/// Performs a *= b; if it doesn't cause integer overflow. Returns false otherwise.
template <typename T> typename std::enable_if<std::is_integral<T>::value, bool>::type checked_multiply(T &a, T b) {
    if(a == 0 || b == 0 || a == 1 || b == 1) {
        a *= b;
        return true;
    }
    if(a == (std::numeric_limits<T>::min)() || b == (std::numeric_limits<T>::min)()) {
        return false;
    }
    if(overflowCheck(a, b)) {
        return false;
    }
    a *= b;
    return true;
}

/// Performs a *= b; if it doesn't equal infinity. Returns false otherwise.
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, bool>::type checked_multiply(T &a, T b) {
    T c = a * b;
    if(std::isinf(c) && !std::isinf(a) && !std::isinf(b)) {
        return false;
    }
    a = c;
    return true;
}

}  // namespace detail
/// Verify items are in a set
class IsMember : public Validator {
  public:
    using filter_fn_t = std::function<std::string(std::string)>;

    /// This allows in-place construction using an initializer list
    template <typename T, typename... Args>
    IsMember(std::initializer_list<T> values, Args &&...args)
        : IsMember(std::vector<T>(values), std::forward<Args>(args)...) {}

    /// This checks to see if an item is in a set (empty function)
    template <typename T> explicit IsMember(T &&set) : IsMember(std::forward<T>(set), nullptr) {}

    /// This checks to see if an item is in a set: pointer or copy version. You can pass in a function that will filter
    /// both sides of the comparison before computing the comparison.
    template <typename T, typename F> explicit IsMember(T set, F filter_function) {

        // Get the type of the contained item - requires a container have ::value_type
        // if the type does not have first_type and second_type, these are both value_type
        using element_t = typename detail::element_type<T>::type;             // Removes (smart) pointers if needed
        using item_t = typename detail::pair_adaptor<element_t>::first_type;  // Is value_type if not a map

        using local_item_t = typename IsMemberType<item_t>::type;  // This will convert bad types to good ones
                                                                   // (const char * to std::string)

        // Make a local copy of the filter function, using a std::function if not one already
        std::function<local_item_t(local_item_t)> filter_fn = filter_function;

        // This is the type name for help, it will take the current version of the set contents
        desc_function_ = [set]() { return detail::generate_set(detail::smart_deref(set)); };

        // This is the function that validates
        // It stores a copy of the set pointer-like, so shared_ptr will stay alive
        func_ = [set, filter_fn](std::string &input) {
            using CLI::detail::lexical_cast;
            local_item_t b;
            if(!lexical_cast(input, b)) {
                throw ValidationError(input);  // name is added later
            }
            if(filter_fn) {
                b = filter_fn(b);
            }
            auto res = detail::search(set, b, filter_fn);
            if(res.first) {
                // Make sure the version in the input string is identical to the one in the set
                if(filter_fn) {
                    input = detail::value_string(detail::pair_adaptor<element_t>::first(*(res.second)));
                }

                // Return empty error string (success)
                return std::string{};
            }

            // If you reach this point, the result was not found
            return input + " not in " + detail::generate_set(detail::smart_deref(set));
        };
    }

    /// You can pass in as many filter functions as you like, they nest (string only currently)
    template <typename T, typename... Args>
    IsMember(T &&set, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, Args &&...other)
        : IsMember(
              std::forward<T>(set),
              [filter_fn_1, filter_fn_2](std::string a) { return filter_fn_2(filter_fn_1(a)); },
              other...) {}
};

/// definition of the default transformation object
template <typename T> using TransformPairs = std::vector<std::pair<std::string, T>>;

/// Translate named items to other or a value set
class Transformer : public Validator {
  public:
    using filter_fn_t = std::function<std::string(std::string)>;

    /// This allows in-place construction
    template <typename... Args>
    Transformer(std::initializer_list<std::pair<std::string, std::string>> values, Args &&...args)
        : Transformer(TransformPairs<std::string>(values), std::forward<Args>(args)...) {}

    /// direct map of std::string to std::string
    template <typename T> explicit Transformer(T &&mapping) : Transformer(std::forward<T>(mapping), nullptr) {}

    /// This checks to see if an item is in a set: pointer or copy version. You can pass in a function that will filter
    /// both sides of the comparison before computing the comparison.
    template <typename T, typename F> explicit Transformer(T mapping, F filter_function) {

        static_assert(detail::pair_adaptor<typename detail::element_type<T>::type>::value,
                      "mapping must produce value pairs");
        // Get the type of the contained item - requires a container have ::value_type
        // if the type does not have first_type and second_type, these are both value_type
        using element_t = typename detail::element_type<T>::type;             // Removes (smart) pointers if needed
        using item_t = typename detail::pair_adaptor<element_t>::first_type;  // Is value_type if not a map
        using local_item_t = typename IsMemberType<item_t>::type;             // Will convert bad types to good ones
                                                                              // (const char * to std::string)

        // Make a local copy of the filter function, using a std::function if not one already
        std::function<local_item_t(local_item_t)> filter_fn = filter_function;

        // This is the type name for help, it will take the current version of the set contents
        desc_function_ = [mapping]() { return detail::generate_map(detail::smart_deref(mapping)); };

        func_ = [mapping, filter_fn](std::string &input) {
            using CLI::detail::lexical_cast;
            local_item_t b;
            if(!lexical_cast(input, b)) {
                return std::string();
                // there is no possible way we can match anything in the mapping if we can't convert so just return
            }
            if(filter_fn) {
                b = filter_fn(b);
            }
            auto res = detail::search(mapping, b, filter_fn);
            if(res.first) {
                input = detail::value_string(detail::pair_adaptor<element_t>::second(*res.second));
            }
            return std::string{};
        };
    }

    /// You can pass in as many filter functions as you like, they nest
    template <typename T, typename... Args>
    Transformer(T &&mapping, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, Args &&...other)
        : Transformer(
              std::forward<T>(mapping),
              [filter_fn_1, filter_fn_2](std::string a) { return filter_fn_2(filter_fn_1(a)); },
              other...) {}
};

/// translate named items to other or a value set
class CheckedTransformer : public Validator {
  public:
    using filter_fn_t = std::function<std::string(std::string)>;

    /// This allows in-place construction
    template <typename... Args>
    CheckedTransformer(std::initializer_list<std::pair<std::string, std::string>> values, Args &&...args)
        : CheckedTransformer(TransformPairs<std::string>(values), std::forward<Args>(args)...) {}

    /// direct map of std::string to std::string
    template <typename T> explicit CheckedTransformer(T mapping) : CheckedTransformer(std::move(mapping), nullptr) {}

    /// This checks to see if an item is in a set: pointer or copy version. You can pass in a function that will filter
    /// both sides of the comparison before computing the comparison.
    template <typename T, typename F> explicit CheckedTransformer(T mapping, F filter_function) {

        static_assert(detail::pair_adaptor<typename detail::element_type<T>::type>::value,
                      "mapping must produce value pairs");
        // Get the type of the contained item - requires a container have ::value_type
        // if the type does not have first_type and second_type, these are both value_type
        using element_t = typename detail::element_type<T>::type;             // Removes (smart) pointers if needed
        using item_t = typename detail::pair_adaptor<element_t>::first_type;  // Is value_type if not a map
        using local_item_t = typename IsMemberType<item_t>::type;             // Will convert bad types to good ones
                                                                              // (const char * to std::string)
        using iteration_type_t = typename detail::pair_adaptor<element_t>::value_type;  // the type of the object pair

        // Make a local copy of the filter function, using a std::function if not one already
        std::function<local_item_t(local_item_t)> filter_fn = filter_function;

        auto tfunc = [mapping]() {
            std::string out("value in ");
            out += detail::generate_map(detail::smart_deref(mapping)) + " OR {";
            out += detail::join(
                detail::smart_deref(mapping),
                [](const iteration_type_t &v) { return detail::to_string(detail::pair_adaptor<element_t>::second(v)); },
                ",");
            out.push_back('}');
            return out;
        };

        desc_function_ = tfunc;

        func_ = [mapping, tfunc, filter_fn](std::string &input) {
            using CLI::detail::lexical_cast;
            local_item_t b;
            bool converted = lexical_cast(input, b);
            if(converted) {
                if(filter_fn) {
                    b = filter_fn(b);
                }
                auto res = detail::search(mapping, b, filter_fn);
                if(res.first) {
                    input = detail::value_string(detail::pair_adaptor<element_t>::second(*res.second));
                    return std::string{};
                }
            }
            for(const auto &v : detail::smart_deref(mapping)) {
                auto output_string = detail::value_string(detail::pair_adaptor<element_t>::second(v));
                if(output_string == input) {
                    return std::string();
                }
            }

            return "Check " + input + " " + tfunc() + " FAILED";
        };
    }

    /// You can pass in as many filter functions as you like, they nest
    template <typename T, typename... Args>
    CheckedTransformer(T &&mapping, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, Args &&...other)
        : CheckedTransformer(
              std::forward<T>(mapping),
              [filter_fn_1, filter_fn_2](std::string a) { return filter_fn_2(filter_fn_1(a)); },
              other...) {}
};

/// Helper function to allow ignore_case to be passed to IsMember or Transform
inline std::string ignore_case(std::string item) { return detail::to_lower(item); }

/// Helper function to allow ignore_underscore to be passed to IsMember or Transform
inline std::string ignore_underscore(std::string item) { return detail::remove_underscore(item); }

/// Helper function to allow checks to ignore spaces to be passed to IsMember or Transform
inline std::string ignore_space(std::string item) {
    item.erase(std::remove(std::begin(item), std::end(item), ' '), std::end(item));
    item.erase(std::remove(std::begin(item), std::end(item), '\t'), std::end(item));
    return item;
}

/// Multiply a number by a factor using given mapping.
/// Can be used to write transforms for SIZE or DURATION inputs.
///
/// Example:
///   With mapping = `{"b"->1, "kb"->1024, "mb"->1024*1024}`
///   one can recognize inputs like "100", "12kb", "100 MB",
///   that will be automatically transformed to 100, 14448, 104857600.
///
/// Output number type matches the type in the provided mapping.
/// Therefore, if it is required to interpret real inputs like "0.42 s",
/// the mapping should be of a type <string, float> or <string, double>.
class AsNumberWithUnit : public Validator {
  public:
    /// Adjust AsNumberWithUnit behavior.
    /// CASE_SENSITIVE/CASE_INSENSITIVE controls how units are matched.
    /// UNIT_OPTIONAL/UNIT_REQUIRED throws ValidationError
    ///   if UNIT_REQUIRED is set and unit literal is not found.
    enum Options {
        CASE_SENSITIVE = 0,
        CASE_INSENSITIVE = 1,
        UNIT_OPTIONAL = 0,
        UNIT_REQUIRED = 2,
        DEFAULT = CASE_INSENSITIVE | UNIT_OPTIONAL
    };

    template <typename Number>
    explicit AsNumberWithUnit(std::map<std::string, Number> mapping,
                              Options opts = DEFAULT,
                              const std::string &unit_name = "UNIT") {
        description(generate_description<Number>(unit_name, opts));
        validate_mapping(mapping, opts);

        // transform function
        func_ = [mapping, opts](std::string &input) -> std::string {
            Number num{};

            detail::rtrim(input);
            if(input.empty()) {
                throw ValidationError("Input is empty");
            }

            // Find split position between number and prefix
            auto unit_begin = input.end();
            while(unit_begin > input.begin() && std::isalpha(*(unit_begin - 1), std::locale())) {
                --unit_begin;
            }

            std::string unit{unit_begin, input.end()};
            input.resize(static_cast<std::size_t>(std::distance(input.begin(), unit_begin)));
            detail::trim(input);

            if(opts & UNIT_REQUIRED && unit.empty()) {
                throw ValidationError("Missing mandatory unit");
            }
            if(opts & CASE_INSENSITIVE) {
                unit = detail::to_lower(unit);
            }
            if(unit.empty()) {
                using CLI::detail::lexical_cast;
                if(!lexical_cast(input, num)) {
                    throw ValidationError(std::string("Value ") + input + " could not be converted to " +
                                          detail::type_name<Number>());
                }
                // No need to modify input if no unit passed
                return {};
            }

            // find corresponding factor
            auto it = mapping.find(unit);
            if(it == mapping.end()) {
                throw ValidationError(unit +
                                      " unit not recognized. "
                                      "Allowed values: " +
                                      detail::generate_map(mapping, true));
            }

            if(!input.empty()) {
                using CLI::detail::lexical_cast;
                bool converted = lexical_cast(input, num);
                if(!converted) {
                    throw ValidationError(std::string("Value ") + input + " could not be converted to " +
                                          detail::type_name<Number>());
                }
                // perform safe multiplication
                bool ok = detail::checked_multiply(num, it->second);
                if(!ok) {
                    throw ValidationError(detail::to_string(num) + " multiplied by " + unit +
                                          " factor would cause number overflow. Use smaller value.");
                }
            } else {
                num = static_cast<Number>(it->second);
            }

            input = detail::to_string(num);

            return {};
        };
    }

  private:
    /// Check that mapping contains valid units.
    /// Update mapping for CASE_INSENSITIVE mode.
    template <typename Number> static void validate_mapping(std::map<std::string, Number> &mapping, Options opts) {
        for(auto &kv : mapping) {
            if(kv.first.empty()) {
                throw ValidationError("Unit must not be empty.");
            }
            if(!detail::isalpha(kv.first)) {
                throw ValidationError("Unit must contain only letters.");
            }
        }

        // make all units lowercase if CASE_INSENSITIVE
        if(opts & CASE_INSENSITIVE) {
            std::map<std::string, Number> lower_mapping;
            for(auto &kv : mapping) {
                auto s = detail::to_lower(kv.first);
                if(lower_mapping.count(s)) {
                    throw ValidationError(std::string("Several matching lowercase unit representations are found: ") +
                                          s);
                }
                lower_mapping[detail::to_lower(kv.first)] = kv.second;
            }
            mapping = std::move(lower_mapping);
        }
    }

    /// Generate description like this: NUMBER [UNIT]
    template <typename Number> static std::string generate_description(const std::string &name, Options opts) {
        std::stringstream out;
        out << detail::type_name<Number>() << ' ';
        if(opts & UNIT_REQUIRED) {
            out << name;
        } else {
            out << '[' << name << ']';
        }
        return out.str();
    }
};

inline AsNumberWithUnit::Options operator|(const AsNumberWithUnit::Options &a, const AsNumberWithUnit::Options &b) {
    return static_cast<AsNumberWithUnit::Options>(static_cast<int>(a) | static_cast<int>(b));
}

/// Converts a human-readable size string (with unit literal) to uin64_t size.
/// Example:
///   "100" => 100
///   "1 b" => 100
///   "10Kb" => 10240 // you can configure this to be interpreted as kilobyte (*1000) or kibibyte (*1024)
///   "10 KB" => 10240
///   "10 kb" => 10240
///   "10 kib" => 10240 // *i, *ib are always interpreted as *bibyte (*1024)
///   "10kb" => 10240
///   "2 MB" => 2097152
///   "2 EiB" => 2^61 // Units up to exibyte are supported
class AsSizeValue : public AsNumberWithUnit {
  public:
    using result_t = std::uint64_t;

    /// If kb_is_1000 is true,
    /// interpret 'kb', 'k' as 1000 and 'kib', 'ki' as 1024
    /// (same applies to higher order units as well).
    /// Otherwise, interpret all literals as factors of 1024.
    /// The first option is formally correct, but
    /// the second interpretation is more wide-spread
    /// (see https://en.wikipedia.org/wiki/Binary_prefix).
    explicit AsSizeValue(bool kb_is_1000);

  private:
    /// Get <size unit, factor> mapping
    static std::map<std::string, result_t> init_mapping(bool kb_is_1000);

    /// Cache calculated mapping
    static std::map<std::string, result_t> get_mapping(bool kb_is_1000);
};

namespace detail {
/// Split a string into a program name and command line arguments
/// the string is assumed to contain a file name followed by other arguments
/// the return value contains is a pair with the first argument containing the program name and the second
/// everything else.
CLI11_INLINE std::pair<std::string, std::string> split_program_name(std::string commandline);

}  // namespace detail
/// @}

// [CLI11:validators_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Validators_inl.hpp"  // IWYU pragma: export
#endif
