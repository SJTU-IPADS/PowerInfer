// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

// CLI Library includes
#include "ConfigFwd.hpp"
#include "Error.hpp"
#include "FormatterFwd.hpp"
#include "Macros.hpp"
#include "Option.hpp"
#include "Split.hpp"
#include "StringTools.hpp"
#include "TypeTools.hpp"

namespace CLI {
// [CLI11:app_hpp:verbatim]

#ifndef CLI11_PARSE
#define CLI11_PARSE(app, ...)                                                                                          \
    try {                                                                                                              \
        (app).parse(__VA_ARGS__);                                                                                      \
    } catch(const CLI::ParseError &e) {                                                                                \
        return (app).exit(e);                                                                                          \
    }
#endif

namespace detail {
enum class Classifier { NONE, POSITIONAL_MARK, SHORT, LONG, WINDOWS_STYLE, SUBCOMMAND, SUBCOMMAND_TERMINATOR };
struct AppFriend;
}  // namespace detail

namespace FailureMessage {
/// Printout a clean, simple message on error (the default in CLI11 1.5+)
CLI11_INLINE std::string simple(const App *app, const Error &e);

/// Printout the full help string on error (if this fn is set, the old default for CLI11)
CLI11_INLINE std::string help(const App *app, const Error &e);
}  // namespace FailureMessage

/// enumeration of modes of how to deal with extras in config files

enum class config_extras_mode : char { error = 0, ignore, ignore_all, capture };

class App;

using App_p = std::shared_ptr<App>;

namespace detail {
/// helper functions for adding in appropriate flag modifiers for add_flag

template <typename T, enable_if_t<!std::is_integral<T>::value || (sizeof(T) <= 1U), detail::enabler> = detail::dummy>
Option *default_flag_modifiers(Option *opt) {
    return opt->always_capture_default();
}

/// summing modifiers
template <typename T, enable_if_t<std::is_integral<T>::value && (sizeof(T) > 1U), detail::enabler> = detail::dummy>
Option *default_flag_modifiers(Option *opt) {
    return opt->multi_option_policy(MultiOptionPolicy::Sum)->default_str("0")->force_callback();
}

}  // namespace detail

class Option_group;
/// Creates a command line program, with very few defaults.
/** To use, create a new `Program()` instance with `argc`, `argv`, and a help description. The templated
 *  add_option methods make it easy to prepare options. Remember to call `.start` before starting your
 * program, so that the options can be evaluated and the help option doesn't accidentally run your program. */
class App {
    friend Option;
    friend detail::AppFriend;

  protected:
    // This library follows the Google style guide for member names ending in underscores

    /// @name Basics
    ///@{

    /// Subcommand name or program name (from parser if name is empty)
    std::string name_{};

    /// Description of the current program/subcommand
    std::string description_{};

    /// If true, allow extra arguments (ie, don't throw an error). INHERITABLE
    bool allow_extras_{false};

    /// If ignore, allow extra arguments in the ini file (ie, don't throw an error). INHERITABLE
    /// if error, error on an extra argument, and if capture feed it to the app
    config_extras_mode allow_config_extras_{config_extras_mode::ignore};

    ///  If true, cease processing on an unrecognized option (implies allow_extras) INHERITABLE
    bool prefix_command_{false};

    /// If set to true the name was automatically generated from the command line vs a user set name
    bool has_automatic_name_{false};

    /// If set to true the subcommand is required to be processed and used, ignored for main app
    bool required_{false};

    /// If set to true the subcommand is disabled and cannot be used, ignored for main app
    bool disabled_{false};

    /// Flag indicating that the pre_parse_callback has been triggered
    bool pre_parse_called_{false};

    /// Flag indicating that the callback for the subcommand should be executed immediately on parse completion which is
    /// before help or ini files are processed. INHERITABLE
    bool immediate_callback_{false};

    /// This is a function that runs prior to the start of parsing
    std::function<void(std::size_t)> pre_parse_callback_{};

    /// This is a function that runs when parsing has finished.
    std::function<void()> parse_complete_callback_{};

    /// This is a function that runs when all processing has completed
    std::function<void()> final_callback_{};

    ///@}
    /// @name Options
    ///@{

    /// The default values for options, customizable and changeable INHERITABLE
    OptionDefaults option_defaults_{};

    /// The list of options, stored locally
    std::vector<Option_p> options_{};

    ///@}
    /// @name Help
    ///@{

    /// Usage to put after program/subcommand description in the help output INHERITABLE
    std::string usage_{};

    /// This is a function that generates a usage to put after program/subcommand description in help output
    std::function<std::string()> usage_callback_{};

    /// Footer to put after all options in the help output INHERITABLE
    std::string footer_{};

    /// This is a function that generates a footer to put after all other options in help output
    std::function<std::string()> footer_callback_{};

    /// A pointer to the help flag if there is one INHERITABLE
    Option *help_ptr_{nullptr};

    /// A pointer to the help all flag if there is one INHERITABLE
    Option *help_all_ptr_{nullptr};

    /// A pointer to a version flag if there is one
    Option *version_ptr_{nullptr};

    /// This is the formatter for help printing. Default provided. INHERITABLE (same pointer)
    std::shared_ptr<FormatterBase> formatter_{new Formatter()};

    /// The error message printing function INHERITABLE
    std::function<std::string(const App *, const Error &e)> failure_message_{FailureMessage::simple};

    ///@}
    /// @name Parsing
    ///@{

    using missing_t = std::vector<std::pair<detail::Classifier, std::string>>;

    /// Pair of classifier, string for missing options. (extra detail is removed on returning from parse)
    ///
    /// This is faster and cleaner than storing just a list of strings and reparsing. This may contain the -- separator.
    missing_t missing_{};

    /// This is a list of pointers to options with the original parse order
    std::vector<Option *> parse_order_{};

    /// This is a list of the subcommands collected, in order
    std::vector<App *> parsed_subcommands_{};

    /// this is a list of subcommands that are exclusionary to this one
    std::set<App *> exclude_subcommands_{};

    /// This is a list of options which are exclusionary to this App, if the options were used this subcommand should
    /// not be
    std::set<Option *> exclude_options_{};

    /// this is a list of subcommands or option groups that are required by this one, the list is not mutual,  the
    /// listed subcommands do not require this one
    std::set<App *> need_subcommands_{};

    /// This is a list of options which are required by this app, the list is not mutual, listed options do not need the
    /// subcommand not be
    std::set<Option *> need_options_{};

    ///@}
    /// @name Subcommands
    ///@{

