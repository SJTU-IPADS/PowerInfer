// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../Config.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

namespace CLI {
// [CLI11:config_inl_hpp:verbatim]

static constexpr auto multiline_literal_quote = R"(''')";
static constexpr auto multiline_string_quote = R"(""")";

namespace detail {

CLI11_INLINE bool is_printable(const std::string &test_string) {
    return std::all_of(test_string.begin(), test_string.end(), [](char x) {
        return (isprint(static_cast<unsigned char>(x)) != 0 || x == '\n' || x == '\t');
    });
}

CLI11_INLINE std::string
convert_arg_for_ini(const std::string &arg, char stringQuote, char literalQuote, bool disable_multi_line) {
    if(arg.empty()) {
        return std::string(2, stringQuote);
    }
    // some specifically supported strings
    if(arg == "true" || arg == "false" || arg == "nan" || arg == "inf") {
        return arg;
    }
    // floating point conversion can convert some hex codes, but don't try that here
    if(arg.compare(0, 2, "0x") != 0 && arg.compare(0, 2, "0X") != 0) {
        using CLI::detail::lexical_cast;
        double val = 0.0;
        if(lexical_cast(arg, val)) {
            if(arg.find_first_not_of("0123456789.-+eE") == std::string::npos) {
                return arg;
            }
        }
    }
    // just quote a single non numeric character
    if(arg.size() == 1) {
        if(isprint(static_cast<unsigned char>(arg.front())) == 0) {
            return binary_escape_string(arg);
        }
        if(arg == "'") {
            return std::string(1, stringQuote) + "'" + stringQuote;
        }
        return std::string(1, literalQuote) + arg + literalQuote;
    }
    // handle hex, binary or octal arguments
    if(arg.front() == '0') {
        if(arg[1] == 'x') {
            if(std::all_of(arg.begin() + 2, arg.end(), [](char x) {
                   return (x >= '0' && x <= '9') || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f');
               })) {
                return arg;
            }
        } else if(arg[1] == 'o') {
            if(std::all_of(arg.begin() + 2, arg.end(), [](char x) { return (x >= '0' && x <= '7'); })) {
                return arg;
            }
        } else if(arg[1] == 'b') {
            if(std::all_of(arg.begin() + 2, arg.end(), [](char x) { return (x == '0' || x == '1'); })) {
                return arg;
            }
        }
    }
    if(!is_printable(arg)) {
        return binary_escape_string(arg);
    }
    if(detail::has_escapable_character(arg)) {
        if(arg.size() > 100 && !disable_multi_line) {
            return std::string(multiline_literal_quote) + arg + multiline_literal_quote;
        }
        return std::string(1, stringQuote) + detail::add_escaped_characters(arg) + stringQuote;
    }
    return std::string(1, stringQuote) + arg + stringQuote;
}

CLI11_INLINE std::string ini_join(const std::vector<std::string> &args,
                                  char sepChar,
                                  char arrayStart,
                                  char arrayEnd,
                                  char stringQuote,
                                  char literalQuote) {
    bool disable_multi_line{false};
    std::string joined;
    if(args.size() > 1 && arrayStart != '\0') {
        joined.push_back(arrayStart);
        disable_multi_line = true;
    }
    std::size_t start = 0;
    for(const auto &arg : args) {
        if(start++ > 0) {
            joined.push_back(sepChar);
            if(!std::isspace<char>(sepChar, std::locale())) {
                joined.push_back(' ');
            }
        }
        joined.append(convert_arg_for_ini(arg, stringQuote, literalQuote, disable_multi_line));
    }
    if(args.size() > 1 && arrayEnd != '\0') {
        joined.push_back(arrayEnd);
    }
    return joined;
}

CLI11_INLINE std::vector<std::string>
generate_parents(const std::string &section, std::string &name, char parentSeparator) {
    std::vector<std::string> parents;
    if(detail::to_lower(section) != "default") {
        if(section.find(parentSeparator) != std::string::npos) {
            parents = detail::split_up(section, parentSeparator);
        } else {
            parents = {section};
        }
    }
    if(name.find(parentSeparator) != std::string::npos) {
        std::vector<std::string> plist = detail::split_up(name, parentSeparator);
        name = plist.back();
        plist.pop_back();
        parents.insert(parents.end(), plist.begin(), plist.end());
    }
    // clean up quotes on the parents
    try {
        detail::remove_quotes(parents);
    } catch(const std::invalid_argument &iarg) {
        throw CLI::ParseError(iarg.what(), CLI::ExitCodes::InvalidError);
    }
    return parents;
}

CLI11_INLINE void
checkParentSegments(std::vector<ConfigItem> &output, const std::string &currentSection, char parentSeparator) {

    std::string estring;
    auto parents = detail::generate_parents(currentSection, estring, parentSeparator);
    if(!output.empty() && output.back().name == "--") {
        std::size_t msize = (parents.size() > 1U) ? parents.size() : 2;
        while(output.back().parents.size() >= msize) {
            output.push_back(output.back());
            output.back().parents.pop_back();
        }

        if(parents.size() > 1) {
            std::size_t common = 0;
            std::size_t mpair = (std::min)(output.back().parents.size(), parents.size() - 1);
            for(std::size_t ii = 0; ii < mpair; ++ii) {
                if(output.back().parents[ii] != parents[ii]) {
                    break;
                }
                ++common;
            }
            if(common == mpair) {
                output.pop_back();
            } else {
                while(output.back().parents.size() > common + 1) {
                    output.push_back(output.back());
                    output.back().parents.pop_back();
                }
            }
            for(std::size_t ii = common; ii < parents.size() - 1; ++ii) {
                output.emplace_back();
                output.back().parents.assign(parents.begin(), parents.begin() + static_cast<std::ptrdiff_t>(ii) + 1);
                output.back().name = "++";
            }
        }
    } else if(parents.size() > 1) {
        for(std::size_t ii = 0; ii < parents.size() - 1; ++ii) {
            output.emplace_back();
            output.back().parents.assign(parents.begin(), parents.begin() + static_cast<std::ptrdiff_t>(ii) + 1);
            output.back().name = "++";
        }
    }

    // insert a section end which is just an empty items_buffer
    output.emplace_back();
    output.back().parents = std::move(parents);
    output.back().name = "++";
}

/// @brief  checks if a string represents a multiline comment
CLI11_INLINE bool hasMLString(std::string const &fullString, char check) {
    if(fullString.length() < 3) {
        return false;
    }
    auto it = fullString.rbegin();
    return (*it == check) && (*(it + 1) == check) && (*(it + 2) == check);
}

/// @brief  find a matching configItem in a list
inline auto find_matching_config(std::vector<ConfigItem> &items,
                                 const std::vector<std::string> &parents,
                                 const std::string &name,
                                 bool fullSearch) -> decltype(items.begin()) {
    if(items.empty()) {
        return items.end();
    }
    auto search = items.end() - 1;
    do {
        if(search->parents == parents && search->name == name) {
            return search;
        }
        if(search == items.begin()) {
            break;
        }
        --search;
    } while(fullSearch);
    return items.end();
}
}  // namespace detail

