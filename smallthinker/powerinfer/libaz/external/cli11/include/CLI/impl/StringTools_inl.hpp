// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../StringTools.hpp"

// [CLI11:public_includes:set]
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

namespace CLI {
// [CLI11:string_tools_inl_hpp:verbatim]

namespace detail {
CLI11_INLINE std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    // Check to see if empty string, give consistent result
    if(s.empty()) {
        elems.emplace_back();
    } else {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while(std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
    }
    return elems;
}

CLI11_INLINE std::string &ltrim(std::string &str) {
    auto it = std::find_if(str.begin(), str.end(), [](char ch) { return !std::isspace<char>(ch, std::locale()); });
    str.erase(str.begin(), it);
    return str;
}

CLI11_INLINE std::string &ltrim(std::string &str, const std::string &filter) {
    auto it = std::find_if(str.begin(), str.end(), [&filter](char ch) { return filter.find(ch) == std::string::npos; });
    str.erase(str.begin(), it);
    return str;
}

CLI11_INLINE std::string &rtrim(std::string &str) {
    auto it = std::find_if(str.rbegin(), str.rend(), [](char ch) { return !std::isspace<char>(ch, std::locale()); });
    str.erase(it.base(), str.end());
    return str;
}

CLI11_INLINE std::string &rtrim(std::string &str, const std::string &filter) {
    auto it =
        std::find_if(str.rbegin(), str.rend(), [&filter](char ch) { return filter.find(ch) == std::string::npos; });
    str.erase(it.base(), str.end());
    return str;
}

CLI11_INLINE std::string &remove_quotes(std::string &str) {
    if(str.length() > 1 && (str.front() == '"' || str.front() == '\'' || str.front() == '`')) {
        if(str.front() == str.back()) {
            str.pop_back();
            str.erase(str.begin(), str.begin() + 1);
        }
    }
    return str;
}

CLI11_INLINE std::string &remove_outer(std::string &str, char key) {
    if(str.length() > 1 && (str.front() == key)) {
        if(str.front() == str.back()) {
            str.pop_back();
            str.erase(str.begin(), str.begin() + 1);
        }
    }
    return str;
}

CLI11_INLINE std::string fix_newlines(const std::string &leader, std::string input) {
    std::string::size_type n = 0;
    while(n != std::string::npos && n < input.size()) {
        n = input.find('\n', n);
        if(n != std::string::npos) {
            input = input.substr(0, n + 1) + leader + input.substr(n + 1);
            n += leader.size();
        }
    }
    return input;
}

CLI11_INLINE std::ostream &format_aliases(std::ostream &out, const std::vector<std::string> &aliases, std::size_t wid) {
    if(!aliases.empty()) {
        out << std::setw(static_cast<int>(wid)) << "     aliases: ";
        bool front = true;
        for(const auto &alias : aliases) {
            if(!front) {
                out << ", ";
            } else {
                front = false;
            }
            out << detail::fix_newlines("              ", alias);
        }
        out << "\n";
    }
    return out;
}

CLI11_INLINE bool valid_name_string(const std::string &str) {
    if(str.empty() || !valid_first_char(str[0])) {
        return false;
    }
    auto e = str.end();
    for(auto c = str.begin() + 1; c != e; ++c)
        if(!valid_later_char(*c))
            return false;
    return true;
}

CLI11_INLINE std::string find_and_replace(std::string str, std::string from, std::string to) {

    std::size_t start_pos = 0;

    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    return str;
}

CLI11_INLINE void remove_default_flag_values(std::string &flags) {
    auto loc = flags.find_first_of('{', 2);
    while(loc != std::string::npos) {
        auto finish = flags.find_first_of("},", loc + 1);
        if((finish != std::string::npos) && (flags[finish] == '}')) {
            flags.erase(flags.begin() + static_cast<std::ptrdiff_t>(loc),
                        flags.begin() + static_cast<std::ptrdiff_t>(finish) + 1);
        }
        loc = flags.find_first_of('{', loc + 1);
    }
    flags.erase(std::remove(flags.begin(), flags.end(), '!'), flags.end());
}

CLI11_INLINE std::ptrdiff_t
find_member(std::string name, const std::vector<std::string> names, bool ignore_case, bool ignore_underscore) {
    auto it = std::end(names);
    if(ignore_case) {
        if(ignore_underscore) {
            name = detail::to_lower(detail::remove_underscore(name));
            it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
                return detail::to_lower(detail::remove_underscore(local_name)) == name;
            });
        } else {
            name = detail::to_lower(name);
            it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
                return detail::to_lower(local_name) == name;
            });
        }

    } else if(ignore_underscore) {
        name = detail::remove_underscore(name);
        it = std::find_if(std::begin(names), std::end(names), [&name](std::string local_name) {
            return detail::remove_underscore(local_name) == name;
        });
    } else {
        it = std::find(std::begin(names), std::end(names), name);
    }

    return (it != std::end(names)) ? (it - std::begin(names)) : (-1);
}

