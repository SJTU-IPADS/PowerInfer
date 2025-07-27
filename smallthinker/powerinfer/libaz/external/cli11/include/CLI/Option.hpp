// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

#include "Error.hpp"
#include "Macros.hpp"
#include "Split.hpp"
#include "StringTools.hpp"
#include "Validators.hpp"

namespace CLI {
// [CLI11:option_hpp:verbatim]

using results_t = std::vector<std::string>;
/// callback function definition
using callback_t = std::function<bool(const results_t &)>;

class Option;
class App;

using Option_p = std::unique_ptr<Option>;
/// Enumeration of the multiOption Policy selection
enum class MultiOptionPolicy : char {
    Throw,      //!< Throw an error if any extra arguments were given
    TakeLast,   //!< take only the last Expected number of arguments
    TakeFirst,  //!< take only the first Expected number of arguments
    Join,       //!< merge all the arguments together into a single string via the delimiter character default('\n')
    TakeAll,    //!< just get all the passed argument regardless
    Sum,        //!< sum all the arguments together if numerical or concatenate directly without delimiter
    Reverse,    //!< take only the last Expected number of arguments in reverse order
};

/// This is the CRTP base class for Option and OptionDefaults. It was designed this way
/// to share parts of the class; an OptionDefaults can copy to an Option.
template <typename CRTP> class OptionBase {
    friend App;

  protected:
    /// The group membership
    std::string group_ = std::string("OPTIONS");

    /// True if this is a required option
    bool required_{false};

    /// Ignore the case when matching (option, not value)
    bool ignore_case_{false};

    /// Ignore underscores when matching (option, not value)
    bool ignore_underscore_{false};

    /// Allow this option to be given in a configuration file
    bool configurable_{true};

    /// Disable overriding flag values with '=value'
    bool disable_flag_override_{false};

    /// Specify a delimiter character for vector arguments
    char delimiter_{'\0'};

    /// Automatically capture default value
    bool always_capture_default_{false};

    /// Policy for handling multiple arguments beyond the expected Max
    MultiOptionPolicy multi_option_policy_{MultiOptionPolicy::Throw};

    /// Copy the contents to another similar class (one based on OptionBase)
    template <typename T> void copy_to(T *other) const;

  public:
    // setters

    /// Changes the group membership
    CRTP *group(const std::string &name) {
        if(!detail::valid_alias_name_string(name)) {
            throw IncorrectConstruction("Group names may not contain newlines or null characters");
        }
        group_ = name;
        return static_cast<CRTP *>(this);
    }

    /// Set the option as required
    CRTP *required(bool value = true) {
        required_ = value;
        return static_cast<CRTP *>(this);
    }

    /// Support Plumbum term
    CRTP *mandatory(bool value = true) { return required(value); }

    CRTP *always_capture_default(bool value = true) {
        always_capture_default_ = value;
        return static_cast<CRTP *>(this);
    }

    // Getters

    /// Get the group of this option
    CLI11_NODISCARD const std::string &get_group() const { return group_; }

    /// True if this is a required option
    CLI11_NODISCARD bool get_required() const { return required_; }

    /// The status of ignore case
    CLI11_NODISCARD bool get_ignore_case() const { return ignore_case_; }

    /// The status of ignore_underscore
    CLI11_NODISCARD bool get_ignore_underscore() const { return ignore_underscore_; }

    /// The status of configurable
    CLI11_NODISCARD bool get_configurable() const { return configurable_; }

    /// The status of configurable
    CLI11_NODISCARD bool get_disable_flag_override() const { return disable_flag_override_; }

    /// Get the current delimiter char
    CLI11_NODISCARD char get_delimiter() const { return delimiter_; }

    /// Return true if this will automatically capture the default value for help printing
    CLI11_NODISCARD bool get_always_capture_default() const { return always_capture_default_; }

    /// The status of the multi option policy
    CLI11_NODISCARD MultiOptionPolicy get_multi_option_policy() const { return multi_option_policy_; }

    // Shortcuts for multi option policy

    /// Set the multi option policy to take last
    CRTP *take_last() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeLast);
        return self;
    }

    /// Set the multi option policy to take last
    CRTP *take_first() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeFirst);
        return self;
    }

    /// Set the multi option policy to take all arguments
    CRTP *take_all() {
        auto self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::TakeAll);
        return self;
    }

    /// Set the multi option policy to join
    CRTP *join() {
        auto *self = static_cast<CRTP *>(this);
        self->multi_option_policy(MultiOptionPolicy::Join);
        return self;
    }

    /// Set the multi option policy to join with a specific delimiter
    CRTP *join(char delim) {
        auto self = static_cast<CRTP *>(this);
        self->delimiter_ = delim;
        self->multi_option_policy(MultiOptionPolicy::Join);
        return self;
    }

    /// Allow in a configuration file
    CRTP *configurable(bool value = true) {
        configurable_ = value;
        return static_cast<CRTP *>(this);
    }

    /// Allow in a configuration file
    CRTP *delimiter(char value = '\0') {
        delimiter_ = value;
        return static_cast<CRTP *>(this);
    }
};