    /// Storage for subcommand list
    std::vector<App_p> subcommands_{};

    /// If true, the program name is not case-sensitive INHERITABLE
    bool ignore_case_{false};

    /// If true, the program should ignore underscores INHERITABLE
    bool ignore_underscore_{false};

    /// Allow options or other arguments to fallthrough, so that parent commands can collect options after subcommand.
    /// INHERITABLE
    bool fallthrough_{false};

    /// Allow subcommands to fallthrough, so that parent commands can trigger other subcommands after subcommand.
    bool subcommand_fallthrough_{true};

    /// Allow '/' for options for Windows like options. Defaults to true on Windows, false otherwise. INHERITABLE
    bool allow_windows_style_options_{
#ifdef _WIN32
        true
#else
        false
#endif
    };
    /// specify that positional arguments come at the end of the argument sequence not inheritable
    bool positionals_at_end_{false};

    enum class startup_mode : char { stable, enabled, disabled };
    /// specify the startup mode for the app
    /// stable=no change, enabled= startup enabled, disabled=startup disabled
    startup_mode default_startup{startup_mode::stable};

    /// if set to true the subcommand can be triggered via configuration files INHERITABLE
    bool configurable_{false};

    /// If set to true positional options are validated before assigning INHERITABLE
    bool validate_positionals_{false};

    /// If set to true optional vector arguments are validated before assigning INHERITABLE
    bool validate_optional_arguments_{false};

    /// indicator that the subcommand is silent and won't show up in subcommands list
    /// This is potentially useful as a modifier subcommand
    bool silent_{false};

    /// indicator that the subcommand should allow non-standard option arguments, such as -single_dash_flag
    bool allow_non_standard_options_{false};

    /// Counts the number of times this command/subcommand was parsed
    std::uint32_t parsed_{0U};

    /// Minimum required subcommands (not inheritable!)
    std::size_t require_subcommand_min_{0};

    /// Max number of subcommands allowed (parsing stops after this number). 0 is unlimited INHERITABLE
    std::size_t require_subcommand_max_{0};

    /// Minimum required options (not inheritable!)
    std::size_t require_option_min_{0};

    /// Max number of options allowed. 0 is unlimited (not inheritable)
    std::size_t require_option_max_{0};

    /// A pointer to the parent if this is a subcommand
    App *parent_{nullptr};

    /// The group membership INHERITABLE
    std::string group_{"SUBCOMMANDS"};

    /// Alias names for the subcommand
    std::vector<std::string> aliases_{};

    ///@}
    /// @name Config
    ///@{

    /// Pointer to the config option
    Option *config_ptr_{nullptr};

    /// This is the formatter for help printing. Default provided. INHERITABLE (same pointer)
    std::shared_ptr<Config> config_formatter_{new ConfigTOML()};

    ///@}

#ifdef _WIN32
    /// When normalizing argv to UTF-8 on Windows, this is the storage for normalized args.
    std::vector<std::string> normalized_argv_{};

    /// When normalizing argv to UTF-8 on Windows, this is the `char**` value returned to the user.
    std::vector<char *> normalized_argv_view_{};
#endif

    /// Special private constructor for subcommand
    App(std::string app_description, std::string app_name, App *parent);

  public:
    /// @name Basic
    ///@{

    /// Create a new program. Pass in the same arguments as main(), along with a help string.
    explicit App(std::string app_description = "", std::string app_name = "")
        : App(app_description, app_name, nullptr) {
        set_help_flag("-h,--help", "Print this help message and exit");
    }

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    /// virtual destructor
    virtual ~App() = default;

    /// Convert the contents of argv to UTF-8. Only does something on Windows, does nothing elsewhere.
    CLI11_NODISCARD char **ensure_utf8(char **argv);

    /// Set a callback for execution when all parsing and processing has completed
    ///
    /// Due to a bug in c++11,
    /// it is not possible to overload on std::function (fixed in c++14
    /// and backported to c++11 on newer compilers). Use capture by reference
    /// to get a pointer to App if needed.
    App *callback(std::function<void()> app_callback) {
        if(immediate_callback_) {
            parse_complete_callback_ = std::move(app_callback);
        } else {
            final_callback_ = std::move(app_callback);
        }
        return this;
    }

    /// Set a callback for execution when all parsing and processing has completed
    /// aliased as callback
    App *final_callback(std::function<void()> app_callback) {
        final_callback_ = std::move(app_callback);
        return this;
    }

    /// Set a callback to execute when parsing has completed for the app
    ///
    App *parse_complete_callback(std::function<void()> pc_callback) {
        parse_complete_callback_ = std::move(pc_callback);
        return this;
    }

    /// Set a callback to execute prior to parsing.
    ///
    App *preparse_callback(std::function<void(std::size_t)> pp_callback) {
        pre_parse_callback_ = std::move(pp_callback);
        return this;
    }

    /// Set a name for the app (empty will use parser to set the name)
    App *name(std::string app_name = "");

    /// Set an alias for the app
    App *alias(std::string app_name);

    /// Remove the error when extras are left over on the command line.
    App *allow_extras(bool allow = true) {
        allow_extras_ = allow;
        return this;
    }

    /// Remove the error when extras are left over on the command line.
    App *required(bool require = true) {
        required_ = require;
        return this;
    }

    /// Disable the subcommand or option group
    App *disabled(bool disable = true) {
        disabled_ = disable;
        return this;
    }

    /// silence the subcommand from showing up in the processed list
    App *silent(bool silence = true) {
        silent_ = silence;
        return this;
    }

    /// allow non standard option names
    App *allow_non_standard_option_names(bool allowed = true) {
        allow_non_standard_options_ = allowed;
        return this;
    }

    /// Set the subcommand to be disabled by default, so on clear(), at the start of each parse it is disabled
    App *disabled_by_default(bool disable = true) {
        if(disable) {
            default_startup = startup_mode::disabled;
        } else {
            default_startup = (default_startup == startup_mode::enabled) ? startup_mode::enabled : startup_mode::stable;
        }
        return this;
    }

    /// Set the subcommand to be enabled by default, so on clear(), at the start of each parse it is enabled (not
    /// disabled)
    App *enabled_by_default(bool enable = true) {
        if(enable) {
            default_startup = startup_mode::enabled;
        } else {
            default_startup =
                (default_startup == startup_mode::disabled) ? startup_mode::disabled : startup_mode::stable;
        }
        return this;
    }

    /// Set the subcommand callback to be executed immediately on subcommand completion
    App *immediate_callback(bool immediate = true);

