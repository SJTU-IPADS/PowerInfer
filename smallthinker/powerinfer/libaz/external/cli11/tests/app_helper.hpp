// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#ifdef CLI11_SINGLE_FILE
#include "CLI11.hpp"
#else
#include "CLI/CLI.hpp"
#endif

#include "catch.hpp"
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using input_t = std::vector<std::string>;

class TApp {
  public:
    CLI::App app{"My Test Program"};
    input_t args{};
    virtual ~TApp() = default;
    void run() {
        // It is okay to re-parse - clear is called automatically before a parse.
        input_t newargs = args;
        std::reverse(std::begin(newargs), std::end(newargs));
        app.parse(newargs);
    }
};

CLI11_INLINE int fileClear(const std::string &name) { return std::remove(name.c_str()); }

class TempFile {
    std::string _name{};

  public:
    explicit TempFile(std::string name) : _name(std::move(name)) {
        if(!CLI::NonexistentPath(_name).empty())
            throw std::runtime_error(_name);
    }

    ~TempFile() {
        std::remove(_name.c_str());  // Doesn't matter if returns 0 or not
    }

    operator const std::string &() const { return _name; }  // NOLINT(google-explicit-constructor)
    CLI11_NODISCARD const char *c_str() const { return _name.c_str(); }
};

inline void put_env(std::string name, std::string value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

inline void unset_env(std::string name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

/// these are provided for compatibility with the char8_t for C++20 that breaks stuff
CLI11_INLINE std::string from_u8string(const std::string &s) { return s; }
CLI11_INLINE std::string from_u8string(std::string &&s) { return std::move(s); }
#if defined(__cpp_lib_char8_t)
CLI11_INLINE std::string from_u8string(const std::u8string &s) { return std::string(s.begin(), s.end()); }
#elif defined(__cpp_char8_t)
CLI11_INLINE std::string from_u8string(const char8_t *s) { return std::string(reinterpret_cast<const char *>(s)); }
#endif

CLI11_INLINE void check_identical_files(const char *path1, const char *path2) {
    std::string err1 = CLI::ExistingFile(path1);
    if(!err1.empty()) {
        FAIL("Could not open " << path1 << ": " << err1);
    }

    std::string err2 = CLI::ExistingFile(path2);
    if(!err2.empty()) {
        FAIL("Could not open " << path2 << ": " << err2);
    }

    // open files at the end to compare size first
    std::ifstream file1(path1, std::ifstream::ate | std::ifstream::binary);
    std::ifstream file2(path2, std::ifstream::ate | std::ifstream::binary);

    if(!file1.good()) {
        FAIL("File " << path1 << " is corrupted");
    }

    if(!file2.good()) {
        FAIL("File " << path2 << " is corrupted");
    }

    if(file1.tellg() != file2.tellg()) {
        FAIL("Different file sizes:\n  " << file1.tellg() << " bytes in " << path1 << "\n  " << file2.tellg()
                                         << " bytes in " << path2);
    }

    // rewind files
    file1.seekg(0);
    file2.seekg(0);

    std::array<uint8_t, 10240> buffer1;
    std::array<uint8_t, 10240> buffer2;

    for(size_t ibuffer = 0; file1.good(); ++ibuffer) {
        // Flawfinder: ignore
        file1.read(reinterpret_cast<char *>(buffer1.data()), static_cast<std::streamsize>(buffer1.size()));
        // Flawfinder: ignore
        file2.read(reinterpret_cast<char *>(buffer2.data()), static_cast<std::streamsize>(buffer2.size()));

        for(size_t i = 0; i < static_cast<size_t>(file1.gcount()); ++i) {
            if(buffer1[i] != buffer2[i]) {
                FAIL(std::hex << std::setfill('0') << "Different bytes at position " << (ibuffer * 10240 + i) << ":\n  "
                              << "0x" << std::setw(2) << static_cast<int>(buffer1[i]) << " in " << path1 << "\n  "
                              << "0x" << std::setw(2) << static_cast<int>(buffer2[i]) << " in " << path2);
            }
        }
    }
}