/// This is a version of OptionBase that only supports setting values,
/// for defaults. It is stored as the default option in an App.
class OptionDefaults : public OptionBase<OptionDefaults> {
  public:
    OptionDefaults() = default;

    // Methods here need a different implementation if they are Option vs. OptionDefault

    /// Take the last argument if given multiple times
    OptionDefaults *multi_option_policy(MultiOptionPolicy value = MultiOptionPolicy::Throw) {
        multi_option_policy_ = value;
        return this;
    }

    /// Ignore the case of the option name
    OptionDefaults *ignore_case(bool value = true) {
        ignore_case_ = value;
        return this;
    }

    /// Ignore underscores in the option name
    OptionDefaults *ignore_underscore(bool value = true) {
        ignore_underscore_ = value;
        return this;
    }

    /// Disable overriding flag values with an '=<value>' segment
    OptionDefaults *disable_flag_override(bool value = true) {
        disable_flag_override_ = value;
        return this;
    }

    /// set a delimiter character to split up single arguments to treat as multiple inputs
    OptionDefaults *delimiter(char value = '\0') {
        delimiter_ = value;
        return this;
    }
};

class Option : public OptionBase<Option> {
    friend App;

  protected:
    /// @name Names
    ///@{

    /// A list of the short names (`-a`) without the leading dashes
    std::vector<std::string> snames_{};

    /// A list of the long names (`--long`) without the leading dashes
    std::vector<std::string> lnames_{};

    /// A list of the flag names with the appropriate default value, the first part of the pair should be duplicates of
    /// what is in snames or lnames but will trigger a particular response on a flag
    std::vector<std::pair<std::string, std::string>> default_flag_values_{};

    /// a list of flag names with specified default values;
    std::vector<std::string> fnames_{};

    /// A positional name
    std::string pname_{};

    /// If given, check the environment for this option
    std::string envname_{};

    ///@}
    /// @name Help
    ///@{

    /// The description for help strings
    std::string description_{};

    /// A human readable default value, either manually set, captured, or captured by default
    std::string default_str_{};

    /// If given, replace the text that describes the option type and usage in the help text
    std::string option_text_{};

    /// A human readable type value, set when App creates this
    ///
    /// This is a lambda function so "types" can be dynamic, such as when a set prints its contents.
    std::function<std::string()> type_name_{[]() { return std::string(); }};

    /// Run this function to capture a default (ignore if empty)
    std::function<std::string()> default_function_{};

    ///@}
    /// @name Configuration
    ///@{

    /// The number of arguments that make up one option. max is the nominal type size, min is the minimum number of
    /// strings
    int type_size_max_{1};
    /// The minimum number of arguments an option should be expecting
    int type_size_min_{1};

    /// The minimum number of expected values
    int expected_min_{1};
    /// The maximum number of expected values
    int expected_max_{1};

    /// A list of Validators to run on each value parsed
    std::vector<Validator> validators_{};

    /// A list of options that are required with this option
    std::set<Option *> needs_{};

    /// A list of options that are excluded with this option
    std::set<Option *> excludes_{};

    ///@}
    /// @name Other
    ///@{

    /// link back up to the parent App for fallthrough
    App *parent_{nullptr};

    /// Options store a callback to do all the work
    callback_t callback_{};

    ///@}
    /// @name Parsing results
    ///@{