    /// Set the subcommand to validate positional arguments before assigning
    App *validate_positionals(bool validate = true) {
        validate_positionals_ = validate;
        return this;
    }

    /// Set the subcommand to validate optional vector arguments before assigning
    App *validate_optional_arguments(bool validate = true) {
        validate_optional_arguments_ = validate;
        return this;
    }

    /// ignore extras in config files
    App *allow_config_extras(bool allow = true) {
        if(allow) {
            allow_config_extras_ = config_extras_mode::capture;
            allow_extras_ = true;
        } else {
            allow_config_extras_ = config_extras_mode::error;
        }
        return this;
    }

    /// ignore extras in config files
    App *allow_config_extras(config_extras_mode mode) {
        allow_config_extras_ = mode;
        return this;
    }

    /// Do not parse anything after the first unrecognized option (if true) all remaining arguments are stored in
    /// remaining args
    App *prefix_command(bool is_prefix = true) {
        prefix_command_ = is_prefix;
        return this;
    }

    /// Ignore case. Subcommands inherit value.
    App *ignore_case(bool value = true);

    /// Allow windows style options, such as `/opt`. First matching short or long name used. Subcommands inherit
    /// value.
    App *allow_windows_style_options(bool value = true) {
        allow_windows_style_options_ = value;
        return this;
    }

    /// Specify that the positional arguments are only at the end of the sequence
    App *positionals_at_end(bool value = true) {
        positionals_at_end_ = value;
        return this;
    }

    /// Specify that the subcommand can be triggered by a config file
    App *configurable(bool value = true) {
        configurable_ = value;
        return this;
    }

    /// Ignore underscore. Subcommands inherit value.
    App *ignore_underscore(bool value = true);

    /// Set the help formatter
    App *formatter(std::shared_ptr<FormatterBase> fmt) {
        formatter_ = fmt;
        return this;
    }

    /// Set the help formatter
    App *formatter_fn(std::function<std::string(const App *, std::string, AppFormatMode)> fmt) {
        formatter_ = std::make_shared<FormatterLambda>(fmt);
        return this;
    }

    /// Set the config formatter
    App *config_formatter(std::shared_ptr<Config> fmt) {
        config_formatter_ = fmt;
        return this;
    }

    /// Check to see if this subcommand was parsed, true only if received on command line.
    CLI11_NODISCARD bool parsed() const { return parsed_ > 0; }

    /// Get the OptionDefault object, to set option defaults
    OptionDefaults *option_defaults() { return &option_defaults_; }

    ///@}
    /// @name Adding options
    ///@{

    /// Add an option, will automatically understand the type for common types.
    ///
    /// To use, create a variable with the expected type, and pass it in after the name.
    /// After start is called, you can use count to see if the value was passed, and
    /// the value will be initialized properly. Numbers, vectors, and strings are supported.
    ///
    /// ->required(), ->default, and the validators are options,
    /// The positional options take an optional number of arguments.
    ///
    /// For example,
    ///
    ///     std::string filename;
    ///     program.add_option("filename", filename, "description of filename");
    ///
    Option *add_option(std::string option_name,
                       callback_t option_callback,
                       std::string option_description = "",
                       bool defaulted = false,
                       std::function<std::string()> func = {});

    /// Add option for assigning to a variable
    template <typename AssignTo,
              typename ConvertTo = AssignTo,
              enable_if_t<!std::is_const<ConvertTo>::value, detail::enabler> = detail::dummy>
    Option *add_option(std::string option_name,
                       AssignTo &variable,  ///< The variable to set
                       std::string option_description = "") {

        auto fun = [&variable](const CLI::results_t &res) {  // comment for spacing
            return detail::lexical_conversion<AssignTo, ConvertTo>(res, variable);
        };

        Option *opt = add_option(option_name, fun, option_description, false, [&variable]() {
            return CLI::detail::checked_to_string<AssignTo, ConvertTo>(variable);
        });
        opt->type_name(detail::type_name<ConvertTo>());
        // these must be actual lvalues since (std::max) sometimes is defined in terms of references and references
        // to structs used in the evaluation can be temporary so that would cause issues.
        auto Tcount = detail::type_count<AssignTo>::value;
        auto XCcount = detail::type_count<ConvertTo>::value;
        opt->type_size(detail::type_count_min<ConvertTo>::value, (std::max)(Tcount, XCcount));
        opt->expected(detail::expected_count<ConvertTo>::value);
        opt->run_callback_for_default();
        return opt;
    }

    /// Add option for assigning to a variable
    template <typename AssignTo, enable_if_t<!std::is_const<AssignTo>::value, detail::enabler> = detail::dummy>
    Option *add_option_no_stream(std::string option_name,
                                 AssignTo &variable,  ///< The variable to set
                                 std::string option_description = "") {

        auto fun = [&variable](const CLI::results_t &res) {  // comment for spacing
            return detail::lexical_conversion<AssignTo, AssignTo>(res, variable);
        };

        Option *opt = add_option(option_name, fun, option_description, false, []() { return std::string{}; });
        opt->type_name(detail::type_name<AssignTo>());
        opt->type_size(detail::type_count_min<AssignTo>::value, detail::type_count<AssignTo>::value);
        opt->expected(detail::expected_count<AssignTo>::value);
        opt->run_callback_for_default();
        return opt;
    }

    /// Add option for a callback of a specific type
    template <typename ArgType>
    Option *add_option_function(std::string option_name,
                                const std::function<void(const ArgType &)> &func,  ///< the callback to execute
                                std::string option_description = "") {

        auto fun = [func](const CLI::results_t &res) {
            ArgType variable;
            bool result = detail::lexical_conversion<ArgType, ArgType>(res, variable);
            if(result) {
                func(variable);
            }
            return result;
        };

        Option *opt = add_option(option_name, std::move(fun), option_description, false);
        opt->type_name(detail::type_name<ArgType>());
        opt->type_size(detail::type_count_min<ArgType>::value, detail::type_count<ArgType>::value);
        opt->expected(detail::expected_count<ArgType>::value);
        return opt;
    }

    /// Add option with no description or variable assignment
    Option *add_option(std::string option_name) {
        return add_option(option_name, CLI::callback_t{}, std::string{}, false);
    }

    /// Add option with description but with no variable assignment or callback
    template <typename T,
              enable_if_t<std::is_const<T>::value && std::is_constructible<std::string, T>::value, detail::enabler> =
                  detail::dummy>
    Option *add_option(std::string option_name, T &option_description) {
        return add_option(option_name, CLI::callback_t(), option_description, false);
    }