static const std::string escapedChars("\b\t\n\f\r\"\\");
static const std::string escapedCharsCode("btnfr\"\\");
static const std::string bracketChars{"\"'`[(<{"};
static const std::string matchBracketChars("\"'`])>}");

CLI11_INLINE bool has_escapable_character(const std::string &str) {
    return (str.find_first_of(escapedChars) != std::string::npos);
}

CLI11_INLINE std::string add_escaped_characters(const std::string &str) {
    std::string out;
    out.reserve(str.size() + 4);
    for(char s : str) {
        auto sloc = escapedChars.find_first_of(s);
        if(sloc != std::string::npos) {
            out.push_back('\\');
            out.push_back(escapedCharsCode[sloc]);
        } else {
            out.push_back(s);
        }
    }
    return out;
}

CLI11_INLINE std::uint32_t hexConvert(char hc) {
    int hcode{0};
    if(hc >= '0' && hc <= '9') {
        hcode = (hc - '0');
    } else if(hc >= 'A' && hc <= 'F') {
        hcode = (hc - 'A' + 10);
    } else if(hc >= 'a' && hc <= 'f') {
        hcode = (hc - 'a' + 10);
    } else {
        hcode = -1;
    }
    return static_cast<uint32_t>(hcode);
}

CLI11_INLINE char make_char(std::uint32_t code) { return static_cast<char>(static_cast<unsigned char>(code)); }

CLI11_INLINE void append_codepoint(std::string &str, std::uint32_t code) {
    if(code < 0x80) {  // ascii code equivalent
        str.push_back(static_cast<char>(code));
    } else if(code < 0x800) {  // \u0080 to \u07FF
        // 110yyyyx 10xxxxxx; 0x3f == 0b0011'1111
        str.push_back(make_char(0xC0 | code >> 6));
        str.push_back(make_char(0x80 | (code & 0x3F)));
    } else if(code < 0x10000) {  // U+0800...U+FFFF
        if(0xD800 <= code && code <= 0xDFFF) {
            throw std::invalid_argument("[0xD800, 0xDFFF] are not valid UTF-8.");
        }
        // 1110yyyy 10yxxxxx 10xxxxxx
        str.push_back(make_char(0xE0 | code >> 12));
        str.push_back(make_char(0x80 | (code >> 6 & 0x3F)));
        str.push_back(make_char(0x80 | (code & 0x3F)));
    } else if(code < 0x110000) {  // U+010000 ... U+10FFFF
        // 11110yyy 10yyxxxx 10xxxxxx 10xxxxxx
        str.push_back(make_char(0xF0 | code >> 18));
        str.push_back(make_char(0x80 | (code >> 12 & 0x3F)));
        str.push_back(make_char(0x80 | (code >> 6 & 0x3F)));
        str.push_back(make_char(0x80 | (code & 0x3F)));
    }
}