    /// complete Results of parsing
    results_t results_{};
    /// results after reduction
    results_t proc_results_{};
    /// enumeration for the option state machine
    enum class option_state : char {
        parsing = 0,       //!< The option is currently collecting parsed results
        validated = 2,     //!< the results have been validated
        reduced = 4,       //!< a subset of results has been generated
        callback_run = 6,  //!< the callback has been executed
    };
    /// Whether the callback has run (needed for INI parsing)
    option_state current_option_state_{option_state::parsing};
    /// Specify that extra args beyond type_size_max should be allowed
    bool allow_extra_args_{false};
    /// Specify that the option should act like a flag vs regular option
    bool flag_like_{false};
    /// Control option to run the callback to set the default
    bool run_callback_for_default_{false};
    /// flag indicating a separator needs to be injected after each argument call
    bool inject_separator_{false};
    /// flag indicating that the option should trigger the validation and callback chain on each result when loaded
    bool trigger_on_result_{false};
    /// flag indicating that the option should force the callback regardless if any results present
    bool force_callback_{false};
    ///@}

    /// Making an option by hand is not defined, it must be made by the App class
    Option(std::string option_name,
           std::string option_description,
           callback_t callback,
           App *parent,
           bool allow_non_standard = false)
        : description_(std::move(option_description)), parent_(parent), callback_(std::move(callback)) {
        std::tie(snames_, lnames_, pname_) = detail::get_names(detail::split_names(option_name), allow_non_standard);
    }

  public:
    /// @name Basic
    ///@{

    Option(const Option &) = delete;
    Option &operator=(const Option &) = delete;

    /// Count the total number of times an option was passed
    CLI11_NODISCARD std::size_t count() const { return results_.size(); }

    /// True if the option was not passed
    CLI11_NODISCARD bool empty() const { return results_.empty(); }

    /// This bool operator returns true if any arguments were passed or the option callback is forced
    explicit operator bool() const { return !empty() || force_callback_; }

    /// Clear the parsed results (mostly for testing)
    void clear() {
        results_.clear();
        current_option_state_ = option_state::parsing;
    }

    ///@}
    /// @name Setting options
    ///@{

    /// Set the number of expected arguments
    Option *expected(int value);

    /// Set the range of expected arguments
    Option *expected(int value_min, int value_max);

    /// Set the value of allow_extra_args which allows extra value arguments on the flag or option to be included
    /// with each instance
    Option *allow_extra_args(bool value = true) {
        allow_extra_args_ = value;
        return this;
    }
    /// Get the current value of allow extra args
    CLI11_NODISCARD bool get_allow_extra_args() const { return allow_extra_args_; }
    /// Set the value of trigger_on_parse which specifies that the option callback should be triggered on every parse
    Option *trigger_on_parse(bool value = true) {
        trigger_on_result_ = value;
        return this;
    }
    /// The status of trigger on parse
    CLI11_NODISCARD bool get_trigger_on_parse() const { return trigger_on_result_; }

    /// Set the value of force_callback
    Option *force_callback(bool value = true) {
        force_callback_ = value;
        return this;
    }
    /// The status of force_callback
    CLI11_NODISCARD bool get_force_callback() const { return force_callback_; }

    /// Set the value of run_callback_for_default which controls whether the callback function should be called to set
    /// the default This is controlled automatically but could be manipulated by the user.
    Option *run_callback_for_default(bool value = true) {
        run_callback_for_default_ = value;
        return this;
    }
    /// Get the current value of run_callback_for_default
    CLI11_NODISCARD bool get_run_callback_for_default() const { return run_callback_for_default_; }

    /// Adds a Validator with a built in type name
    Option *check(Validator validator, const std::string &validator_name = "");

    /// Adds a Validator. Takes a const string& and returns an error message (empty if conversion/check is okay).
    Option *check(std::function<std::string(const std::string &)> Validator,
                  std::string Validator_description = "",
                  std::string Validator_name = "");

    /// Adds a transforming Validator with a built in type name
    Option *transform(Validator Validator, const std::string &Validator_name = "");

    /// Adds a Validator-like function that can change result
    Option *transform(const std::function<std::string(std::string)> &func,
                      std::string transform_description = "",
                      std::string transform_name = "");

    /// Adds a user supplied function to run on each item passed in (communicate though lambda capture)
    Option *each(const std::function<void(std::string)> &func);

    /// Get a named Validator
    Validator *get_validator(const std::string &Validator_name = "");