inline std::vector<ConfigItem> ConfigBase::from_config(std::istream &input) const {
    std::string line;
    std::string buffer;
    std::string currentSection = "default";
    std::string previousSection = "default";
    std::vector<ConfigItem> output;
    bool isDefaultArray = (arrayStart == '[' && arrayEnd == ']' && arraySeparator == ',');
    bool isINIArray = (arrayStart == '\0' || arrayStart == ' ') && arrayStart == arrayEnd;
    bool inSection{false};
    bool inMLineComment{false};
    bool inMLineValue{false};

    char aStart = (isINIArray) ? '[' : arrayStart;
    char aEnd = (isINIArray) ? ']' : arrayEnd;
    char aSep = (isINIArray && arraySeparator == ' ') ? ',' : arraySeparator;
    int currentSectionIndex{0};

    std::string line_sep_chars{parentSeparatorChar, commentChar, valueDelimiter};
    while(getline(input, buffer)) {
        std::vector<std::string> items_buffer;
        std::string name;
        line = detail::trim_copy(buffer);
        std::size_t len = line.length();
        // lines have to be at least 3 characters to have any meaning to CLI just skip the rest
        if(len < 3) {
            continue;
        }
        if(line.compare(0, 3, multiline_string_quote) == 0 || line.compare(0, 3, multiline_literal_quote) == 0) {
            inMLineComment = true;
            auto cchar = line.front();
            while(inMLineComment) {
                if(getline(input, line)) {
                    detail::trim(line);
                } else {
                    break;
                }
                if(detail::hasMLString(line, cchar)) {
                    inMLineComment = false;
                }
            }
            continue;
        }
        if(line.front() == '[' && line.back() == ']') {
            if(currentSection != "default") {
                // insert a section end which is just an empty items_buffer
                output.emplace_back();
                output.back().parents = detail::generate_parents(currentSection, name, parentSeparatorChar);
                output.back().name = "--";
            }
            currentSection = line.substr(1, len - 2);
            // deal with double brackets for TOML
            if(currentSection.size() > 1 && currentSection.front() == '[' && currentSection.back() == ']') {
                currentSection = currentSection.substr(1, currentSection.size() - 2);
            }
            if(detail::to_lower(currentSection) == "default") {
                currentSection = "default";
            } else {
                detail::checkParentSegments(output, currentSection, parentSeparatorChar);
            }
            inSection = false;
            if(currentSection == previousSection) {
                ++currentSectionIndex;
            } else {
                currentSectionIndex = 0;
                previousSection = currentSection;
            }
            continue;
        }

        // comment lines
        if(line.front() == ';' || line.front() == '#' || line.front() == commentChar) {
            continue;
        }
        std::size_t search_start = 0;
        if(line.find_first_of("\"'`") != std::string::npos) {
            while(search_start < line.size()) {
                auto test_char = line[search_start];
                if(test_char == '\"' || test_char == '\'' || test_char == '`') {
                    search_start = detail::close_sequence(line, search_start, line[search_start]);
                    ++search_start;
                } else if(test_char == valueDelimiter || test_char == commentChar) {
                    --search_start;
                    break;
                } else if(test_char == ' ' || test_char == '\t' || test_char == parentSeparatorChar) {
                    ++search_start;
                } else {
                    search_start = line.find_first_of(line_sep_chars, search_start);
                }
            }
        }
        // Find = in string, split and recombine
        auto delimiter_pos = line.find_first_of(valueDelimiter, search_start + 1);
        auto comment_pos = line.find_first_of(commentChar, search_start);
        if(comment_pos < delimiter_pos) {
            delimiter_pos = std::string::npos;
        }
        if(delimiter_pos != std::string::npos) {

            name = detail::trim_copy(line.substr(0, delimiter_pos));
            std::string item = detail::trim_copy(line.substr(delimiter_pos + 1, std::string::npos));
            bool mlquote =
                (item.compare(0, 3, multiline_literal_quote) == 0 || item.compare(0, 3, multiline_string_quote) == 0);
            if(!mlquote && comment_pos != std::string::npos) {
                auto citems = detail::split_up(item, commentChar);
                item = detail::trim_copy(citems.front());
            }
            if(mlquote) {
                // multiline string
                auto keyChar = item.front();
                item = buffer.substr(delimiter_pos + 1, std::string::npos);
                detail::ltrim(item);
                item.erase(0, 3);
                inMLineValue = true;
                bool lineExtension{false};
                bool firstLine = true;
                if(!item.empty() && item.back() == '\\') {
                    item.pop_back();
                    lineExtension = true;
                } else if(detail::hasMLString(item, keyChar)) {
                    // deal with the first line closing the multiline literal
                    item.pop_back();
                    item.pop_back();
                    item.pop_back();
                    if(keyChar == '\"') {
                        try {
                            item = detail::remove_escaped_characters(item);
                        } catch(const std::invalid_argument &iarg) {
                            throw CLI::ParseError(iarg.what(), CLI::ExitCodes::InvalidError);
                        }
                    }
                    inMLineValue = false;
                }
                while(inMLineValue) {
                    std::string l2;
                    if(!std::getline(input, l2)) {
                        break;
                    }
                    line = l2;
                    detail::rtrim(line);
                    if(detail::hasMLString(line, keyChar)) {
                        line.pop_back();
                        line.pop_back();
                        line.pop_back();
                        if(lineExtension) {
                            detail::ltrim(line);
                        } else if(!(firstLine && item.empty())) {
                            item.push_back('\n');
                        }
                        firstLine = false;
                        item += line;
                        inMLineValue = false;
                        if(!item.empty() && item.back() == '\n') {
                            item.pop_back();
                        }
                        if(keyChar == '\"') {
                            try {
                                item = detail::remove_escaped_characters(item);
                            } catch(const std::invalid_argument &iarg) {
                                throw CLI::ParseError(iarg.what(), CLI::ExitCodes::InvalidError);
                            }
                        }
                    } else {
                        if(lineExtension) {
                            detail::trim(l2);
                        } else if(!(firstLine && item.empty())) {
                            item.push_back('\n');
                        }
                        lineExtension = false;
                        firstLine = false;
                        if(!l2.empty() && l2.back() == '\\') {
                            lineExtension = true;
                            l2.pop_back();
                        }
                        item += l2;
                    }
                }
                items_buffer = {item};
            } else if(item.size() > 1 && item.front() == aStart) {
                for(std::string multiline; item.back() != aEnd && std::getline(input, multiline);) {
                    detail::trim(multiline);
                    item += multiline;
                }
                if(item.back() == aEnd) {
                    items_buffer = detail::split_up(item.substr(1, item.length() - 2), aSep);
                } else {
                    items_buffer = detail::split_up(item.substr(1, std::string::npos), aSep);
                }
            } else if((isDefaultArray || isINIArray) && item.find_first_of(aSep) != std::string::npos) {
                items_buffer = detail::split_up(item, aSep);
            } else if((isDefaultArray || isINIArray) && item.find_first_of(' ') != std::string::npos) {
                items_buffer = detail::split_up(item, '\0');
            } else {
                items_buffer = {item};
            }
        } else {
            name = detail::trim_copy(line.substr(0, comment_pos));
            items_buffer = {"true"};
        }
        std::vector<std::string> parents;
        try {
            parents = detail::generate_parents(currentSection, name, parentSeparatorChar);
            detail::process_quoted_string(name);
            // clean up quotes on the items and check for escaped strings
            for(auto &it : items_buffer) {
                detail::process_quoted_string(it, stringQuote, literalQuote);
            }
        } catch(const std::invalid_argument &ia) {
            throw CLI::ParseError(ia.what(), CLI::ExitCodes::InvalidError);
        }

        if(parents.size() > maximumLayers) {
            continue;
        }
        if(!configSection.empty() && !inSection) {
            if(parents.empty() || parents.front() != configSection) {
                continue;
            }
            if(configIndex >= 0 && currentSectionIndex != configIndex) {
                continue;
            }
            parents.erase(parents.begin());
            inSection = true;
        }
        auto match = detail::find_matching_config(output, parents, name, allowMultipleDuplicateFields);
        if(match != output.end()) {
            if((match->inputs.size() > 1 && items_buffer.size() > 1) || allowMultipleDuplicateFields) {
                // insert a separator if one is not already present
                if(!(match->inputs.back().empty() || items_buffer.front().empty() || match->inputs.back() == "%%" ||
                     items_buffer.front() == "%%")) {
                    match->inputs.emplace_back("%%");
                    match->multiline = true;
                }
            }
            match->inputs.insert(match->inputs.end(), items_buffer.begin(), items_buffer.end());
        } else {
            output.emplace_back();
            output.back().parents = std::move(parents);
            output.back().name = std::move(name);
            output.back().inputs = std::move(items_buffer);
        }
    }
    if(currentSection != "default") {
        // insert a section end which is just an empty items_buffer
        std::string ename;
        output.emplace_back();
        output.back().parents = detail::generate_parents(currentSection, ename, parentSeparatorChar);
        output.back().name = "--";
        while(output.back().parents.size() > 1) {
            output.push_back(output.back());
            output.back().parents.pop_back();
        }
    }
    return output;
}