    /// Set a help flag, replace the existing one if present
    Option *set_help_flag(std::string flag_name = "", const std::string &help_description = "");

    /// Set a help all flag, replaced the existing one if present
    Option *set_help_all_flag(std::string help_name = "", const std::string &help_description = "");

    /// Set a version flag and version display string, replace the existing one if present
    Option *set_version_flag(std::string flag_name = "",
                             const std::string &versionString = "",
                             const std::string &version_help = "Display program version information and exit");

    /// Generate the version string through a callback function
    Option *set_version_flag(std::string flag_name,
                             std::function<std::string()> vfunc,
                             const std::string &version_help = "Display program version information and exit");

  private:
    /// Internal function for adding a flag
    Option *_add_flag_internal(std::string flag_name, CLI::callback_t fun, std::string flag_description);

  public:
    /// Add a flag with no description or variable assignment
    Option *add_flag(std::string flag_name) { return _add_flag_internal(flag_name, CLI::callback_t(), std::string{}); }

    /// Add flag with description but with no variable assignment or callback
    /// takes a constant string,  if a variable string is passed that variable will be assigned the results from the
    /// flag
    template <typename T,
              enable_if_t<std::is_const<T>::value && std::is_constructible<std::string, T>::value, detail::enabler> =
                  detail::dummy>
    Option *add_flag(std::string flag_name, T &flag_description) {
        return _add_flag_internal(flag_name, CLI::callback_t(), flag_description);
    }

    /// Other type version accepts all other types that are not vectors such as bool, enum, string or other classes
    /// that can be converted from a string
    template <typename T,
              enable_if_t<!detail::is_mutable_container<T>::value && !std::is_const<T>::value &&
                              !std::is_constructible<std::function<void(int)>, T>::value,
                          detail::enabler> = detail::dummy>
    Option *add_flag(std::string flag_name,
                     T &flag_result,  ///< A variable holding the flag result
                     std::string flag_description = "") {

        CLI::callback_t fun = [&flag_result](const CLI::results_t &res) {
            using CLI::detail::lexical_cast;
            return lexical_cast(res[0], flag_result);
        };
        auto *opt = _add_flag_internal(flag_name, std::move(fun), std::move(flag_description));
        return detail::default_flag_modifiers<T>(opt);
    }

    /// Vector version to capture multiple flags.
    template <typename T,
              enable_if_t<!std::is_assignable<std::function<void(std::int64_t)> &, T>::value, detail::enabler> =
                  detail::dummy>
    Option *add_flag(std::string flag_name,
                     std::vector<T> &flag_results,  ///< A vector of values with the flag results
                     std::string flag_description = "") {
        CLI::callback_t fun = [&flag_results](const CLI::results_t &res) {
            bool retval = true;
            for(const auto &elem : res) {
                using CLI::detail::lexical_cast;
                flag_results.emplace_back();
                retval &= lexical_cast(elem, flag_results.back());
            }
            return retval;
        };
        return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description))
            ->multi_option_policy(MultiOptionPolicy::TakeAll)
            ->run_callback_for_default();
    }

    /// Add option for callback that is triggered with a true flag and takes no arguments
    Option *add_flag_callback(std::string flag_name,
                              std::function<void(void)> function,  ///< A function to call, void(void)
                              std::string flag_description = "");

    /// Add option for callback with an integer value
    Option *add_flag_function(std::string flag_name,
                              std::function<void(std::int64_t)> function,  ///< A function to call, void(int)
                              std::string flag_description = "");

#ifdef CLI11_CPP14
    /// Add option for callback (C++14 or better only)
    Option *add_flag(std::string flag_name,
                     std::function<void(std::int64_t)> function,  ///< A function to call, void(std::int64_t)
                     std::string flag_description = "") {
        return add_flag_function(std::move(flag_name), std::move(function), std::move(flag_description));
    }