    /// Get a Validator by index NOTE: this may not be the order of definition
    Validator *get_validator(int index);

    /// Sets required options
    Option *needs(Option *opt) {
        if(opt != this) {
            needs_.insert(opt);
        }
        return this;
    }

    /// Can find a string if needed
    template <typename T = App> Option *needs(std::string opt_name) {
        auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
        if(opt == nullptr) {
            throw IncorrectConstruction::MissingOption(opt_name);
        }
        return needs(opt);
    }

    /// Any number supported, any mix of string and Opt
    template <typename A, typename B, typename... ARG> Option *needs(A opt, B opt1, ARG... args) {
        needs(opt);
        return needs(opt1, args...);  // NOLINT(readability-suspicious-call-argument)
    }

    /// Remove needs link from an option. Returns true if the option really was in the needs list.
    bool remove_needs(Option *opt);

    /// Sets excluded options
    Option *excludes(Option *opt);

    /// Can find a string if needed
    template <typename T = App> Option *excludes(std::string opt_name) {
        auto opt = static_cast<T *>(parent_)->get_option_no_throw(opt_name);
        if(opt == nullptr) {
            throw IncorrectConstruction::MissingOption(opt_name);
        }
        return excludes(opt);
    }

    /// Any number supported, any mix of string and Opt
    template <typename A, typename B, typename... ARG> Option *excludes(A opt, B opt1, ARG... args) {
        excludes(opt);
        return excludes(opt1, args...);
    }

    /// Remove needs link from an option. Returns true if the option really was in the needs list.
    bool remove_excludes(Option *opt);

    /// Sets environment variable to read if no option given
    Option *envname(std::string name) {
        envname_ = std::move(name);
        return this;
    }

    /// Ignore case
    ///
    /// The template hides the fact that we don't have the definition of App yet.
    /// You are never expected to add an argument to the template here.
    template <typename T = App> Option *ignore_case(bool value = true);

    /// Ignore underscores in the option names
    ///
    /// The template hides the fact that we don't have the definition of App yet.
    /// You are never expected to add an argument to the template here.
    template <typename T = App> Option *ignore_underscore(bool value = true);

    /// Take the last argument if given multiple times (or another policy)
    Option *multi_option_policy(MultiOptionPolicy value = MultiOptionPolicy::Throw);

    /// Disable flag overrides values, e.g. --flag=<value> is not allowed
    Option *disable_flag_override(bool value = true) {
        disable_flag_override_ = value;
        return this;
    }
    ///@}
    /// @name Accessors
    ///@{

    /// The number of arguments the option expects
    CLI11_NODISCARD int get_type_size() const { return type_size_min_; }

    /// The minimum number of arguments the option expects
    CLI11_NODISCARD int get_type_size_min() const { return type_size_min_; }
    /// The maximum number of arguments the option expects
    CLI11_NODISCARD int get_type_size_max() const { return type_size_max_; }

    /// Return the inject_separator flag
    CLI11_NODISCARD bool get_inject_separator() const { return inject_separator_; }

    /// The environment variable associated to this value
    CLI11_NODISCARD std::string get_envname() const { return envname_; }

    /// The set of options needed
    CLI11_NODISCARD std::set<Option *> get_needs() const { return needs_; }

    /// The set of options excluded
    CLI11_NODISCARD std::set<Option *> get_excludes() const { return excludes_; }

    /// The default value (for help printing)
    CLI11_NODISCARD std::string get_default_str() const { return default_str_; }

    /// Get the callback function
    CLI11_NODISCARD callback_t get_callback() const { return callback_; }

    /// Get the long names
    CLI11_NODISCARD const std::vector<std::string> &get_lnames() const { return lnames_; }

    /// Get the short names
    CLI11_NODISCARD const std::vector<std::string> &get_snames() const { return snames_; }

    /// Get the flag names with specified default values
    CLI11_NODISCARD const std::vector<std::string> &get_fnames() const { return fnames_; }
    /// Get a single name for the option, first of lname, sname, pname, envname
    CLI11_NODISCARD const std::string &get_single_name() const {
        if(!lnames_.empty()) {
            return lnames_[0];
        }
        if(!snames_.empty()) {
            return snames_[0];
        }
        if(!pname_.empty()) {
            return pname_;
        }
        return envname_;
    }
    /// The number of times the option expects to be included
    CLI11_NODISCARD int get_expected() const { return expected_min_; }