CLI11_INLINE std::string &clean_name_string(std::string &name, const std::string &keyChars) {
    if(name.find_first_of(keyChars) != std::string::npos || (name.front() == '[' && name.back() == ']') ||
       (name.find_first_of("'`\"\\") != std::string::npos)) {
        if(name.find_first_of('\'') == std::string::npos) {
            name.insert(0, 1, '\'');
            name.push_back('\'');
        } else {
            if(detail::has_escapable_character(name)) {
                name = detail::add_escaped_characters(name);
            }
            name.insert(0, 1, '\"');
            name.push_back('\"');
        }
    }
    return name;
}

CLI11_INLINE std::string
ConfigBase::to_config(const App *app, bool default_also, bool write_description, std::string prefix) const {
    std::stringstream out;
    std::string commentLead;
    commentLead.push_back(commentChar);
    commentLead.push_back(' ');

    std::string commentTest = "#;";
    commentTest.push_back(commentChar);
    commentTest.push_back(parentSeparatorChar);

    std::string keyChars = commentTest;
    keyChars.push_back(literalQuote);
    keyChars.push_back(stringQuote);
    keyChars.push_back(arrayStart);
    keyChars.push_back(arrayEnd);
    keyChars.push_back(valueDelimiter);
    keyChars.push_back(arraySeparator);

    std::vector<std::string> groups = app->get_groups();
    bool defaultUsed = false;
    groups.insert(groups.begin(), std::string("OPTIONS"));

    for(auto &group : groups) {
        if(group == "OPTIONS" || group.empty()) {
            if(defaultUsed) {
                continue;
            }
            defaultUsed = true;
        }
        if(write_description && group != "OPTIONS" && !group.empty()) {
            out << '\n' << commentChar << commentLead << group << " Options\n";
        }
        for(const Option *opt : app->get_options({})) {
            // Only process options that are configurable
            if(opt->get_configurable()) {
                if(opt->get_group() != group) {
                    if(!(group == "OPTIONS" && opt->get_group().empty())) {
                        continue;
                    }
                }
                std::string single_name = opt->get_single_name();
                if(single_name.empty()) {
                    continue;
                }

                auto results = opt->reduced_results();
                std::string value =
                    detail::ini_join(results, arraySeparator, arrayStart, arrayEnd, stringQuote, literalQuote);

                bool isDefault = false;
                if(value.empty() && default_also) {
                    if(!opt->get_default_str().empty()) {
                        value = detail::convert_arg_for_ini(opt->get_default_str(), stringQuote, literalQuote, false);
                    } else if(opt->get_expected_min() == 0) {
                        value = "false";
                    } else if(opt->get_run_callback_for_default() || !opt->get_required()) {
                        value = "\"\"";  // empty string default value
                    } else {
                        value = "\"<REQUIRED>\"";
                    }
                    isDefault = true;
                }

                if(!value.empty()) {
                    if(!opt->get_fnames().empty()) {
                        try {
                            value = opt->get_flag_value(single_name, value);
                        } catch(const CLI::ArgumentMismatch &) {
                            bool valid{false};
                            for(const auto &test_name : opt->get_fnames()) {
                                try {
                                    value = opt->get_flag_value(test_name, value);
                                    single_name = test_name;
                                    valid = true;
                                } catch(const CLI::ArgumentMismatch &) {
                                    continue;
                                }
                            }
                            if(!valid) {
                                value = detail::ini_join(
                                    opt->results(), arraySeparator, arrayStart, arrayEnd, stringQuote, literalQuote);
                            }
                        }
                    }
                    if(write_description && opt->has_description()) {
                        if(out.tellp() != std::streampos(0)) {
                            out << '\n';
                        }
                        out << commentLead << detail::fix_newlines(commentLead, opt->get_description()) << '\n';
                    }
                    clean_name_string(single_name, keyChars);

                    std::string name = prefix + single_name;
                    if(commentDefaultsBool && isDefault) {
                        name = commentChar + name;
                    }
                    out << name << valueDelimiter << value << '\n';
                }
            }
        }
    }

    auto subcommands = app->get_subcommands({});
    for(const App *subcom : subcommands) {
        if(subcom->get_name().empty()) {
            if(!default_also && (subcom->count_all() == 0)) {
                continue;
            }
            if(write_description && !subcom->get_group().empty()) {
                out << '\n' << commentLead << subcom->get_group() << " Options\n";
            }
            /*if (!prefix.empty() || app->get_parent() == nullptr) {
                out << '[' << prefix << "___"<< subcom->get_group() << "]\n";
            } else {
                std::string subname = app->get_name() + parentSeparatorChar + "___"+subcom->get_group();
                const auto *p = app->get_parent();
                while(p->get_parent() != nullptr) {
                    subname = p->get_name() + parentSeparatorChar +subname;
                    p = p->get_parent();
                }
                out << '[' << subname << "]\n";
            }
            */
            out << to_config(subcom, default_also, write_description, prefix);
        }
    }

    for(const App *subcom : subcommands) {
        if(!subcom->get_name().empty()) {
            if(!default_also && (subcom->count_all() == 0)) {
                continue;
            }
            std::string subname = subcom->get_name();
            clean_name_string(subname, keyChars);

            if(subcom->get_configurable() && (default_also || app->got_subcommand(subcom))) {
                if(!prefix.empty() || app->get_parent() == nullptr) {

                    out << '[' << prefix << subname << "]\n";
                } else {
                    std::string appname = app->get_name();
                    clean_name_string(appname, keyChars);
                    subname = appname + parentSeparatorChar + subname;
                    const auto *p = app->get_parent();
                    while(p->get_parent() != nullptr) {
                        std::string pname = p->get_name();
                        clean_name_string(pname, keyChars);
                        subname = pname + parentSeparatorChar + subname;
                        p = p->get_parent();
                    }
                    out << '[' << subname << "]\n";
                }
                out << to_config(subcom, default_also, write_description, "");
            } else {
                out << to_config(subcom, default_also, write_description, prefix + subname + parentSeparatorChar);
            }
        }
    }

    if(write_description && !out.str().empty()) {
        std::string outString =
            commentChar + commentLead + detail::fix_newlines(commentChar + commentLead, app->get_description()) + '\n';
        return outString + out.str();
    }
    return out.str();
}
// [CLI11:config_inl_hpp:end]
}  // namespace CLI