#endif

    /// Set a configuration ini file option, or clear it if no name passed
    Option *set_config(std::string option_name = "",
                       std::string default_filename = "",
                       const std::string &help_message = "Read an ini file",
                       bool config_required = false);

    /// Removes an option from the App. Takes an option pointer. Returns true if found and removed.
    bool remove_option(Option *opt);

    /// creates an option group as part of the given app
    template <typename T = Option_group>
    T *add_option_group(std::string group_name, std::string group_description = "") {
        if(!detail::valid_alias_name_string(group_name)) {
            throw IncorrectConstruction("option group names may not contain newlines or null characters");
        }
        auto option_group = std::make_shared<T>(std::move(group_description), group_name, this);
        auto *ptr = option_group.get();
        // move to App_p for overload resolution on older gcc versions
        App_p app_ptr = std::dynamic_pointer_cast<App>(option_group);
        add_subcommand(std::move(app_ptr));
        return ptr;
    }

    ///@}
    /// @name Subcommands
    ///@{

    /// Add a subcommand. Inherits INHERITABLE and OptionDefaults, and help flag
    App *add_subcommand(std::string subcommand_name = "", std::string subcommand_description = "");

    /// Add a previously created app as a subcommand
    App *add_subcommand(CLI::App_p subcom);

    /// Removes a subcommand from the App. Takes a subcommand pointer. Returns true if found and removed.
    bool remove_subcommand(App *subcom);

    /// Check to see if a subcommand is part of this command (doesn't have to be in command line)
    /// returns the first subcommand if passed a nullptr
    App *get_subcommand(const App *subcom) const;

    /// Check to see if a subcommand is part of this command (text version)
    CLI11_NODISCARD App *get_subcommand(std::string subcom) const;

    /// Get a subcommand by name (noexcept non-const version)
    /// returns null if subcommand doesn't exist
    CLI11_NODISCARD App *get_subcommand_no_throw(std::string subcom) const noexcept;

    /// Get a pointer to subcommand by index
    CLI11_NODISCARD App *get_subcommand(int index = 0) const;

    /// Check to see if a subcommand is part of this command and get a shared_ptr to it
    CLI::App_p get_subcommand_ptr(App *subcom) const;

    /// Check to see if a subcommand is part of this command (text version)
    CLI11_NODISCARD CLI::App_p get_subcommand_ptr(std::string subcom) const;

    /// Get an owning pointer to subcommand by index
    CLI11_NODISCARD CLI::App_p get_subcommand_ptr(int index = 0) const;

    /// Check to see if an option group is part of this App
    CLI11_NODISCARD App *get_option_group(std::string group_name) const;

    /// No argument version of count counts the number of times this subcommand was
    /// passed in. The main app will return 1. Unnamed subcommands will also return 1 unless
    /// otherwise modified in a callback
    CLI11_NODISCARD std::size_t count() const { return parsed_; }

    /// Get a count of all the arguments processed in options and subcommands, this excludes arguments which were
    /// treated as extras.
    CLI11_NODISCARD std::size_t count_all() const;

    /// Changes the group membership
    App *group(std::string group_name) {
        group_ = group_name;
        return this;
    }

    /// The argumentless form of require subcommand requires 1 or more subcommands
    App *require_subcommand() {
        require_subcommand_min_ = 1;
        require_subcommand_max_ = 0;
        return this;
    }

    /// Require a subcommand to be given (does not affect help call)
    /// The number required can be given. Negative values indicate maximum
    /// number allowed (0 for any number). Max number inheritable.
    App *require_subcommand(int value) {
        if(value < 0) {
            require_subcommand_min_ = 0;
            require_subcommand_max_ = static_cast<std::size_t>(-value);
        } else {
            require_subcommand_min_ = static_cast<std::size_t>(value);
            require_subcommand_max_ = static_cast<std::size_t>(value);
        }
        return this;
    }

    /// Explicitly control the number of subcommands required. Setting 0
    /// for the max means unlimited number allowed. Max number inheritable.
    App *require_subcommand(std::size_t min, std::size_t max) {
        require_subcommand_min_ = min;
        require_subcommand_max_ = max;
        return this;
    }

    /// The argumentless form of require option requires 1 or more options be used
    App *require_option() {
        require_option_min_ = 1;
        require_option_max_ = 0;
        return this;
    }

    /// Require an option to be given (does not affect help call)
    /// The number required can be given. Negative values indicate maximum
    /// number allowed (0 for any number).
    App *require_option(int value) {
        if(value < 0) {
            require_option_min_ = 0;
            require_option_max_ = static_cast<std::size_t>(-value);
        } else {
            require_option_min_ = static_cast<std::size_t>(value);
            require_option_max_ = static_cast<std::size_t>(value);
        }
        return this;
    }

    /// Explicitly control the number of options required. Setting 0
    /// for the max means unlimited number allowed. Max number inheritable.
    App *require_option(std::size_t min, std::size_t max) {
        require_option_min_ = min;
        require_option_max_ = max;
        return this;
    }

    /// Set fallthrough, set to true so that options will fallthrough to parent if not recognized in a subcommand
    /// Default from parent, usually set on parent.
    App *fallthrough(bool value = true) {
        fallthrough_ = value;
        return this;
    }

    /// Set subcommand fallthrough, set to true so that subcommands on parents are recognized
    App *subcommand_fallthrough(bool value = true) {
        subcommand_fallthrough_ = value;
        return this;
    }

    /// Check to see if this subcommand was parsed, true only if received on command line.
    /// This allows the subcommand to be directly checked.
    explicit operator bool() const { return parsed_ > 0; }

    ///@}
    /// @name Extras for subclassing
    ///@{

    /// This allows subclasses to inject code before callbacks but after parse.
    ///
    /// This does not run if any errors or help is thrown.
    virtual void pre_callback() {}

    ///@}
    /// @name Parsing
    ///@{
    //
    /// Reset the parsed data
    void clear();

    /// Parses the command line - throws errors.
    /// This must be called after the options are in but before the rest of the program.
    void parse(int argc, const char *const *argv);
    void parse(int argc, const wchar_t *const *argv);

  private:
    template <class CharT> void parse_char_t(int argc, const CharT *const *argv);

  public:
    /// Parse a single string as if it contained command line arguments.
    /// This function splits the string into arguments then calls parse(std::vector<std::string> &)
    /// the function takes an optional boolean argument specifying if the programName is included in the string to
    /// process
    void parse(std::string commandline, bool program_name_included = false);
    void parse(std::wstring commandline, bool program_name_included = false);

    /// The real work is done here. Expects a reversed vector.
    /// Changes the vector to the remaining options.
    void parse(std::vector<std::string> &args);

    /// The real work is done here. Expects a reversed vector.
    void parse(std::vector<std::string> &&args);

    void parse_from_stream(std::istream &input);

    /// Provide a function to print a help message. The function gets access to the App pointer and error.
    void failure_message(std::function<std::string(const App *, const Error &e)> function) {
        failure_message_ = function;
    }

    /// Print a nice error message and return the exit code
    int exit(const Error &e, std::ostream &out = std::cout, std::ostream &err = std::cerr) const;

    ///@}
    /// @name Post parsing
    ///@{

    /// Counts the number of times the given option was passed.
    CLI11_NODISCARD std::size_t count(std::string option_name) const { return get_option(option_name)->count(); }

    /// Get a subcommand pointer list to the currently selected subcommands (after parsing by default, in command
    /// line order; use parsed = false to get the original definition list.)
    CLI11_NODISCARD std::vector<App *> get_subcommands() const { return parsed_subcommands_; }

    /// Get a filtered subcommand pointer list from the original definition list. An empty function will provide all
    /// subcommands (const)
    std::vector<const App *> get_subcommands(const std::function<bool(const App *)> &filter) const;

    /// Get a filtered subcommand pointer list from the original definition list. An empty function will provide all
    /// subcommands
    std::vector<App *> get_subcommands(const std::function<bool(App *)> &filter);

    /// Check to see if given subcommand was selected
    bool got_subcommand(const App *subcom) const {
        // get subcom needed to verify that this was a real subcommand
        return get_subcommand(subcom)->parsed_ > 0;
    }

    /// Check with name instead of pointer to see if subcommand was selected
    CLI11_NODISCARD bool got_subcommand(std::string subcommand_name) const noexcept {
        App *sub = get_subcommand_no_throw(subcommand_name);
        return (sub != nullptr) ? (sub->parsed_ > 0) : false;
    }

    /// Sets excluded options for the subcommand
    App *excludes(Option *opt) {
        if(opt == nullptr) {
            throw OptionNotFound("nullptr passed");
        }
        exclude_options_.insert(opt);
        return this;
    }

    /// Sets excluded subcommands for the subcommand
    App *excludes(App *app) {
        if(app == nullptr) {
            throw OptionNotFound("nullptr passed");
        }
        if(app == this) {
            throw OptionNotFound("cannot self reference in needs");
        }
        auto res = exclude_subcommands_.insert(app);
        // subcommand exclusion should be symmetric
        if(res.second) {
            app->exclude_subcommands_.insert(this);
        }
        return this;
    }

    App *needs(Option *opt) {
        if(opt == nullptr) {
            throw OptionNotFound("nullptr passed");
        }
        need_options_.insert(opt);
        return this;
    }

    App *needs(App *app) {
        if(app == nullptr) {
            throw OptionNotFound("nullptr passed");
        }
        if(app == this) {
            throw OptionNotFound("cannot self reference in needs");
        }
        need_subcommands_.insert(app);
        return this;
    }

    /// Removes an option from the excludes list of this subcommand
    bool remove_excludes(Option *opt);

    /// Removes a subcommand from the excludes list of this subcommand
    bool remove_excludes(App *app);

    /// Removes an option from the needs list of this subcommand
    bool remove_needs(Option *opt);

    /// Removes a subcommand from the needs list of this subcommand
    bool remove_needs(App *app);
    ///@}
    /// @name Help
    ///@{

    /// Set usage.
    App *usage(std::string usage_string) {
        usage_ = std::move(usage_string);
        return this;
    }
    /// Set usage.
    App *usage(std::function<std::string()> usage_function) {
        usage_callback_ = std::move(usage_function);
        return this;
    }
    /// Set footer.
    App *footer(std::string footer_string) {
        footer_ = std::move(footer_string);
        return this;
    }
    /// Set footer.
    App *footer(std::function<std::string()> footer_function) {
        footer_callback_ = std::move(footer_function);
        return this;
    }
    /// Produce a string that could be read in as a config of the current values of the App. Set default_also to
    /// include default arguments. write_descriptions will print a description for the App and for each option.
    CLI11_NODISCARD std::string config_to_str(bool default_also = false, bool write_description = false) const {
        return config_formatter_->to_config(this, default_also, write_description, "");
    }

    /// Makes a help message, using the currently configured formatter
    /// Will only do one subcommand at a time
    CLI11_NODISCARD std::string help(std::string prev = "", AppFormatMode mode = AppFormatMode::Normal) const;

    /// Displays a version string
    CLI11_NODISCARD std::string version() const;
    ///@}
    /// @name Getters
    ///@{

    /// Access the formatter
    CLI11_NODISCARD std::shared_ptr<FormatterBase> get_formatter() const { return formatter_; }

    /// Access the config formatter
    CLI11_NODISCARD std::shared_ptr<Config> get_config_formatter() const { return config_formatter_; }

    /// Access the config formatter as a configBase pointer
    CLI11_NODISCARD std::shared_ptr<ConfigBase> get_config_formatter_base() const {
        // This is safer as a dynamic_cast if we have RTTI, as Config -> ConfigBase
#if CLI11_USE_STATIC_RTTI == 0
        return std::dynamic_pointer_cast<ConfigBase>(config_formatter_);
#else
        return std::static_pointer_cast<ConfigBase>(config_formatter_);
#endif
    }

    /// Get the app or subcommand description
    CLI11_NODISCARD std::string get_description() const { return description_; }

    /// Set the description of the app
    App *description(std::string app_description) {
        description_ = std::move(app_description);
        return this;
    }

    /// Get the list of options (user facing function, so returns raw pointers), has optional filter function
    std::vector<const Option *> get_options(const std::function<bool(const Option *)> filter = {}) const;

    /// Non-const version of the above
    std::vector<Option *> get_options(const std::function<bool(Option *)> filter = {});

    /// Get an option by name (noexcept non-const version)
    CLI11_NODISCARD Option *get_option_no_throw(std::string option_name) noexcept;

    /// Get an option by name (noexcept const version)
    CLI11_NODISCARD const Option *get_option_no_throw(std::string option_name) const noexcept;

    /// Get an option by name
    CLI11_NODISCARD const Option *get_option(std::string option_name) const {
        const auto *opt = get_option_no_throw(option_name);
        if(opt == nullptr) {
            throw OptionNotFound(option_name);
        }
        return opt;
    }

    /// Get an option by name (non-const version)
    Option *get_option(std::string option_name) {
        auto *opt = get_option_no_throw(option_name);
        if(opt == nullptr) {
            throw OptionNotFound(option_name);
        }
        return opt;
    }

    /// Shortcut bracket operator for getting a pointer to an option
    const Option *operator[](const std::string &option_name) const { return get_option(option_name); }

    /// Shortcut bracket operator for getting a pointer to an option
    const Option *operator[](const char *option_name) const { return get_option(option_name); }

    /// Check the status of ignore_case
    CLI11_NODISCARD bool get_ignore_case() const { return ignore_case_; }

    /// Check the status of ignore_underscore
    CLI11_NODISCARD bool get_ignore_underscore() const { return ignore_underscore_; }

    /// Check the status of fallthrough
    CLI11_NODISCARD bool get_fallthrough() const { return fallthrough_; }

    /// Check the status of subcommand fallthrough
    CLI11_NODISCARD bool get_subcommand_fallthrough() const { return subcommand_fallthrough_; }

    /// Check the status of the allow windows style options
    CLI11_NODISCARD bool get_allow_windows_style_options() const { return allow_windows_style_options_; }

    /// Check the status of the allow windows style options
    CLI11_NODISCARD bool get_positionals_at_end() const { return positionals_at_end_; }

    /// Check the status of the allow windows style options
    CLI11_NODISCARD bool get_configurable() const { return configurable_; }

    /// Get the group of this subcommand
    CLI11_NODISCARD const std::string &get_group() const { return group_; }

    /// Generate and return the usage.
    CLI11_NODISCARD std::string get_usage() const {
        return (usage_callback_) ? usage_callback_() + '\n' + usage_ : usage_;
    }

    /// Generate and return the footer.
    CLI11_NODISCARD std::string get_footer() const {
        return (footer_callback_) ? footer_callback_() + '\n' + footer_ : footer_;
    }

    /// Get the required min subcommand value
    CLI11_NODISCARD std::size_t get_require_subcommand_min() const { return require_subcommand_min_; }

    /// Get the required max subcommand value
    CLI11_NODISCARD std::size_t get_require_subcommand_max() const { return require_subcommand_max_; }

    /// Get the required min option value
    CLI11_NODISCARD std::size_t get_require_option_min() const { return require_option_min_; }

    /// Get the required max option value
    CLI11_NODISCARD std::size_t get_require_option_max() const { return require_option_max_; }

    /// Get the prefix command status
    CLI11_NODISCARD bool get_prefix_command() const { return prefix_command_; }

    /// Get the status of allow extras
    CLI11_NODISCARD bool get_allow_extras() const { return allow_extras_; }

    /// Get the status of required
    CLI11_NODISCARD bool get_required() const { return required_; }

    /// Get the status of disabled
    CLI11_NODISCARD bool get_disabled() const { return disabled_; }

    /// Get the status of silence
    CLI11_NODISCARD bool get_silent() const { return silent_; }

    /// Get the status of silence
    CLI11_NODISCARD bool get_allow_non_standard_option_names() const { return allow_non_standard_options_; }

    /// Get the status of disabled
    CLI11_NODISCARD bool get_immediate_callback() const { return immediate_callback_; }

    /// Get the status of disabled by default
    CLI11_NODISCARD bool get_disabled_by_default() const { return (default_startup == startup_mode::disabled); }

    /// Get the status of disabled by default
    CLI11_NODISCARD bool get_enabled_by_default() const { return (default_startup == startup_mode::enabled); }
    /// Get the status of validating positionals
    CLI11_NODISCARD bool get_validate_positionals() const { return validate_positionals_; }
    /// Get the status of validating optional vector arguments
    CLI11_NODISCARD bool get_validate_optional_arguments() const { return validate_optional_arguments_; }

    /// Get the status of allow extras
    CLI11_NODISCARD config_extras_mode get_allow_config_extras() const { return allow_config_extras_; }

    /// Get a pointer to the help flag.
    Option *get_help_ptr() { return help_ptr_; }

    /// Get a pointer to the help flag. (const)
    CLI11_NODISCARD const Option *get_help_ptr() const { return help_ptr_; }

    /// Get a pointer to the help all flag. (const)
    CLI11_NODISCARD const Option *get_help_all_ptr() const { return help_all_ptr_; }

    /// Get a pointer to the config option.
    Option *get_config_ptr() { return config_ptr_; }

    /// Get a pointer to the config option. (const)
    CLI11_NODISCARD const Option *get_config_ptr() const { return config_ptr_; }

    /// Get a pointer to the version option.
    Option *get_version_ptr() { return version_ptr_; }

    /// Get a pointer to the version option. (const)
    CLI11_NODISCARD const Option *get_version_ptr() const { return version_ptr_; }

    /// Get the parent of this subcommand (or nullptr if main app)
    App *get_parent() { return parent_; }

    /// Get the parent of this subcommand (or nullptr if main app) (const version)
    CLI11_NODISCARD const App *get_parent() const { return parent_; }

    /// Get the name of the current app
    CLI11_NODISCARD const std::string &get_name() const { return name_; }

    /// Get the aliases of the current app
    CLI11_NODISCARD const std::vector<std::string> &get_aliases() const { return aliases_; }

    /// clear all the aliases of the current App
    App *clear_aliases() {
        aliases_.clear();
        return this;
    }

    /// Get a display name for an app
    CLI11_NODISCARD std::string get_display_name(bool with_aliases = false) const;

    /// Check the name, case-insensitive and underscore insensitive if set
    CLI11_NODISCARD bool check_name(std::string name_to_check) const;

    /// Get the groups available directly from this option (in order)
    CLI11_NODISCARD std::vector<std::string> get_groups() const;

    /// This gets a vector of pointers with the original parse order
    CLI11_NODISCARD const std::vector<Option *> &parse_order() const { return parse_order_; }

    /// This returns the missing options from the current subcommand
    CLI11_NODISCARD std::vector<std::string> remaining(bool recurse = false) const;

    /// This returns the missing options in a form ready for processing by another command line program
    CLI11_NODISCARD std::vector<std::string> remaining_for_passthrough(bool recurse = false) const;

    /// This returns the number of remaining options, minus the -- separator
    CLI11_NODISCARD std::size_t remaining_size(bool recurse = false) const;

    ///@}

  protected:
    /// Check the options to make sure there are no conflicts.
    ///
    /// Currently checks to see if multiple positionals exist with unlimited args and checks if the min and max options
    /// are feasible
    void _validate() const;

    /// configure subcommands to enable parsing through the current object
    /// set the correct fallthrough and prefix for nameless subcommands and manage the automatic enable or disable
    /// makes sure parent is set correctly
    void _configure();

    /// Internal function to run (App) callback, bottom up
    void run_callback(bool final_mode = false, bool suppress_final_callback = false);

    /// Check to see if a subcommand is valid. Give up immediately if subcommand max has been reached.
    CLI11_NODISCARD bool _valid_subcommand(const std::string &current, bool ignore_used = true) const;

    /// Selects a Classifier enum based on the type of the current argument
    CLI11_NODISCARD detail::Classifier _recognize(const std::string &current,
                                                  bool ignore_used_subcommands = true) const;

    // The parse function is now broken into several parts, and part of process

    /// Read and process a configuration file (main app only)
    void _process_config_file();

    /// Read and process a particular configuration file
    bool _process_config_file(const std::string &config_file, bool throw_error);

    /// Get envname options if not yet passed. Runs on *all* subcommands.
    void _process_env();

    /// Process callbacks. Runs on *all* subcommands.
    void _process_callbacks();

    /// Run help flag processing if any are found.
    ///
    /// The flags allow recursive calls to remember if there was a help flag on a parent.
    void _process_help_flags(bool trigger_help = false, bool trigger_all_help = false) const;

    /// Verify required options and cross requirements. Subcommands too (only if selected).
    void _process_requirements();

    /// Process callbacks and such.
    void _process();

    /// Throw an error if anything is left over and should not be.
    void _process_extras();

    /// Throw an error if anything is left over and should not be.
    /// Modifies the args to fill in the missing items before throwing.
    void _process_extras(std::vector<std::string> &args);

    /// Internal function to recursively increment the parsed counter on the current app as well unnamed subcommands
    void increment_parsed();

    /// Internal parse function
    void _parse(std::vector<std::string> &args);

    /// Internal parse function
    void _parse(std::vector<std::string> &&args);

    /// Internal function to parse a stream
    void _parse_stream(std::istream &input);

    /// Parse one config param, return false if not found in any subcommand, remove if it is
    ///
    /// If this has more than one dot.separated.name, go into the subcommand matching it
    /// Returns true if it managed to find the option, if false you'll need to remove the arg manually.
    void _parse_config(const std::vector<ConfigItem> &args);

    /// Fill in a single config option
    bool _parse_single_config(const ConfigItem &item, std::size_t level = 0);

    /// Parse "one" argument (some may eat more than one), delegate to parent if fails, add to missing if missing
    /// from main return false if the parse has failed and needs to return to parent
    bool _parse_single(std::vector<std::string> &args, bool &positional_only);

    /// Count the required remaining positional arguments
    CLI11_NODISCARD std::size_t _count_remaining_positionals(bool required_only = false) const;

    /// Count the required remaining positional arguments
    CLI11_NODISCARD bool _has_remaining_positionals() const;

    /// Parse a positional, go up the tree to check
    /// @param haltOnSubcommand if set to true the operation will not process subcommands merely return false
    /// Return true if the positional was used false otherwise
    bool _parse_positional(std::vector<std::string> &args, bool haltOnSubcommand);

    /// Locate a subcommand by name with two conditions, should disabled subcommands be ignored, and should used
    /// subcommands be ignored
    CLI11_NODISCARD App *
    _find_subcommand(const std::string &subc_name, bool ignore_disabled, bool ignore_used) const noexcept;

    /// Parse a subcommand, modify args and continue
    ///
    /// Unlike the others, this one will always allow fallthrough
    /// return true if the subcommand was processed false otherwise
    bool _parse_subcommand(std::vector<std::string> &args);

    /// Parse a short (false) or long (true) argument, must be at the top of the list
    /// if local_processing_only is set to true then fallthrough is disabled will return false if not found
    /// return true if the argument was processed or false if nothing was done
    bool _parse_arg(std::vector<std::string> &args, detail::Classifier current_type, bool local_processing_only);

    /// Trigger the pre_parse callback if needed
    void _trigger_pre_parse(std::size_t remaining_args);

    /// Get the appropriate parent to fallthrough to which is the first one that has a name or the main app
    App *_get_fallthrough_parent();

    /// Helper function to run through all possible comparisons of subcommand names to check there is no overlap
    CLI11_NODISCARD const std::string &_compare_subcommand_names(const App &subcom, const App &base) const;

    /// Helper function to place extra values in the most appropriate position
    void _move_to_missing(detail::Classifier val_type, const std::string &val);

  public:
    /// function that could be used by subclasses of App to shift options around into subcommands
    void _move_option(Option *opt, App *app);
};  // namespace CLI

