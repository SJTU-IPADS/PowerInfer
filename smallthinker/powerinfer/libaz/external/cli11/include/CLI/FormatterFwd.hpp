// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

#include "StringTools.hpp"

namespace CLI {
// [CLI11:formatter_fwd_hpp:verbatim]

class Option;
class App;

/// This enum signifies the type of help requested
///
/// This is passed in by App; all user classes must accept this as
/// the second argument.

enum class AppFormatMode {
    Normal,  ///< The normal, detailed help
    All,     ///< A fully expanded help
    Sub,     ///< Used when printed as part of expanded subcommand
};

/// This is the minimum requirements to run a formatter.
///
/// A user can subclass this is if they do not care at all
/// about the structure in CLI::Formatter.
class FormatterBase {
  protected:
    /// @name Options
    ///@{

    /// The width of the left column (options/flags/subcommands)
    std::size_t column_width_{30};

    /// The width of the right column (description of options/flags/subcommands)
    std::size_t right_column_width_{65};

    /// The width of the description paragraph at the top of help
    std::size_t description_paragraph_width_{80};

    /// The width of the footer paragraph
    std::size_t footer_paragraph_width_{80};

    /// @brief The required help printout labels (user changeable)
    /// Values are Needs, Excludes, etc.
    std::map<std::string, std::string> labels_{};

    ///@}
    /// @name Basic
    ///@{

  public:
    FormatterBase() = default;
    FormatterBase(const FormatterBase &) = default;
    FormatterBase(FormatterBase &&) = default;
    FormatterBase &operator=(const FormatterBase &) = default;
    FormatterBase &operator=(FormatterBase &&) = default;

    /// Adding a destructor in this form to work around bug in GCC 4.7
    virtual ~FormatterBase() noexcept {}  // NOLINT(modernize-use-equals-default)

    /// This is the key method that puts together help
    virtual std::string make_help(const App *, std::string, AppFormatMode) const = 0;

    ///@}
    /// @name Setters
    ///@{

    /// Set the "REQUIRED" label
    void label(std::string key, std::string val) { labels_[key] = val; }

    /// Set the left column width (options/flags/subcommands)
    void column_width(std::size_t val) { column_width_ = val; }

    /// Set the right column width (description of options/flags/subcommands)
    void right_column_width(std::size_t val) { right_column_width_ = val; }

    /// Set the description paragraph width at the top of help
    void description_paragraph_width(std::size_t val) { description_paragraph_width_ = val; }

    /// Set the footer paragraph width
    void footer_paragraph_width(std::size_t val) { footer_paragraph_width_ = val; }

    ///@}
    /// @name Getters
    ///@{

    /// Get the current value of a name (REQUIRED, etc.)
    CLI11_NODISCARD std::string get_label(std::string key) const {
        if(labels_.find(key) == labels_.end())
            return key;
        return labels_.at(key);
    }

    /// Get the current left column width (options/flags/subcommands)
    CLI11_NODISCARD std::size_t get_column_width() const { return column_width_; }

    /// Get the current right column width (description of options/flags/subcommands)
    CLI11_NODISCARD std::size_t get_right_column_width() const { return right_column_width_; }

    /// Get the current description paragraph width at the top of help
    CLI11_NODISCARD std::size_t get_description_paragraph_width() const { return description_paragraph_width_; }

    /// Get the current footer paragraph width
    CLI11_NODISCARD std::size_t get_footer_paragraph_width() const { return footer_paragraph_width_; }

    ///@}
};

/// This is a specialty override for lambda functions
class FormatterLambda final : public FormatterBase {
    using funct_t = std::function<std::string(const App *, std::string, AppFormatMode)>;

    /// The lambda to hold and run
    funct_t lambda_;

  public:
    /// Create a FormatterLambda with a lambda function
    explicit FormatterLambda(funct_t funct) : lambda_(std::move(funct)) {}

    /// Adding a destructor (mostly to make GCC 4.7 happy)
    ~FormatterLambda() noexcept override {}  // NOLINT(modernize-use-equals-default)

    /// This will simply call the lambda function
    std::string make_help(const App *app, std::string name, AppFormatMode mode) const override {
        return lambda_(app, name, mode);
    }
};

/// This is the default Formatter for CLI11. It pretty prints help output, and is broken into quite a few
/// overridable methods, to be highly customizable with minimal effort.
class Formatter : public FormatterBase {
  public:
    Formatter() = default;
    Formatter(const Formatter &) = default;
    Formatter(Formatter &&) = default;
    Formatter &operator=(const Formatter &) = default;
    Formatter &operator=(Formatter &&) = default;

    /// @name Overridables
    ///@{

    /// This prints out a group of options with title
    ///
    CLI11_NODISCARD virtual std::string
    make_group(std::string group, bool is_positional, std::vector<const Option *> opts) const;

    /// This prints out just the positionals "group"
    virtual std::string make_positionals(const App *app) const;

    /// This prints out all the groups of options
    std::string make_groups(const App *app, AppFormatMode mode) const;

    /// This prints out all the subcommands
    virtual std::string make_subcommands(const App *app, AppFormatMode mode) const;

    /// This prints out a subcommand
    virtual std::string make_subcommand(const App *sub) const;

    /// This prints out a subcommand in help-all
    virtual std::string make_expanded(const App *sub, AppFormatMode mode) const;

    /// This prints out all the groups of options
    virtual std::string make_footer(const App *app) const;

    /// This displays the description line
    virtual std::string make_description(const App *app) const;

    /// This displays the usage line
    virtual std::string make_usage(const App *app, std::string name) const;

    /// This puts everything together
    std::string make_help(const App *app, std::string, AppFormatMode mode) const override;

    ///@}
    /// @name Options
    ///@{

    /// This prints out an option help line, either positional or optional form
    virtual std::string make_option(const Option *, bool) const;

    /// @brief This is the name part of an option, Default: left column
    virtual std::string make_option_name(const Option *, bool) const;

    /// @brief This is the options part of the name, Default: combined into left column
    virtual std::string make_option_opts(const Option *) const;

    /// @brief This is the description. Default: Right column, on new line if left column too large
    virtual std::string make_option_desc(const Option *) const;

    /// @brief This is used to print the name on the USAGE line
    virtual std::string make_option_usage(const Option *opt) const;

    ///@}
};

// [CLI11:formatter_fwd_hpp:end]
}  // namespace CLI