CLI11_INLINE std::string remove_escaped_characters(const std::string &str) {

    std::string out;
    out.reserve(str.size());
    for(auto loc = str.begin(); loc < str.end(); ++loc) {
        if(*loc == '\\') {
            if(str.end() - loc < 2) {
                throw std::invalid_argument("invalid escape sequence " + str);
            }
            auto ecloc = escapedCharsCode.find_first_of(*(loc + 1));
            if(ecloc != std::string::npos) {
                out.push_back(escapedChars[ecloc]);
                ++loc;
            } else if(*(loc + 1) == 'u') {
                // must have 4 hex characters
                if(str.end() - loc < 6) {
                    throw std::invalid_argument("unicode sequence must have 4 hex codes " + str);
                }
                std::uint32_t code{0};
                std::uint32_t mplier{16 * 16 * 16};
                for(int ii = 2; ii < 6; ++ii) {
                    std::uint32_t res = hexConvert(*(loc + ii));
                    if(res > 0x0F) {
                        throw std::invalid_argument("unicode sequence must have 4 hex codes " + str);
                    }
                    code += res * mplier;
                    mplier = mplier / 16;
                }
                append_codepoint(out, code);
                loc += 5;
            } else if(*(loc + 1) == 'U') {
                // must have 8 hex characters
                if(str.end() - loc < 10) {
                    throw std::invalid_argument("unicode sequence must have 8 hex codes " + str);
                }
                std::uint32_t code{0};
                std::uint32_t mplier{16 * 16 * 16 * 16 * 16 * 16 * 16};
                for(int ii = 2; ii < 10; ++ii) {
                    std::uint32_t res = hexConvert(*(loc + ii));
                    if(res > 0x0F) {
                        throw std::invalid_argument("unicode sequence must have 8 hex codes " + str);
                    }
                    code += res * mplier;
                    mplier = mplier / 16;
                }
                append_codepoint(out, code);
                loc += 9;
            } else if(*(loc + 1) == '0') {
                out.push_back('\0');
                ++loc;
            } else {
                throw std::invalid_argument(std::string("unrecognized escape sequence \\") + *(loc + 1) + " in " + str);
            }
        } else {
            out.push_back(*loc);
        }
    }
    return out;
}

CLI11_INLINE std::size_t close_string_quote(const std::string &str, std::size_t start, char closure_char) {
    std::size_t loc{0};
    for(loc = start + 1; loc < str.size(); ++loc) {
        if(str[loc] == closure_char) {
            break;
        }
        if(str[loc] == '\\') {
            // skip the next character for escaped sequences
            ++loc;
        }
    }
    return loc;
}

CLI11_INLINE std::size_t close_literal_quote(const std::string &str, std::size_t start, char closure_char) {
    auto loc = str.find_first_of(closure_char, start + 1);
    return (loc != std::string::npos ? loc : str.size());
}

CLI11_INLINE std::size_t close_sequence(const std::string &str, std::size_t start, char closure_char) {

    auto bracket_loc = matchBracketChars.find(closure_char);
    switch(bracket_loc) {
    case 0:
        return close_string_quote(str, start, closure_char);
    case 1:
    case 2:
    case std::string::npos:
        return close_literal_quote(str, start, closure_char);
    default:
        break;
    }

    std::string closures(1, closure_char);
    auto loc = start + 1;

    while(loc < str.size()) {
        if(str[loc] == closures.back()) {
            closures.pop_back();
            if(closures.empty()) {
                return loc;
            }
        }
        bracket_loc = bracketChars.find(str[loc]);
        if(bracket_loc != std::string::npos) {
            switch(bracket_loc) {
            case 0:
                loc = close_string_quote(str, loc, str[loc]);
                break;
            case 1:
            case 2:
                loc = close_literal_quote(str, loc, str[loc]);
                break;
            default:
                closures.push_back(matchBracketChars[bracket_loc]);
                break;
            }
        }
        ++loc;
    }
    if(loc > str.size()) {
        loc = str.size();
    }
    return loc;
}