/// Extension of App to better manage groups of options
class Option_group : public App {
  public:
    Option_group(std::string group_description, std::string group_name, App *parent)
        : App(std::move(group_description), "", parent) {
        group(group_name);
        // option groups should have automatic fallthrough
        if(group_name.empty() || group_name.front() == '+') {
            // help will not be used by default in these contexts
            set_help_flag("");
            set_help_all_flag("");
        }
    }
    using App::add_option;
    /// Add an existing option to the Option_group
    Option *add_option(Option *opt) {
        if(get_parent() == nullptr) {
            throw OptionNotFound("Unable to locate the specified option");
        }
        get_parent()->_move_option(opt, this);
        return opt;
    }
    /// Add an existing option to the Option_group
    void add_options(Option *opt) { add_option(opt); }
    /// Add a bunch of options to the group
    template <typename... Args> void add_options(Option *opt, Args... args) {
        add_option(opt);
        add_options(args...);
    }
    using App::add_subcommand;
    /// Add an existing subcommand to be a member of an option_group
    App *add_subcommand(App *subcom) {
        App_p subc = subcom->get_parent()->get_subcommand_ptr(subcom);
        subc->get_parent()->remove_subcommand(subcom);
        add_subcommand(std::move(subc));
        return subcom;
    }
};