    /// The number of times the option expects to be included
    CLI11_NODISCARD int get_expected_min() const { return expected_min_; }
    /// The max number of times the option expects to be included
    CLI11_NODISCARD int get_expected_max() const { return expected_max_; }

    /// The total min number of expected  string values to be used
    CLI11_NODISCARD int get_items_expected_min() const { return type_size_min_ * expected_min_; }

    /// Get the maximum number of items expected to be returned and used for the callback
    CLI11_NODISCARD int get_items_expected_max() const {
        int t = type_size_max_;
        return detail::checked_multiply(t, expected_max_) ? t : detail::expected_max_vector_size;
    }
    /// The total min number of expected  string values to be used
    CLI11_NODISCARD int get_items_expected() const { return get_items_expected_min(); }

    /// True if the argument can be given directly
    CLI11_NODISCARD bool get_positional() const { return !pname_.empty(); }

    /// True if option has at least one non-positional name
    CLI11_NODISCARD bool nonpositional() const { return (!lnames_.empty() || !snames_.empty()); }

    /// True if option has description
    CLI11_NODISCARD bool has_description() const { return !description_.empty(); }

    /// Get the description
    CLI11_NODISCARD const std::string &get_description() const { return description_; }

    /// Set the description
    Option *description(std::string option_description) {
        description_ = std::move(option_description);
        return this;
    }

    Option *option_text(std::string text) {
        option_text_ = std::move(text);
        return this;
    }

    CLI11_NODISCARD const std::string &get_option_text() const { return option_text_; }

    ///@}
    /// @name Help tools
    ///@{

    /// \brief Gets a comma separated list of names.
    /// Will include / prefer the positional name if positional is true.
    /// If all_options is false, pick just the most descriptive name to show.
    /// Use `get_name(true)` to get the positional name (replaces `get_pname`)
    CLI11_NODISCARD std::string get_name(bool positional = false,  ///< Show the positional name
                                         bool all_options = false  ///< Show every option
    ) const;

    ///@}
    /// @name Parser tools
    ///@{

    /// Process the callback
    void run_callback();

    /// If options share any of the same names, find it
    CLI11_NODISCARD const std::string &matching_name(const Option &other) const;

    /// If options share any of the same names, they are equal (not counting positional)
    bool operator==(const Option &other) const { return !matching_name(other).empty(); }

    /// Check a name. Requires "-" or "--" for short / long, supports positional name
    CLI11_NODISCARD bool check_name(const std::string &name) const;

    /// Requires "-" to be removed from string
    CLI11_NODISCARD bool check_sname(std::string name) const {
        return (detail::find_member(std::move(name), snames_, ignore_case_) >= 0);
    }

    /// Requires "--" to be removed from string
    CLI11_NODISCARD bool check_lname(std::string name) const {
        return (detail::find_member(std::move(name), lnames_, ignore_case_, ignore_underscore_) >= 0);
    }

    /// Requires "--" to be removed from string
    CLI11_NODISCARD bool check_fname(std::string name) const {
        if(fnames_.empty()) {
            return false;
        }
        return (detail::find_member(std::move(name), fnames_, ignore_case_, ignore_underscore_) >= 0);
    }

    /// Get the value that goes for a flag, nominally gets the default value but allows for overrides if not
    /// disabled
    CLI11_NODISCARD std::string get_flag_value(const std::string &name, std::string input_value) const;

    /// Puts a result at the end
    Option *add_result(std::string s);

    /// Puts a result at the end and get a count of the number of arguments actually added
    Option *add_result(std::string s, int &results_added);

    /// Puts a result at the end
    Option *add_result(std::vector<std::string> s);

    /// Get the current complete results set
    CLI11_NODISCARD const results_t &results() const { return results_; }

    /// Get a copy of the results
    CLI11_NODISCARD results_t reduced_results() const;