CLI11_INLINE std::vector<std::string> split_up(std::string str, char delimiter) {

    auto find_ws = [delimiter](char ch) {
        return (delimiter == '\0') ? std::isspace<char>(ch, std::locale()) : (ch == delimiter);
    };
    trim(str);

    std::vector<std::string> output;
    while(!str.empty()) {
        if(bracketChars.find_first_of(str[0]) != std::string::npos) {
            auto bracketLoc = bracketChars.find_first_of(str[0]);
            auto end = close_sequence(str, 0, matchBracketChars[bracketLoc]);
            if(end >= str.size()) {
                output.push_back(std::move(str));
                str.clear();
            } else {
                output.push_back(str.substr(0, end + 1));
                if(end + 2 < str.size()) {
                    str = str.substr(end + 2);
                } else {
                    str.clear();
                }
            }

        } else {
            auto it = std::find_if(std::begin(str), std::end(str), find_ws);
            if(it != std::end(str)) {
                std::string value = std::string(str.begin(), it);
                output.push_back(value);
                str = std::string(it + 1, str.end());
            } else {
                output.push_back(str);
                str.clear();
            }
        }
        trim(str);
    }
    return output;
}

CLI11_INLINE std::size_t escape_detect(std::string &str, std::size_t offset) {
    auto next = str[offset + 1];
    if((next == '\"') || (next == '\'') || (next == '`')) {
        auto astart = str.find_last_of("-/ \"\'`", offset - 1);
        if(astart != std::string::npos) {
            if(str[astart] == ((str[offset] == '=') ? '-' : '/'))
                str[offset] = ' ';  // interpret this as a space so the split_up works properly
        }
    }
    return offset + 1;
}

CLI11_INLINE std::string binary_escape_string(const std::string &string_to_escape) {
    // s is our escaped output string
    std::string escaped_string{};
    // loop through all characters
    for(char c : string_to_escape) {
        // check if a given character is printable
        // the cast is necessary to avoid undefined behaviour
        if(isprint(static_cast<unsigned char>(c)) == 0) {
            std::stringstream stream;
            // if the character is not printable
            // we'll convert it to a hex string using a stringstream
            // note that since char is signed we have to cast it to unsigned first
            stream << std::hex << static_cast<unsigned int>(static_cast<unsigned char>(c));
            std::string code = stream.str();
            escaped_string += std::string("\\x") + (code.size() < 2 ? "0" : "") + code;
        } else if(c == 'x' || c == 'X') {
            // need to check for inadvertent binary sequences
            if(!escaped_string.empty() && escaped_string.back() == '\\') {
                escaped_string += std::string("\\x") + (c == 'x' ? "78" : "58");
            } else {
                escaped_string.push_back(c);
            }

        } else {
            escaped_string.push_back(c);
        }
    }
    if(escaped_string != string_to_escape) {
        auto sqLoc = escaped_string.find('\'');
        while(sqLoc != std::string::npos) {
            escaped_string[sqLoc] = '\\';
            escaped_string.insert(sqLoc + 1, "x27");
            sqLoc = escaped_string.find('\'');
        }
        escaped_string.insert(0, "'B\"(");
        escaped_string.push_back(')');
        escaped_string.push_back('"');
        escaped_string.push_back('\'');
    }
    return escaped_string;
}

CLI11_INLINE bool is_binary_escaped_string(const std::string &escaped_string) {
    size_t ssize = escaped_string.size();
    if(escaped_string.compare(0, 3, "B\"(") == 0 && escaped_string.compare(ssize - 2, 2, ")\"") == 0) {
        return true;
    }
    return (escaped_string.compare(0, 4, "'B\"(") == 0 && escaped_string.compare(ssize - 3, 3, ")\"'") == 0);
}