/// Helper function to enable one option group/subcommand when another is used
CLI11_INLINE void TriggerOn(App *trigger_app, App *app_to_enable);

/// Helper function to enable one option group/subcommand when another is used
CLI11_INLINE void TriggerOn(App *trigger_app, std::vector<App *> apps_to_enable);

/// Helper function to disable one option group/subcommand when another is used
CLI11_INLINE void TriggerOff(App *trigger_app, App *app_to_enable);

/// Helper function to disable one option group/subcommand when another is used
CLI11_INLINE void TriggerOff(App *trigger_app, std::vector<App *> apps_to_enable);

/// Helper function to mark an option as deprecated
CLI11_INLINE void deprecate_option(Option *opt, const std::string &replacement = "");

/// Helper function to mark an option as deprecated
inline void deprecate_option(App *app, const std::string &option_name, const std::string &replacement = "") {
    auto *opt = app->get_option(option_name);
    deprecate_option(opt, replacement);
}

/// Helper function to mark an option as deprecated
inline void deprecate_option(App &app, const std::string &option_name, const std::string &replacement = "") {
    auto *opt = app.get_option(option_name);
    deprecate_option(opt, replacement);
}

/// Helper function to mark an option as retired
CLI11_INLINE void retire_option(App *app, Option *opt);