    /// Get the results as a specified type
    template <typename T> void results(T &output) const {
        bool retval = false;
        if(current_option_state_ >= option_state::reduced || (results_.size() == 1 && validators_.empty())) {
            const results_t &res = (proc_results_.empty()) ? results_ : proc_results_;
            retval = detail::lexical_conversion<T, T>(res, output);
        } else {
            results_t res;
            if(results_.empty()) {
                if(!default_str_.empty()) {
                    // _add_results takes an rvalue only
                    _add_result(std::string(default_str_), res);
                    _validate_results(res);
                    results_t extra;
                    _reduce_results(extra, res);
                    if(!extra.empty()) {
                        res = std::move(extra);
                    }
                } else {
                    res.emplace_back();
                }
            } else {
                res = reduced_results();
            }
            retval = detail::lexical_conversion<T, T>(res, output);
        }
        if(!retval) {
            throw ConversionError(get_name(), results_);
        }
    }

    /// Return the results as the specified type
    template <typename T> CLI11_NODISCARD T as() const {
        T output;
        results(output);
        return output;
    }

    /// See if the callback has been run already
    CLI11_NODISCARD bool get_callback_run() const { return (current_option_state_ == option_state::callback_run); }

    ///@}
    /// @name Custom options
    ///@{

    /// Set the type function to run when displayed on this option
    Option *type_name_fn(std::function<std::string()> typefun) {
        type_name_ = std::move(typefun);
        return this;
    }

    /// Set a custom option typestring
    Option *type_name(std::string typeval) {
        type_name_fn([typeval]() { return typeval; });
        return this;
    }

    /// Set a custom option size
    Option *type_size(int option_type_size);

    /// Set a custom option type size range
    Option *type_size(int option_type_size_min, int option_type_size_max);

    /// Set the value of the separator injection flag
    void inject_separator(bool value = true) { inject_separator_ = value; }

    /// Set a capture function for the default. Mostly used by App.
    Option *default_function(const std::function<std::string()> &func) {
        default_function_ = func;
        return this;
    }

    /// Capture the default value from the original value (if it can be captured)
    Option *capture_default_str() {
        if(default_function_) {
            default_str_ = default_function_();
        }
        return this;
    }

    /// Set the default value string representation (does not change the contained value)
    Option *default_str(std::string val) {
        default_str_ = std::move(val);
        return this;
    }

    /// Set the default value and validate the results and run the callback if appropriate to set the value into the
    /// bound value only available for types that can be converted to a string
    template <typename X> Option *default_val(const X &val) {
        std::string val_str = detail::to_string(val);
        auto old_option_state = current_option_state_;
        results_t old_results{std::move(results_)};
        results_.clear();
        try {
            add_result(val_str);
            // if trigger_on_result_ is set the callback already ran
            if(run_callback_for_default_ && !trigger_on_result_) {
                run_callback();  // run callback sets the state, we need to reset it again
                current_option_state_ = option_state::parsing;
            } else {
                _validate_results(results_);
                current_option_state_ = old_option_state;
            }
        } catch(const ValidationError &err) {
            // this should be done
            results_ = std::move(old_results);
            current_option_state_ = old_option_state;
            // try an alternate way to convert
            std::string alternate = detail::value_string(val);
            if(!alternate.empty() && alternate != val_str) {
                return default_val(alternate);
            }

            throw ValidationError(get_name(),
                                  std::string("given default value does not pass validation :") + err.what());
        } catch(const ConversionError &err) {
            // this should be done
            results_ = std::move(old_results);
            current_option_state_ = old_option_state;

            throw ConversionError(
                get_name(), std::string("given default value(\"") + val_str + "\") produces an error : " + err.what());
        } catch(const CLI::Error &) {
            results_ = std::move(old_results);
            current_option_state_ = old_option_state;
            throw;
        }
        results_ = std::move(old_results);
        default_str_ = std::move(val_str);
        return this;
    }

    /// Get the full typename for this option
    CLI11_NODISCARD std::string get_type_name() const;

  private:
    /// Run the results through the Validators
    void _validate_results(results_t &res) const;

    /** reduce the results in accordance with the MultiOptionPolicy
    @param[out] out results are assigned to res if there if they are different
    */
    void _reduce_results(results_t &out, const results_t &original) const;

    // Run a result through the Validators
    std::string _validate(std::string &result, int index) const;

    /// Add a single result to the result set, taking into account delimiters
    int _add_result(std::string &&result, std::vector<std::string> &res) const;
};

// [CLI11:option_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Option_inl.hpp"  // IWYU pragma: export
#endif