CLI11_INLINE std::string extract_binary_string(const std::string &escaped_string) {
    std::size_t start{0};
    std::size_t tail{0};
    size_t ssize = escaped_string.size();
    if(escaped_string.compare(0, 3, "B\"(") == 0 && escaped_string.compare(ssize - 2, 2, ")\"") == 0) {
        start = 3;
        tail = 2;
    } else if(escaped_string.compare(0, 4, "'B\"(") == 0 && escaped_string.compare(ssize - 3, 3, ")\"'") == 0) {
        start = 4;
        tail = 3;
    }

    if(start == 0) {
        return escaped_string;
    }
    std::string outstring;

    outstring.reserve(ssize - start - tail);
    std::size_t loc = start;
    while(loc < ssize - tail) {
        // ssize-2 to skip )" at the end
        if(escaped_string[loc] == '\\' && (escaped_string[loc + 1] == 'x' || escaped_string[loc + 1] == 'X')) {
            auto c1 = escaped_string[loc + 2];
            auto c2 = escaped_string[loc + 3];

            std::uint32_t res1 = hexConvert(c1);
            std::uint32_t res2 = hexConvert(c2);
            if(res1 <= 0x0F && res2 <= 0x0F) {
                loc += 4;
                outstring.push_back(static_cast<char>(res1 * 16 + res2));
                continue;
            }
        }
        outstring.push_back(escaped_string[loc]);
        ++loc;
    }
    return outstring;
}

CLI11_INLINE void remove_quotes(std::vector<std::string> &args) {
    for(auto &arg : args) {
        if(arg.front() == '\"' && arg.back() == '\"') {
            remove_quotes(arg);
            // only remove escaped for string arguments not literal strings
            arg = remove_escaped_characters(arg);
        } else {
            remove_quotes(arg);
        }
    }
}

CLI11_INLINE void handle_secondary_array(std::string &str) {
    if(str.size() >= 2 && str.front() == '[' && str.back() == ']') {
        // handle some special array processing for arguments if it might be interpreted as a secondary array
        std::string tstr{"[["};
        for(std::size_t ii = 1; ii < str.size(); ++ii) {
            tstr.push_back(str[ii]);
            tstr.push_back(str[ii]);
        }
        str = std::move(tstr);
    }
}

CLI11_INLINE bool process_quoted_string(std::string &str, char string_char, char literal_char) {
    if(str.size() <= 1) {
        return false;
    }
    if(detail::is_binary_escaped_string(str)) {
        str = detail::extract_binary_string(str);
        handle_secondary_array(str);
        return true;
    }
    if(str.front() == string_char && str.back() == string_char) {
        detail::remove_outer(str, string_char);
        if(str.find_first_of('\\') != std::string::npos) {
            str = detail::remove_escaped_characters(str);
        }
        handle_secondary_array(str);
        return true;
    }
    if((str.front() == literal_char || str.front() == '`') && str.back() == str.front()) {
        detail::remove_outer(str, str.front());
        handle_secondary_array(str);
        return true;
    }
    return false;
}

std::string get_environment_value(const std::string &env_name) {
    char *buffer = nullptr;
    std::string ename_string;

#ifdef _MSC_VER
    // Windows version
    std::size_t sz = 0;
    if(_dupenv_s(&buffer, &sz, env_name.c_str()) == 0 && buffer != nullptr) {
        ename_string = std::string(buffer);
        free(buffer);
    }
#else
    // This also works on Windows, but gives a warning
    buffer = std::getenv(env_name.c_str());
    if(buffer != nullptr) {
        ename_string = std::string(buffer);
    }
#endif
    return ename_string;
}

CLI11_INLINE std::ostream &streamOutAsParagraph(std::ostream &out,
                                                const std::string &text,
                                                std::size_t paragraphWidth,
                                                const std::string &linePrefix,
                                                bool skipPrefixOnFirstLine) {
    if(!skipPrefixOnFirstLine)
        out << linePrefix;  // First line prefix

    std::istringstream lss(text);
    std::string line = "";
    while(std::getline(lss, line)) {
        std::istringstream iss(line);
        std::string word = "";
        std::size_t charsWritten = 0;

        while(iss >> word) {
            if(word.length() + charsWritten > paragraphWidth) {
                out << '\n' << linePrefix;
                charsWritten = 0;
            }

            out << word << " ";
            charsWritten += word.length() + 1;
        }

        if(!lss.eof())
            out << '\n' << linePrefix;
    }
    return out;
}

}  // namespace detail
// [CLI11:string_tools_inl_hpp:end]
}  // namespace CLI