/// Helper function to mark an option as retired
CLI11_INLINE void retire_option(App &app, Option *opt);

/// Helper function to mark an option as retired
CLI11_INLINE void retire_option(App *app, const std::string &option_name);

/// Helper function to mark an option as retired
CLI11_INLINE void retire_option(App &app, const std::string &option_name);

namespace detail {
/// This class is simply to allow tests access to App's protected functions
struct AppFriend {
#ifdef CLI11_CPP14

    /// Wrap _parse_short, perfectly forward arguments and return
    template <typename... Args> static decltype(auto) parse_arg(App *app, Args &&...args) {
        return app->_parse_arg(std::forward<Args>(args)...);
    }

    /// Wrap _parse_subcommand, perfectly forward arguments and return
    template <typename... Args> static decltype(auto) parse_subcommand(App *app, Args &&...args) {
        return app->_parse_subcommand(std::forward<Args>(args)...);
    }
#else
    /// Wrap _parse_short, perfectly forward arguments and return
    template <typename... Args>
    static auto parse_arg(App *app, Args &&...args) ->
        typename std::result_of<decltype (&App::_parse_arg)(App, Args...)>::type {
        return app->_parse_arg(std::forward<Args>(args)...);
    }

    /// Wrap _parse_subcommand, perfectly forward arguments and return
    template <typename... Args>
    static auto parse_subcommand(App *app, Args &&...args) ->
        typename std::result_of<decltype (&App::_parse_subcommand)(App, Args...)>::type {
        return app->_parse_subcommand(std::forward<Args>(args)...);
    }
#endif
    /// Wrap the fallthrough parent function to make sure that is working correctly
    static App *get_fallthrough_parent(App *app) { return app->_get_fallthrough_parent(); }
};
}  // namespace detail

// [CLI11:app_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/App_inl.hpp"  // IWYU pragma: export
#endif
