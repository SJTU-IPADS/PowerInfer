// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../Split.hpp"

// [CLI11:public_includes:set]
#include <string>
#include <tuple>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

#include "../Error.hpp"
#include "../StringTools.hpp"

namespace CLI {
// [CLI11:split_inl_hpp:verbatim]

namespace detail {

CLI11_INLINE bool split_short(const std::string &current, std::string &name, std::string &rest) {
    if(current.size() > 1 && current[0] == '-' && valid_first_char(current[1])) {
        name = current.substr(1, 1);
        rest = current.substr(2);
        return true;
    }
    return false;
}

CLI11_INLINE bool split_long(const std::string &current, std::string &name, std::string &value) {
    if(current.size() > 2 && current.compare(0, 2, "--") == 0 && valid_first_char(current[2])) {
        auto loc = current.find_first_of('=');
        if(loc != std::string::npos) {
            name = current.substr(2, loc - 2);
            value = current.substr(loc + 1);
        } else {
            name = current.substr(2);
            value = "";
        }
        return true;
    }
    return false;
}

CLI11_INLINE bool split_windows_style(const std::string &current, std::string &name, std::string &value) {
    if(current.size() > 1 && current[0] == '/' && valid_first_char(current[1])) {
        auto loc = current.find_first_of(':');
        if(loc != std::string::npos) {
            name = current.substr(1, loc - 1);
            value = current.substr(loc + 1);
        } else {
            name = current.substr(1);
            value = "";
        }
        return true;
    }
    return false;
}

CLI11_INLINE std::vector<std::string> split_names(std::string current) {
    std::vector<std::string> output;
    std::size_t val = 0;
    while((val = current.find(',')) != std::string::npos) {
        output.push_back(trim_copy(current.substr(0, val)));
        current = current.substr(val + 1);
    }
    output.push_back(trim_copy(current));
    return output;
}

CLI11_INLINE std::vector<std::pair<std::string, std::string>> get_default_flag_values(const std::string &str) {
    std::vector<std::string> flags = split_names(str);
    flags.erase(std::remove_if(flags.begin(),
                               flags.end(),
                               [](const std::string &name) {
                                   return ((name.empty()) || (!(((name.find_first_of('{') != std::string::npos) &&
                                                                 (name.back() == '}')) ||
                                                                (name[0] == '!'))));
                               }),
                flags.end());
    std::vector<std::pair<std::string, std::string>> output;
    output.reserve(flags.size());
    for(auto &flag : flags) {
        auto def_start = flag.find_first_of('{');
        std::string defval = "false";
        if((def_start != std::string::npos) && (flag.back() == '}')) {
            defval = flag.substr(def_start + 1);
            defval.pop_back();
            flag.erase(def_start, std::string::npos);  // NOLINT(readability-suspicious-call-argument)
        }
        flag.erase(0, flag.find_first_not_of("-!"));
        output.emplace_back(flag, defval);
    }
    return output;
}

CLI11_INLINE std::tuple<std::vector<std::string>, std::vector<std::string>, std::string>
get_names(const std::vector<std::string> &input, bool allow_non_standard) {

    std::vector<std::string> short_names;
    std::vector<std::string> long_names;
    std::string pos_name;
    for(std::string name : input) {
        if(name.length() == 0) {
            continue;
        }
        if(name.length() > 1 && name[0] == '-' && name[1] != '-') {
            if(name.length() == 2 && valid_first_char(name[1])) {
                short_names.emplace_back(1, name[1]);
            } else if(name.length() > 2) {
                if(allow_non_standard) {
                    name = name.substr(1);
                    if(valid_name_string(name)) {
                        short_names.push_back(name);
                    } else {
                        throw BadNameString::BadLongName(name);
                    }
                } else {
                    throw BadNameString::MissingDash(name);
                }
            } else {
                throw BadNameString::OneCharName(name);
            }
        } else if(name.length() > 2 && name.substr(0, 2) == "--") {
            name = name.substr(2);
            if(valid_name_string(name)) {
                long_names.push_back(name);
            } else {
                throw BadNameString::BadLongName(name);
            }
        } else if(name == "-" || name == "--" || name == "++") {
            throw BadNameString::ReservedName(name);
        } else {
            if(!pos_name.empty()) {
                throw BadNameString::MultiPositionalNames(name);
            }
            if(valid_name_string(name)) {
                pos_name = name;
            } else {
                throw BadNameString::BadPositionalName(name);
            }
        }
    }
    return std::make_tuple(short_names, long_names, pos_name);
}

}  // namespace detail
// [CLI11:split_inl_hpp:end]
}  // namespace CLI
