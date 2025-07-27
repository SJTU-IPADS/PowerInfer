// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../Encoding.hpp"
#include "../Macros.hpp"

// [CLI11:public_includes:set]
#include <array>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <locale>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
// [CLI11:public_includes:end]

namespace CLI {
// [CLI11:encoding_inl_hpp:verbatim]

namespace detail {

#if !CLI11_HAS_CODECVT
/// Attempt to set one of the acceptable unicode locales for conversion
CLI11_INLINE void set_unicode_locale() {
    static const std::array<const char *, 3> unicode_locales{{"C.UTF-8", "en_US.UTF-8", ".UTF-8"}};

    for(const auto &locale_name : unicode_locales) {
        if(std::setlocale(LC_ALL, locale_name) != nullptr) {
            return;
        }
    }
    throw std::runtime_error("CLI::narrow: could not set locale to C.UTF-8");
}

template <typename F> struct scope_guard_t {
    F closure;

    explicit scope_guard_t(F closure_) : closure(closure_) {}
    ~scope_guard_t() { closure(); }
};

template <typename F> CLI11_NODISCARD CLI11_INLINE scope_guard_t<F> scope_guard(F &&closure) {
    return scope_guard_t<F>{std::forward<F>(closure)};
}

#endif  // !CLI11_HAS_CODECVT

CLI11_DIAGNOSTIC_PUSH
CLI11_DIAGNOSTIC_IGNORE_DEPRECATED

CLI11_INLINE std::string narrow_impl(const wchar_t *str, std::size_t str_size) {
#if CLI11_HAS_CODECVT
#ifdef _WIN32
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(str, str + str_size);

#else
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(str, str + str_size);

#endif  // _WIN32
#else   // CLI11_HAS_CODECVT
    (void)str_size;
    std::mbstate_t state = std::mbstate_t();
    const wchar_t *it = str;

    std::string old_locale = std::setlocale(LC_ALL, nullptr);
    auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
    set_unicode_locale();

    std::size_t new_size = std::wcsrtombs(nullptr, &it, 0, &state);
    if(new_size == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("CLI::narrow: conversion error in std::wcsrtombs at offset " +
                                 std::to_string(it - str));
    }
    std::string result(new_size, '\0');
    std::wcsrtombs(const_cast<char *>(result.data()), &str, new_size, &state);

    return result;

#endif  // CLI11_HAS_CODECVT
}

CLI11_INLINE std::wstring widen_impl(const char *str, std::size_t str_size) {
#if CLI11_HAS_CODECVT
#ifdef _WIN32
    return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(str, str + str_size);

#else
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str, str + str_size);

#endif  // _WIN32
#else   // CLI11_HAS_CODECVT
    (void)str_size;
    std::mbstate_t state = std::mbstate_t();
    const char *it = str;

    std::string old_locale = std::setlocale(LC_ALL, nullptr);
    auto sg = scope_guard([&] { std::setlocale(LC_ALL, old_locale.c_str()); });
    set_unicode_locale();

    std::size_t new_size = std::mbsrtowcs(nullptr, &it, 0, &state);
    if(new_size == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("CLI::widen: conversion error in std::mbsrtowcs at offset " +
                                 std::to_string(it - str));
    }
    std::wstring result(new_size, L'\0');
    std::mbsrtowcs(const_cast<wchar_t *>(result.data()), &str, new_size, &state);

    return result;

#endif  // CLI11_HAS_CODECVT
}

CLI11_DIAGNOSTIC_POP

}  // namespace detail

CLI11_INLINE std::string narrow(const wchar_t *str, std::size_t str_size) { return detail::narrow_impl(str, str_size); }
CLI11_INLINE std::string narrow(const std::wstring &str) { return detail::narrow_impl(str.data(), str.size()); }
// Flawfinder: ignore
CLI11_INLINE std::string narrow(const wchar_t *str) { return detail::narrow_impl(str, std::wcslen(str)); }

CLI11_INLINE std::wstring widen(const char *str, std::size_t str_size) { return detail::widen_impl(str, str_size); }
CLI11_INLINE std::wstring widen(const std::string &str) { return detail::widen_impl(str.data(), str.size()); }
// Flawfinder: ignore
CLI11_INLINE std::wstring widen(const char *str) { return detail::widen_impl(str, std::strlen(str)); }

#ifdef CLI11_CPP17
CLI11_INLINE std::string narrow(std::wstring_view str) { return detail::narrow_impl(str.data(), str.size()); }
CLI11_INLINE std::wstring widen(std::string_view str) { return detail::widen_impl(str.data(), str.size()); }
#endif  // CLI11_CPP17

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0
CLI11_INLINE std::filesystem::path to_path(std::string_view str) {
    return std::filesystem::path{
#ifdef _WIN32
        widen(str)
#else
        str
#endif  // _WIN32
    };
}
#endif  // CLI11_HAS_FILESYSTEM

// [CLI11:encoding_inl_hpp:end]
}  // namespace CLI
