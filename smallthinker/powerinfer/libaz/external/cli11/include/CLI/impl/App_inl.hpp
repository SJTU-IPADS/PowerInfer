// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../App.hpp"

#include "../Argv.hpp"
#include "../Encoding.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

namespace CLI {
// [CLI11:app_inl_hpp:verbatim]

CLI11_INLINE App::App(std::string app_description, std::string app_name, App *parent)
    : name_(std::move(app_name)), description_(std::move(app_description)), parent_(parent) {
    // Inherit if not from a nullptr
    if(parent_ != nullptr) {
        if(parent_->help_ptr_ != nullptr)
            set_help_flag(parent_->help_ptr_->get_name(false, true), parent_->help_ptr_->get_description());
        if(parent_->help_all_ptr_ != nullptr)
            set_help_all_flag(parent_->help_all_ptr_->get_name(false, true), parent_->help_all_ptr_->get_description());

        /// OptionDefaults
        option_defaults_ = parent_->option_defaults_;

        // INHERITABLE
        failure_message_ = parent_->failure_message_;
        allow_extras_ = parent_->allow_extras_;
        allow_config_extras_ = parent_->allow_config_extras_;
        prefix_command_ = parent_->prefix_command_;
        immediate_callback_ = parent_->immediate_callback_;
        ignore_case_ = parent_->ignore_case_;
        ignore_underscore_ = parent_->ignore_underscore_;
        fallthrough_ = parent_->fallthrough_;
        validate_positionals_ = parent_->validate_positionals_;
        validate_optional_arguments_ = parent_->validate_optional_arguments_;
        configurable_ = parent_->configurable_;
        allow_windows_style_options_ = parent_->allow_windows_style_options_;
        group_ = parent_->group_;
        usage_ = parent_->usage_;
        footer_ = parent_->footer_;
        formatter_ = parent_->formatter_;
        config_formatter_ = parent_->config_formatter_;
        require_subcommand_max_ = parent_->require_subcommand_max_;
    }
}

CLI11_NODISCARD CLI11_INLINE char **App::ensure_utf8(char **argv) {
#ifdef _WIN32
    (void)argv;

    normalized_argv_ = detail::compute_win32_argv();

    if(!normalized_argv_view_.empty()) {
        normalized_argv_view_.clear();
    }

    normalized_argv_view_.reserve(normalized_argv_.size());
    for(auto &arg : normalized_argv_) {
        // using const_cast is well-defined, string is known to not be const.
        normalized_argv_view_.push_back(const_cast<char *>(arg.data()));
    }

    return normalized_argv_view_.data();
#else
    return argv;
#endif
}

CLI11_INLINE App *App::name(std::string app_name) {

    if(parent_ != nullptr) {
        std::string oname = name_;
        name_ = app_name;
        const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
        if(!res.empty()) {
            name_ = oname;
            throw(OptionAlreadyAdded(app_name + " conflicts with existing subcommand names"));
        }
    } else {
        name_ = app_name;
    }
    has_automatic_name_ = false;
    return this;
}

CLI11_INLINE App *App::alias(std::string app_name) {
    if(app_name.empty() || !detail::valid_alias_name_string(app_name)) {
        throw IncorrectConstruction("Aliases may not be empty or contain newlines or null characters");
    }
    if(parent_ != nullptr) {
        aliases_.push_back(app_name);
        const auto &res = _compare_subcommand_names(*this, *_get_fallthrough_parent());
        if(!res.empty()) {
            aliases_.pop_back();
            throw(OptionAlreadyAdded("alias already matches an existing subcommand: " + app_name));
        }
    } else {
        aliases_.push_back(app_name);
    }

    return this;
}

CLI11_INLINE App *App::immediate_callback(bool immediate) {
    immediate_callback_ = immediate;
    if(immediate_callback_) {
        if(final_callback_ && !(parse_complete_callback_)) {
            std::swap(final_callback_, parse_complete_callback_);
        }
    } else if(!(final_callback_) && parse_complete_callback_) {
        std::swap(final_callback_, parse_complete_callback_);
    }
    return this;
}

CLI11_INLINE App *App::ignore_case(bool value) {
    if(value && !ignore_case_) {
        ignore_case_ = true;
        auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
        const auto &match = _compare_subcommand_names(*this, *p);
        if(!match.empty()) {
            ignore_case_ = false;  // we are throwing so need to be exception invariant
            throw OptionAlreadyAdded("ignore case would cause subcommand name conflicts: " + match);
        }
    }
    ignore_case_ = value;
    return this;
}

CLI11_INLINE App *App::ignore_underscore(bool value) {
    if(value && !ignore_underscore_) {
        ignore_underscore_ = true;
        auto *p = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
        const auto &match = _compare_subcommand_names(*this, *p);
        if(!match.empty()) {
            ignore_underscore_ = false;
            throw OptionAlreadyAdded("ignore underscore would cause subcommand name conflicts: " + match);
        }
    }
    ignore_underscore_ = value;
    return this;
}

CLI11_INLINE Option *App::add_option(std::string option_name,
                                     callback_t option_callback,
                                     std::string option_description,
                                     bool defaulted,
                                     std::function<std::string()> func) {
    Option myopt{option_name, option_description, option_callback, this, allow_non_standard_options_};

    if(std::find_if(std::begin(options_), std::end(options_), [&myopt](const Option_p &v) { return *v == myopt; }) ==
       std::end(options_)) {
        if(myopt.lnames_.empty() && myopt.snames_.empty()) {
            // if the option is positional only there is additional potential for ambiguities in config files and needs
            // to be checked
            std::string test_name = "--" + myopt.get_single_name();
            if(test_name.size() == 3) {
                test_name.erase(0, 1);
            }

            auto *op = get_option_no_throw(test_name);
            if(op != nullptr && op->get_configurable()) {
                throw(OptionAlreadyAdded("added option positional name matches existing option: " + test_name));
            }
        } else if(parent_ != nullptr) {
            for(auto &ln : myopt.lnames_) {
                auto *op = parent_->get_option_no_throw(ln);
                if(op != nullptr && op->get_configurable()) {
                    throw(OptionAlreadyAdded("added option matches existing positional option: " + ln));
                }
            }
            for(auto &sn : myopt.snames_) {
                auto *op = parent_->get_option_no_throw(sn);
                if(op != nullptr && op->get_configurable()) {
                    throw(OptionAlreadyAdded("added option matches existing positional option: " + sn));
                }
            }
        }
        if(allow_non_standard_options_ && !myopt.snames_.empty()) {
            for(auto &sname : myopt.snames_) {
                if(sname.length() > 1) {
                    std::string test_name;
                    test_name.push_back('-');
                    test_name.push_back(sname.front());
                    auto *op = get_option_no_throw(test_name);
                    if(op != nullptr) {
                        throw(OptionAlreadyAdded("added option interferes with existing short option: " + sname));
                    }
                }
            }
            for(auto &opt : options_) {
                for(const auto &osn : opt->snames_) {
                    if(osn.size() > 1) {
                        std::string test_name;
                        test_name.push_back(osn.front());
                        if(myopt.check_sname(test_name)) {
                            throw(OptionAlreadyAdded("added option interferes with existing non standard option: " +
                                                     osn));
                        }
                    }
                }
            }
        }
        options_.emplace_back();
        Option_p &option = options_.back();
        option.reset(new Option(option_name, option_description, option_callback, this, allow_non_standard_options_));

        // Set the default string capture function
        option->default_function(func);

        // For compatibility with CLI11 1.7 and before, capture the default string here
        if(defaulted)
            option->capture_default_str();

        // Transfer defaults to the new option
        option_defaults_.copy_to(option.get());

        // Don't bother to capture if we already did
        if(!defaulted && option->get_always_capture_default())
            option->capture_default_str();

        return option.get();
    }
    // we know something matches now find what it is so we can produce more error information
    for(auto &opt : options_) {
        const auto &matchname = opt->matching_name(myopt);
        if(!matchname.empty()) {
            throw(OptionAlreadyAdded("added option matched existing option name: " + matchname));
        }
    }
    // this line should not be reached the above loop should trigger the throw
    throw(OptionAlreadyAdded("added option matched existing option name"));  // LCOV_EXCL_LINE
}

CLI11_INLINE Option *App::set_help_flag(std::string flag_name, const std::string &help_description) {
    // take flag_description by const reference otherwise add_flag tries to assign to help_description
    if(help_ptr_ != nullptr) {
        remove_option(help_ptr_);
        help_ptr_ = nullptr;
    }

    // Empty name will simply remove the help flag
    if(!flag_name.empty()) {
        help_ptr_ = add_flag(flag_name, help_description);
        help_ptr_->configurable(false);
    }

    return help_ptr_;
}

CLI11_INLINE Option *App::set_help_all_flag(std::string help_name, const std::string &help_description) {
    // take flag_description by const reference otherwise add_flag tries to assign to flag_description
    if(help_all_ptr_ != nullptr) {
        remove_option(help_all_ptr_);
        help_all_ptr_ = nullptr;
    }

    // Empty name will simply remove the help all flag
    if(!help_name.empty()) {
        help_all_ptr_ = add_flag(help_name, help_description);
        help_all_ptr_->configurable(false);
    }

    return help_all_ptr_;
}

CLI11_INLINE Option *
App::set_version_flag(std::string flag_name, const std::string &versionString, const std::string &version_help) {
    // take flag_description by const reference otherwise add_flag tries to assign to version_description
    if(version_ptr_ != nullptr) {
        remove_option(version_ptr_);
        version_ptr_ = nullptr;
    }

    // Empty name will simply remove the version flag
    if(!flag_name.empty()) {
        version_ptr_ = add_flag_callback(
            flag_name, [versionString]() { throw(CLI::CallForVersion(versionString, 0)); }, version_help);
        version_ptr_->configurable(false);
    }

    return version_ptr_;
}

CLI11_INLINE Option *
App::set_version_flag(std::string flag_name, std::function<std::string()> vfunc, const std::string &version_help) {
    if(version_ptr_ != nullptr) {
        remove_option(version_ptr_);
        version_ptr_ = nullptr;
    }

    // Empty name will simply remove the version flag
    if(!flag_name.empty()) {
        version_ptr_ =
            add_flag_callback(flag_name, [vfunc]() { throw(CLI::CallForVersion(vfunc(), 0)); }, version_help);
        version_ptr_->configurable(false);
    }

    return version_ptr_;
}

CLI11_INLINE Option *App::_add_flag_internal(std::string flag_name, CLI::callback_t fun, std::string flag_description) {
    Option *opt = nullptr;
    if(detail::has_default_flag_values(flag_name)) {
        // check for default values and if it has them
        auto flag_defaults = detail::get_default_flag_values(flag_name);
        detail::remove_default_flag_values(flag_name);
        opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
        for(const auto &fname : flag_defaults)
            opt->fnames_.push_back(fname.first);
        opt->default_flag_values_ = std::move(flag_defaults);
    } else {
        opt = add_option(std::move(flag_name), std::move(fun), std::move(flag_description), false);
    }
    // flags cannot have positional values
    if(opt->get_positional()) {
        auto pos_name = opt->get_name(true);
        remove_option(opt);
        throw IncorrectConstruction::PositionalFlag(pos_name);
    }
    opt->multi_option_policy(MultiOptionPolicy::TakeLast);
    opt->expected(0);
    opt->required(false);
    return opt;
}

CLI11_INLINE Option *App::add_flag_callback(std::string flag_name,
                                            std::function<void(void)> function,  ///< A function to call, void(void)
                                            std::string flag_description) {

    CLI::callback_t fun = [function](const CLI::results_t &res) {
        using CLI::detail::lexical_cast;
        bool trigger{false};
        auto result = lexical_cast(res[0], trigger);
        if(result && trigger) {
            function();
        }
        return result;
    };
    return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description));
}

CLI11_INLINE Option *
App::add_flag_function(std::string flag_name,
                       std::function<void(std::int64_t)> function,  ///< A function to call, void(int)
                       std::string flag_description) {

    CLI::callback_t fun = [function](const CLI::results_t &res) {
        using CLI::detail::lexical_cast;
        std::int64_t flag_count{0};
        lexical_cast(res[0], flag_count);
        function(flag_count);
        return true;
    };
    return _add_flag_internal(flag_name, std::move(fun), std::move(flag_description))
        ->multi_option_policy(MultiOptionPolicy::Sum);
}

CLI11_INLINE Option *App::set_config(std::string option_name,
                                     std::string default_filename,
                                     const std::string &help_message,
                                     bool config_required) {

    // Remove existing config if present
    if(config_ptr_ != nullptr) {
        remove_option(config_ptr_);
        config_ptr_ = nullptr;  // need to remove the config_ptr completely
    }

    // Only add config if option passed
    if(!option_name.empty()) {
        config_ptr_ = add_option(option_name, help_message);
        if(config_required) {
            config_ptr_->required();
        }
        if(!default_filename.empty()) {
            config_ptr_->default_str(std::move(default_filename));
            config_ptr_->force_callback_ = true;
        }
        config_ptr_->configurable(false);
        // set the option to take the last value and reverse given by default
        config_ptr_->multi_option_policy(MultiOptionPolicy::Reverse);
    }

    return config_ptr_;
}

CLI11_INLINE bool App::remove_option(Option *opt) {
    // Make sure no links exist
    for(Option_p &op : options_) {
        op->remove_needs(opt);
        op->remove_excludes(opt);
    }

    if(help_ptr_ == opt)
        help_ptr_ = nullptr;
    if(help_all_ptr_ == opt)
        help_all_ptr_ = nullptr;

    auto iterator =
        std::find_if(std::begin(options_), std::end(options_), [opt](const Option_p &v) { return v.get() == opt; });
    if(iterator != std::end(options_)) {
        options_.erase(iterator);
        return true;
    }
    return false;
}

CLI11_INLINE App *App::add_subcommand(std::string subcommand_name, std::string subcommand_description) {
    if(!subcommand_name.empty() && !detail::valid_name_string(subcommand_name)) {
        if(!detail::valid_first_char(subcommand_name[0])) {
            throw IncorrectConstruction(
                "Subcommand name starts with invalid character, '!' and '-' and control characters");
        }
        for(auto c : subcommand_name) {
            if(!detail::valid_later_char(c)) {
                throw IncorrectConstruction(std::string("Subcommand name contains invalid character ('") + c +
                                            "'), all characters are allowed except"
                                            "'=',':','{','}', ' ', and control characters");
            }
        }
    }
    CLI::App_p subcom = std::shared_ptr<App>(new App(std::move(subcommand_description), subcommand_name, this));
    return add_subcommand(std::move(subcom));
}

CLI11_INLINE App *App::add_subcommand(CLI::App_p subcom) {
    if(!subcom)
        throw IncorrectConstruction("passed App is not valid");
    auto *ckapp = (name_.empty() && parent_ != nullptr) ? _get_fallthrough_parent() : this;
    const auto &mstrg = _compare_subcommand_names(*subcom, *ckapp);
    if(!mstrg.empty()) {
        throw(OptionAlreadyAdded("subcommand name or alias matches existing subcommand: " + mstrg));
    }
    subcom->parent_ = this;
    subcommands_.push_back(std::move(subcom));
    return subcommands_.back().get();
}

CLI11_INLINE bool App::remove_subcommand(App *subcom) {
    // Make sure no links exist
    for(App_p &sub : subcommands_) {
        sub->remove_excludes(subcom);
        sub->remove_needs(subcom);
    }

    auto iterator = std::find_if(
        std::begin(subcommands_), std::end(subcommands_), [subcom](const App_p &v) { return v.get() == subcom; });
    if(iterator != std::end(subcommands_)) {
        subcommands_.erase(iterator);
        return true;
    }
    return false;
}

CLI11_INLINE App *App::get_subcommand(const App *subcom) const {
    if(subcom == nullptr)
        throw OptionNotFound("nullptr passed");
    for(const App_p &subcomptr : subcommands_)
        if(subcomptr.get() == subcom)
            return subcomptr.get();
    throw OptionNotFound(subcom->get_name());
}

CLI11_NODISCARD CLI11_INLINE App *App::get_subcommand(std::string subcom) const {
    auto *subc = _find_subcommand(subcom, false, false);
    if(subc == nullptr)
        throw OptionNotFound(subcom);
    return subc;
}

CLI11_NODISCARD CLI11_INLINE App *App::get_subcommand_no_throw(std::string subcom) const noexcept {
    return _find_subcommand(subcom, false, false);
}

CLI11_NODISCARD CLI11_INLINE App *App::get_subcommand(int index) const {
    if(index >= 0) {
        auto uindex = static_cast<unsigned>(index);
        if(uindex < subcommands_.size())
            return subcommands_[uindex].get();
    }
    throw OptionNotFound(std::to_string(index));
}

CLI11_INLINE CLI::App_p App::get_subcommand_ptr(App *subcom) const {
    if(subcom == nullptr)
        throw OptionNotFound("nullptr passed");
    for(const App_p &subcomptr : subcommands_)
        if(subcomptr.get() == subcom)
            return subcomptr;
    throw OptionNotFound(subcom->get_name());
}

CLI11_NODISCARD CLI11_INLINE CLI::App_p App::get_subcommand_ptr(std::string subcom) const {
    for(const App_p &subcomptr : subcommands_)
        if(subcomptr->check_name(subcom))
            return subcomptr;
    throw OptionNotFound(subcom);
}

CLI11_NODISCARD CLI11_INLINE CLI::App_p App::get_subcommand_ptr(int index) const {
    if(index >= 0) {
        auto uindex = static_cast<unsigned>(index);
        if(uindex < subcommands_.size())
            return subcommands_[uindex];
    }
    throw OptionNotFound(std::to_string(index));
}

CLI11_NODISCARD CLI11_INLINE CLI::App *App::get_option_group(std::string group_name) const {
    for(const App_p &app : subcommands_) {
        if(app->name_.empty() && app->group_ == group_name) {
            return app.get();
        }
    }
    throw OptionNotFound(group_name);
}

CLI11_NODISCARD CLI11_INLINE std::size_t App::count_all() const {
    std::size_t cnt{0};
    for(const auto &opt : options_) {
        cnt += opt->count();
    }
    for(const auto &sub : subcommands_) {
        cnt += sub->count_all();
    }
    if(!get_name().empty()) {  // for named subcommands add the number of times the subcommand was called
        cnt += parsed_;
    }
    return cnt;
}

CLI11_INLINE void App::clear() {

    parsed_ = 0;
    pre_parse_called_ = false;

    missing_.clear();
    parsed_subcommands_.clear();
    for(const Option_p &opt : options_) {
        opt->clear();
    }
    for(const App_p &subc : subcommands_) {
        subc->clear();
    }
}

CLI11_INLINE void App::parse(int argc, const char *const *argv) { parse_char_t(argc, argv); }
CLI11_INLINE void App::parse(int argc, const wchar_t *const *argv) { parse_char_t(argc, argv); }

namespace detail {

// Do nothing or perform narrowing
CLI11_INLINE const char *maybe_narrow(const char *str) { return str; }
CLI11_INLINE std::string maybe_narrow(const wchar_t *str) { return narrow(str); }

}  // namespace detail

template <class CharT> CLI11_INLINE void App::parse_char_t(int argc, const CharT *const *argv) {
    // If the name is not set, read from command line
    if(name_.empty() || has_automatic_name_) {
        has_automatic_name_ = true;
        name_ = detail::maybe_narrow(argv[0]);
    }

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc) - 1U);
    for(auto i = static_cast<std::size_t>(argc) - 1U; i > 0U; --i)
        args.emplace_back(detail::maybe_narrow(argv[i]));

    parse(std::move(args));
}

CLI11_INLINE void App::parse(std::string commandline, bool program_name_included) {

    if(program_name_included) {
        auto nstr = detail::split_program_name(commandline);
        if((name_.empty()) || (has_automatic_name_)) {
            has_automatic_name_ = true;
            name_ = nstr.first;
        }
        commandline = std::move(nstr.second);
    } else {
        detail::trim(commandline);
    }
    // the next section of code is to deal with quoted arguments after an '=' or ':' for windows like operations
    if(!commandline.empty()) {
        commandline = detail::find_and_modify(commandline, "=", detail::escape_detect);
        if(allow_windows_style_options_)
            commandline = detail::find_and_modify(commandline, ":", detail::escape_detect);
    }

    auto args = detail::split_up(std::move(commandline));
    // remove all empty strings
    args.erase(std::remove(args.begin(), args.end(), std::string{}), args.end());
    try {
        detail::remove_quotes(args);
    } catch(const std::invalid_argument &arg) {
        throw CLI::ParseError(arg.what(), CLI::ExitCodes::InvalidError);
    }
    std::reverse(args.begin(), args.end());
    parse(std::move(args));
}

CLI11_INLINE void App::parse(std::wstring commandline, bool program_name_included) {
    parse(narrow(commandline), program_name_included);
}

CLI11_INLINE void App::parse(std::vector<std::string> &args) {
    // Clear if parsed
    if(parsed_ > 0)
        clear();

    // parsed_ is incremented in commands/subcommands,
    // but placed here to make sure this is cleared when
    // running parse after an error is thrown, even by _validate or _configure.
    parsed_ = 1;
    _validate();
    _configure();
    // set the parent as nullptr as this object should be the top now
    parent_ = nullptr;
    parsed_ = 0;

    _parse(args);
    run_callback();
}

CLI11_INLINE void App::parse(std::vector<std::string> &&args) {
    // Clear if parsed
    if(parsed_ > 0)
        clear();

    // parsed_ is incremented in commands/subcommands,
    // but placed here to make sure this is cleared when
    // running parse after an error is thrown, even by _validate or _configure.
    parsed_ = 1;
    _validate();
    _configure();
    // set the parent as nullptr as this object should be the top now
    parent_ = nullptr;
    parsed_ = 0;

    _parse(std::move(args));
    run_callback();
}

CLI11_INLINE void App::parse_from_stream(std::istream &input) {
    if(parsed_ == 0) {
        _validate();
        _configure();
        // set the parent as nullptr as this object should be the top now
    }

    _parse_stream(input);
    run_callback();
}

CLI11_INLINE int App::exit(const Error &e, std::ostream &out, std::ostream &err) const {

    /// Avoid printing anything if this is a CLI::RuntimeError
    if(e.get_name() == "RuntimeError")
        return e.get_exit_code();

    if(e.get_name() == "CallForHelp") {
        out << help();
        return e.get_exit_code();
    }

    if(e.get_name() == "CallForAllHelp") {
        out << help("", AppFormatMode::All);
        return e.get_exit_code();
    }

    if(e.get_name() == "CallForVersion") {
        out << e.what() << '\n';
        return e.get_exit_code();
    }

    if(e.get_exit_code() != static_cast<int>(ExitCodes::Success)) {
        if(failure_message_)
            err << failure_message_(this, e) << std::flush;
    }

    return e.get_exit_code();
}

CLI11_INLINE std::vector<const App *> App::get_subcommands(const std::function<bool(const App *)> &filter) const {
    std::vector<const App *> subcomms(subcommands_.size());
    std::transform(
        std::begin(subcommands_), std::end(subcommands_), std::begin(subcomms), [](const App_p &v) { return v.get(); });

    if(filter) {
        subcomms.erase(std::remove_if(std::begin(subcomms),
                                      std::end(subcomms),
                                      [&filter](const App *app) { return !filter(app); }),
                       std::end(subcomms));
    }

    return subcomms;
}

CLI11_INLINE std::vector<App *> App::get_subcommands(const std::function<bool(App *)> &filter) {
    std::vector<App *> subcomms(subcommands_.size());
    std::transform(
        std::begin(subcommands_), std::end(subcommands_), std::begin(subcomms), [](const App_p &v) { return v.get(); });

    if(filter) {
        subcomms.erase(
            std::remove_if(std::begin(subcomms), std::end(subcomms), [&filter](App *app) { return !filter(app); }),
            std::end(subcomms));
    }

    return subcomms;
}

CLI11_INLINE bool App::remove_excludes(Option *opt) {
    auto iterator = std::find(std::begin(exclude_options_), std::end(exclude_options_), opt);
    if(iterator == std::end(exclude_options_)) {
        return false;
    }
    exclude_options_.erase(iterator);
    return true;
}

CLI11_INLINE bool App::remove_excludes(App *app) {
    auto iterator = std::find(std::begin(exclude_subcommands_), std::end(exclude_subcommands_), app);
    if(iterator == std::end(exclude_subcommands_)) {
        return false;
    }
    auto *other_app = *iterator;
    exclude_subcommands_.erase(iterator);
    other_app->remove_excludes(this);
    return true;
}

CLI11_INLINE bool App::remove_needs(Option *opt) {
    auto iterator = std::find(std::begin(need_options_), std::end(need_options_), opt);
    if(iterator == std::end(need_options_)) {
        return false;
    }
    need_options_.erase(iterator);
    return true;
}

CLI11_INLINE bool App::remove_needs(App *app) {
    auto iterator = std::find(std::begin(need_subcommands_), std::end(need_subcommands_), app);
    if(iterator == std::end(need_subcommands_)) {
        return false;
    }
    need_subcommands_.erase(iterator);
    return true;
}

CLI11_NODISCARD CLI11_INLINE std::string App::help(std::string prev, AppFormatMode mode) const {
    if(prev.empty())
        prev = get_name();
    else
        prev += " " + get_name();

    // Delegate to subcommand if needed
    auto selected_subcommands = get_subcommands();
    if(!selected_subcommands.empty()) {
        return selected_subcommands.back()->help(prev, mode);
    }
    return formatter_->make_help(this, prev, mode);
}

CLI11_NODISCARD CLI11_INLINE std::string App::version() const {
    std::string val;
    if(version_ptr_ != nullptr) {
        // copy the results for reuse later
        results_t rv = version_ptr_->results();
        version_ptr_->clear();
        version_ptr_->add_result("true");
        try {
            version_ptr_->run_callback();
        } catch(const CLI::CallForVersion &cfv) {
            val = cfv.what();
        }
        version_ptr_->clear();
        version_ptr_->add_result(rv);
    }
    return val;
}

CLI11_INLINE std::vector<const Option *> App::get_options(const std::function<bool(const Option *)> filter) const {
    std::vector<const Option *> options(options_.size());
    std::transform(
        std::begin(options_), std::end(options_), std::begin(options), [](const Option_p &val) { return val.get(); });

    if(filter) {
        options.erase(std::remove_if(std::begin(options),
                                     std::end(options),
                                     [&filter](const Option *opt) { return !filter(opt); }),
                      std::end(options));
    }
    for(const auto &subcp : subcommands_) {
        // also check down into nameless subcommands
        const App *subc = subcp.get();
        if(subc->get_name().empty() && !subc->get_group().empty() && subc->get_group().front() == '+') {
            std::vector<const Option *> subcopts = subc->get_options(filter);
            options.insert(options.end(), subcopts.begin(), subcopts.end());
        }
    }
    return options;
}

CLI11_INLINE std::vector<Option *> App::get_options(const std::function<bool(Option *)> filter) {
    std::vector<Option *> options(options_.size());
    std::transform(
        std::begin(options_), std::end(options_), std::begin(options), [](const Option_p &val) { return val.get(); });

    if(filter) {
        options.erase(
            std::remove_if(std::begin(options), std::end(options), [&filter](Option *opt) { return !filter(opt); }),
            std::end(options));
    }
    for(auto &subc : subcommands_) {
        // also check down into nameless subcommands
        if(subc->get_name().empty() && !subc->get_group().empty() && subc->get_group().front() == '+') {
            auto subcopts = subc->get_options(filter);
            options.insert(options.end(), subcopts.begin(), subcopts.end());
        }
    }
    return options;
}

CLI11_NODISCARD CLI11_INLINE Option *App::get_option_no_throw(std::string option_name) noexcept {
    for(Option_p &opt : options_) {
        if(opt->check_name(option_name)) {
            return opt.get();
        }
    }
    for(auto &subc : subcommands_) {
        // also check down into nameless subcommands
        if(subc->get_name().empty()) {
            auto *opt = subc->get_option_no_throw(option_name);
            if(opt != nullptr) {
                return opt;
            }
        }
    }
    return nullptr;
}

CLI11_NODISCARD CLI11_INLINE const Option *App::get_option_no_throw(std::string option_name) const noexcept {
    for(const Option_p &opt : options_) {
        if(opt->check_name(option_name)) {
            return opt.get();
        }
    }
    for(const auto &subc : subcommands_) {
        // also check down into nameless subcommands
        if(subc->get_name().empty()) {
            auto *opt = subc->get_option_no_throw(option_name);
            if(opt != nullptr) {
                return opt;
            }
        }
    }
    return nullptr;
}

CLI11_NODISCARD CLI11_INLINE std::string App::get_display_name(bool with_aliases) const {
    if(name_.empty()) {
        return std::string("[Option Group: ") + get_group() + "]";
    }
    if(aliases_.empty() || !with_aliases) {
        return name_;
    }
    std::string dispname = name_;
    for(const auto &lalias : aliases_) {
        dispname.push_back(',');
        dispname.push_back(' ');
        dispname.append(lalias);
    }
    return dispname;
}

CLI11_NODISCARD CLI11_INLINE bool App::check_name(std::string name_to_check) const {
    std::string local_name = name_;
    if(ignore_underscore_) {
        local_name = detail::remove_underscore(name_);
        name_to_check = detail::remove_underscore(name_to_check);
    }
    if(ignore_case_) {
        local_name = detail::to_lower(name_);
        name_to_check = detail::to_lower(name_to_check);
    }

    if(local_name == name_to_check) {
        return true;
    }
    for(std::string les : aliases_) {  // NOLINT(performance-for-range-copy)
        if(ignore_underscore_) {
            les = detail::remove_underscore(les);
        }
        if(ignore_case_) {
            les = detail::to_lower(les);
        }
        if(les == name_to_check) {
            return true;
        }
    }
    return false;
}

CLI11_NODISCARD CLI11_INLINE std::vector<std::string> App::get_groups() const {
    std::vector<std::string> groups;

    for(const Option_p &opt : options_) {
        // Add group if it is not already in there
        if(std::find(groups.begin(), groups.end(), opt->get_group()) == groups.end()) {
            groups.push_back(opt->get_group());
        }
    }

    return groups;
}

CLI11_NODISCARD CLI11_INLINE std::vector<std::string> App::remaining(bool recurse) const {
    std::vector<std::string> miss_list;
    for(const std::pair<detail::Classifier, std::string> &miss : missing_) {
        miss_list.push_back(std::get<1>(miss));
    }
    // Get from a subcommand that may allow extras
    if(recurse) {
        if(!allow_extras_) {
            for(const auto &sub : subcommands_) {
                if(sub->name_.empty() && !sub->missing_.empty()) {
                    for(const std::pair<detail::Classifier, std::string> &miss : sub->missing_) {
                        miss_list.push_back(std::get<1>(miss));
                    }
                }
            }
        }
        // Recurse into subcommands

        for(const App *sub : parsed_subcommands_) {
            std::vector<std::string> output = sub->remaining(recurse);
            std::copy(std::begin(output), std::end(output), std::back_inserter(miss_list));
        }
    }
    return miss_list;
}

CLI11_NODISCARD CLI11_INLINE std::vector<std::string> App::remaining_for_passthrough(bool recurse) const {
    std::vector<std::string> miss_list = remaining(recurse);
    std::reverse(std::begin(miss_list), std::end(miss_list));
    return miss_list;
}

CLI11_NODISCARD CLI11_INLINE std::size_t App::remaining_size(bool recurse) const {
    auto remaining_options = static_cast<std::size_t>(std::count_if(
        std::begin(missing_), std::end(missing_), [](const std::pair<detail::Classifier, std::string> &val) {
            return val.first != detail::Classifier::POSITIONAL_MARK;
        }));

    if(recurse) {
        for(const App_p &sub : subcommands_) {
            remaining_options += sub->remaining_size(recurse);
        }
    }
    return remaining_options;
}

CLI11_INLINE void App::_validate() const {
    // count the number of positional only args
    auto pcount = std::count_if(std::begin(options_), std::end(options_), [](const Option_p &opt) {
        return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional();
    });
    if(pcount > 1) {
        auto pcount_req = std::count_if(std::begin(options_), std::end(options_), [](const Option_p &opt) {
            return opt->get_items_expected_max() >= detail::expected_max_vector_size && !opt->nonpositional() &&
                   opt->get_required();
        });
        if(pcount - pcount_req > 1) {
            throw InvalidError(name_);
        }
    }

    std::size_t nameless_subs{0};
    for(const App_p &app : subcommands_) {
        app->_validate();
        if(app->get_name().empty())
            ++nameless_subs;
    }

    if(require_option_min_ > 0) {
        if(require_option_max_ > 0) {
            if(require_option_max_ < require_option_min_) {
                throw(InvalidError("Required min options greater than required max options", ExitCodes::InvalidError));
            }
        }
        if(require_option_min_ > (options_.size() + nameless_subs)) {
            throw(
                InvalidError("Required min options greater than number of available options", ExitCodes::InvalidError));
        }
    }
}

CLI11_INLINE void App::_configure() {
    if(default_startup == startup_mode::enabled) {
        disabled_ = false;
    } else if(default_startup == startup_mode::disabled) {
        disabled_ = true;
    }
    for(const App_p &app : subcommands_) {
        if(app->has_automatic_name_) {
            app->name_.clear();
        }
        if(app->name_.empty()) {
            app->fallthrough_ = false;  // make sure fallthrough_ is false to prevent infinite loop
            app->prefix_command_ = false;
        }
        // make sure the parent is set to be this object in preparation for parse
        app->parent_ = this;
        app->_configure();
    }
}

CLI11_INLINE void App::run_callback(bool final_mode, bool suppress_final_callback) {
    pre_callback();
    // in the main app if immediate_callback_ is set it runs the main callback before the used subcommands
    if(!final_mode && parse_complete_callback_) {
        parse_complete_callback_();
    }
    // run the callbacks for the received subcommands
    for(App *subc : get_subcommands()) {
        if(subc->parent_ == this) {
            subc->run_callback(true, suppress_final_callback);
        }
    }
    // now run callbacks for option_groups
    for(auto &subc : subcommands_) {
        if(subc->name_.empty() && subc->count_all() > 0) {
            subc->run_callback(true, suppress_final_callback);
        }
    }

    // finally run the main callback
    if(final_callback_ && (parsed_ > 0) && (!suppress_final_callback)) {
        if(!name_.empty() || count_all() > 0 || parent_ == nullptr) {
            final_callback_();
        }
    }
}

CLI11_NODISCARD CLI11_INLINE bool App::_valid_subcommand(const std::string &current, bool ignore_used) const {
    // Don't match if max has been reached - but still check parents
    if(require_subcommand_max_ != 0 && parsed_subcommands_.size() >= require_subcommand_max_ &&
       subcommand_fallthrough_) {
        return parent_ != nullptr && parent_->_valid_subcommand(current, ignore_used);
    }
    auto *com = _find_subcommand(current, true, ignore_used);
    if(com != nullptr) {
        return true;
    }
    // Check parent if exists, else return false
    if(subcommand_fallthrough_) {
        return parent_ != nullptr && parent_->_valid_subcommand(current, ignore_used);
    }
    return false;
}

CLI11_NODISCARD CLI11_INLINE detail::Classifier App::_recognize(const std::string &current,
                                                                bool ignore_used_subcommands) const {
    std::string dummy1, dummy2;

    if(current == "--")
        return detail::Classifier::POSITIONAL_MARK;
    if(_valid_subcommand(current, ignore_used_subcommands))
        return detail::Classifier::SUBCOMMAND;
    if(detail::split_long(current, dummy1, dummy2))
        return detail::Classifier::LONG;
    if(detail::split_short(current, dummy1, dummy2)) {
        if(dummy1[0] >= '0' && dummy1[0] <= '9') {
            if(get_option_no_throw(std::string{'-', dummy1[0]}) == nullptr) {
                return detail::Classifier::NONE;
            }
        }
        return detail::Classifier::SHORT;
    }
    if((allow_windows_style_options_) && (detail::split_windows_style(current, dummy1, dummy2)))
        return detail::Classifier::WINDOWS_STYLE;
    if((current == "++") && !name_.empty() && parent_ != nullptr)
        return detail::Classifier::SUBCOMMAND_TERMINATOR;
    auto dotloc = current.find_first_of('.');
    if(dotloc != std::string::npos) {
        auto *cm = _find_subcommand(current.substr(0, dotloc), true, ignore_used_subcommands);
        if(cm != nullptr) {
            auto res = cm->_recognize(current.substr(dotloc + 1), ignore_used_subcommands);
            if(res == detail::Classifier::SUBCOMMAND) {
                return res;
            }
        }
    }
    return detail::Classifier::NONE;
}

CLI11_INLINE bool App::_process_config_file(const std::string &config_file, bool throw_error) {
    auto path_result = detail::check_path(config_file.c_str());
    if(path_result == detail::path_type::file) {
        try {
            std::vector<ConfigItem> values = config_formatter_->from_file(config_file);
            _parse_config(values);
            return true;
        } catch(const FileError &) {
            if(throw_error) {
                throw;
            }
            return false;
        }
    } else if(throw_error) {
        throw FileError::Missing(config_file);
    } else {
        return false;
    }
}

CLI11_INLINE void App::_process_config_file() {
    if(config_ptr_ != nullptr) {
        bool config_required = config_ptr_->get_required();
        auto file_given = config_ptr_->count() > 0;
        if(!(file_given || config_ptr_->envname_.empty())) {
            std::string ename_string = detail::get_environment_value(config_ptr_->envname_);
            if(!ename_string.empty()) {
                config_ptr_->add_result(ename_string);
            }
        }
        config_ptr_->run_callback();

        auto config_files = config_ptr_->as<std::vector<std::string>>();
        bool files_used{file_given};
        if(config_files.empty() || config_files.front().empty()) {
            if(config_required) {
                throw FileError("config file is required but none was given");
            }
            return;
        }
        for(const auto &config_file : config_files) {
            if(_process_config_file(config_file, config_required || file_given)) {
                files_used = true;
            }
        }
        if(!files_used) {
            // this is done so the count shows as 0 if no callbacks were processed
            config_ptr_->clear();
            bool force = config_ptr_->force_callback_;
            config_ptr_->force_callback_ = false;
            config_ptr_->run_callback();
            config_ptr_->force_callback_ = force;
        }
    }
}

CLI11_INLINE void App::_process_env() {
    for(const Option_p &opt : options_) {
        if(opt->count() == 0 && !opt->envname_.empty()) {
            std::string ename_string = detail::get_environment_value(opt->envname_);
            if(!ename_string.empty()) {
                std::string result = ename_string;
                result = opt->_validate(result, 0);
                if(result.empty()) {
                    opt->add_result(ename_string);
                }
            }
        }
    }

    for(App_p &sub : subcommands_) {
        if(sub->get_name().empty() || (sub->count_all() > 0 && !sub->parse_complete_callback_)) {
            // only process environment variables if the callback has actually been triggered already
            sub->_process_env();
        }
    }
}

CLI11_INLINE void App::_process_callbacks() {

    for(App_p &sub : subcommands_) {
        // process the priority option_groups first
        if(sub->get_name().empty() && sub->parse_complete_callback_) {
            if(sub->count_all() > 0) {
                sub->_process_callbacks();
                sub->run_callback();
            }
        }
    }

    for(const Option_p &opt : options_) {
        if((*opt) && !opt->get_callback_run()) {
            opt->run_callback();
        }
    }
    for(App_p &sub : subcommands_) {
        if(!sub->parse_complete_callback_) {
            sub->_process_callbacks();
        }
    }
}

CLI11_INLINE void App::_process_help_flags(bool trigger_help, bool trigger_all_help) const {
    const Option *help_ptr = get_help_ptr();
    const Option *help_all_ptr = get_help_all_ptr();

    if(help_ptr != nullptr && help_ptr->count() > 0)
        trigger_help = true;
    if(help_all_ptr != nullptr && help_all_ptr->count() > 0)
        trigger_all_help = true;

    // If there were parsed subcommands, call those. First subcommand wins if there are multiple ones.
    if(!parsed_subcommands_.empty()) {
        for(const App *sub : parsed_subcommands_)
            sub->_process_help_flags(trigger_help, trigger_all_help);

        // Only the final subcommand should call for help. All help wins over help.
    } else if(trigger_all_help) {
        throw CallForAllHelp();
    } else if(trigger_help) {
        throw CallForHelp();
    }
}

CLI11_INLINE void App::_process_requirements() {
    // check excludes
    bool excluded{false};
    std::string excluder;
    for(const auto &opt : exclude_options_) {
        if(opt->count() > 0) {
            excluded = true;
            excluder = opt->get_name();
        }
    }
    for(const auto &subc : exclude_subcommands_) {
        if(subc->count_all() > 0) {
            excluded = true;
            excluder = subc->get_display_name();
        }
    }
    if(excluded) {
        if(count_all() > 0) {
            throw ExcludesError(get_display_name(), excluder);
        }
        // if we are excluded but didn't receive anything, just return
        return;
    }

    // check excludes
    bool missing_needed{false};
    std::string missing_need;
    for(const auto &opt : need_options_) {
        if(opt->count() == 0) {
            missing_needed = true;
            missing_need = opt->get_name();
        }
    }
    for(const auto &subc : need_subcommands_) {
        if(subc->count_all() == 0) {
            missing_needed = true;
            missing_need = subc->get_display_name();
        }
    }
    if(missing_needed) {
        if(count_all() > 0) {
            throw RequiresError(get_display_name(), missing_need);
        }
        // if we missing something but didn't have any options, just return
        return;
    }

    std::size_t used_options = 0;
    for(const Option_p &opt : options_) {

        if(opt->count() != 0) {
            ++used_options;
        }
        // Required but empty
        if(opt->get_required() && opt->count() == 0) {
            throw RequiredError(opt->get_name());
        }
        // Requires
        for(const Option *opt_req : opt->needs_)
            if(opt->count() > 0 && opt_req->count() == 0)
                throw RequiresError(opt->get_name(), opt_req->get_name());
        // Excludes
        for(const Option *opt_ex : opt->excludes_)
            if(opt->count() > 0 && opt_ex->count() != 0)
                throw ExcludesError(opt->get_name(), opt_ex->get_name());
    }
    // check for the required number of subcommands
    if(require_subcommand_min_ > 0) {
        auto selected_subcommands = get_subcommands();
        if(require_subcommand_min_ > selected_subcommands.size())
            throw RequiredError::Subcommand(require_subcommand_min_);
    }

    // Max error cannot occur, the extra subcommand will parse as an ExtrasError or a remaining item.

    // run this loop to check how many unnamed subcommands were actually used since they are considered options
    // from the perspective of an App
    for(App_p &sub : subcommands_) {
        if(sub->disabled_)
            continue;
        if(sub->name_.empty() && sub->count_all() > 0) {
            ++used_options;
        }
    }

    if(require_option_min_ > used_options || (require_option_max_ > 0 && require_option_max_ < used_options)) {
        auto option_list = detail::join(options_, [this](const Option_p &ptr) {
            if(ptr.get() == help_ptr_ || ptr.get() == help_all_ptr_) {
                return std::string{};
            }
            return ptr->get_name(false, true);
        });

        auto subc_list = get_subcommands([](App *app) { return ((app->get_name().empty()) && (!app->disabled_)); });
        if(!subc_list.empty()) {
            option_list += "," + detail::join(subc_list, [](const App *app) { return app->get_display_name(); });
        }
        throw RequiredError::Option(require_option_min_, require_option_max_, used_options, option_list);
    }

    // now process the requirements for subcommands if needed
    for(App_p &sub : subcommands_) {
        if(sub->disabled_)
            continue;
        if(sub->name_.empty() && sub->required_ == false) {
            if(sub->count_all() == 0) {
                if(require_option_min_ > 0 && require_option_min_ <= used_options) {
                    continue;
                    // if we have met the requirement and there is nothing in this option group skip checking
                    // requirements
                }
                if(require_option_max_ > 0 && used_options >= require_option_min_) {
                    continue;
                    // if we have met the requirement and there is nothing in this option group skip checking
                    // requirements
                }
            }
        }
        if(sub->count() > 0 || sub->name_.empty()) {
            sub->_process_requirements();
        }

        if(sub->required_ && sub->count_all() == 0) {
            throw(CLI::RequiredError(sub->get_display_name()));
        }
    }
}

CLI11_INLINE void App::_process() {
    // help takes precedence over other potential errors and config and environment shouldn't be processed if help
    // throws
    _process_help_flags();
    try {
        // the config file might generate a FileError but that should not be processed until later in the process
        // to allow for help, version and other errors to generate first.
        _process_config_file();

        // process env shouldn't throw but no reason to process it if config generated an error
        _process_env();
    } catch(const CLI::FileError &) {
        // callbacks can generate exceptions which should take priority
        // over the config file error if one exists.
        _process_callbacks();
        throw;
    }

    _process_callbacks();

    _process_requirements();
}

CLI11_INLINE void App::_process_extras() {
    if(!(allow_extras_ || prefix_command_)) {
        std::size_t num_left_over = remaining_size();
        if(num_left_over > 0) {
            throw ExtrasError(name_, remaining(false));
        }
    }

    for(App_p &sub : subcommands_) {
        if(sub->count() > 0)
            sub->_process_extras();
    }
}

CLI11_INLINE void App::_process_extras(std::vector<std::string> &args) {
    if(!(allow_extras_ || prefix_command_)) {
        std::size_t num_left_over = remaining_size();
        if(num_left_over > 0) {
            args = remaining(false);
            throw ExtrasError(name_, args);
        }
    }

    for(App_p &sub : subcommands_) {
        if(sub->count() > 0)
            sub->_process_extras(args);
    }
}

CLI11_INLINE void App::increment_parsed() {
    ++parsed_;
    for(App_p &sub : subcommands_) {
        if(sub->get_name().empty())
            sub->increment_parsed();
    }
}

CLI11_INLINE void App::_parse(std::vector<std::string> &args) {
    increment_parsed();
    _trigger_pre_parse(args.size());
    bool positional_only = false;

    while(!args.empty()) {
        if(!_parse_single(args, positional_only)) {
            break;
        }
    }

    if(parent_ == nullptr) {
        _process();

        // Throw error if any items are left over (depending on settings)
        _process_extras(args);

        // Convert missing (pairs) to extras (string only) ready for processing in another app
        args = remaining_for_passthrough(false);
    } else if(parse_complete_callback_) {
        _process_env();
        _process_callbacks();
        _process_help_flags();
        _process_requirements();
        run_callback(false, true);
    }
}

CLI11_INLINE void App::_parse(std::vector<std::string> &&args) {
    // this can only be called by the top level in which case parent == nullptr by definition
    // operation is simplified
    increment_parsed();
    _trigger_pre_parse(args.size());
    bool positional_only = false;

    while(!args.empty()) {
        _parse_single(args, positional_only);
    }
    _process();

    // Throw error if any items are left over (depending on settings)
    _process_extras();
}

CLI11_INLINE void App::_parse_stream(std::istream &input) {
    auto values = config_formatter_->from_config(input);
    _parse_config(values);
    increment_parsed();
    _trigger_pre_parse(values.size());
    _process();

    // Throw error if any items are left over (depending on settings)
    _process_extras();
}

CLI11_INLINE void App::_parse_config(const std::vector<ConfigItem> &args) {
    for(const ConfigItem &item : args) {
        if(!_parse_single_config(item) && allow_config_extras_ == config_extras_mode::error)
            throw ConfigError::Extras(item.fullname());
    }
}

CLI11_INLINE bool App::_parse_single_config(const ConfigItem &item, std::size_t level) {

    if(level < item.parents.size()) {
        auto *subcom = get_subcommand_no_throw(item.parents.at(level));
        return (subcom != nullptr) ? subcom->_parse_single_config(item, level + 1) : false;
    }
    // check for section open
    if(item.name == "++") {
        if(configurable_) {
            increment_parsed();
            _trigger_pre_parse(2);
            if(parent_ != nullptr) {
                parent_->parsed_subcommands_.push_back(this);
            }
        }
        return true;
    }
    // check for section close
    if(item.name == "--") {
        if(configurable_ && parse_complete_callback_) {
            _process_callbacks();
            _process_requirements();
            run_callback();
        }
        return true;
    }
    Option *op = get_option_no_throw("--" + item.name);
    if(op == nullptr) {
        if(item.name.size() == 1) {
            op = get_option_no_throw("-" + item.name);
        }
        if(op == nullptr) {
            op = get_option_no_throw(item.name);
        } else if(!op->get_configurable()) {
            auto *testop = get_option_no_throw(item.name);
            if(testop != nullptr && testop->get_configurable()) {
                op = testop;
            }
        }
    } else if(!op->get_configurable()) {
        if(item.name.size() == 1) {
            auto *testop = get_option_no_throw("-" + item.name);
            if(testop != nullptr && testop->get_configurable()) {
                op = testop;
            }
        }
        if(!op->get_configurable()) {
            auto *testop = get_option_no_throw(item.name);
            if(testop != nullptr && testop->get_configurable()) {
                op = testop;
            }
        }
    }

    if(op == nullptr) {
        // If the option was not present
        if(get_allow_config_extras() == config_extras_mode::capture) {
            // Should we worry about classifying the extras properly?
            missing_.emplace_back(detail::Classifier::NONE, item.fullname());
            for(const auto &input : item.inputs) {
                missing_.emplace_back(detail::Classifier::NONE, input);
            }
        }
        return false;
    }

    if(!op->get_configurable()) {
        if(get_allow_config_extras() == config_extras_mode::ignore_all) {
            return false;
        }
        throw ConfigError::NotConfigurable(item.fullname());
    }
    if(op->empty()) {
        std::vector<std::string> buffer;  // a buffer to use for copying an modifying inputs in a few cases
        bool useBuffer{false};
        if(item.multiline) {
            if(!op->get_inject_separator()) {
                buffer = item.inputs;
                buffer.erase(std::remove(buffer.begin(), buffer.end(), "%%"), buffer.end());
                useBuffer = true;
            }
        }
        const std::vector<std::string> &inputs = (useBuffer) ? buffer : item.inputs;
        if(op->get_expected_min() == 0) {
            if(item.inputs.size() <= 1) {
                // Flag parsing
                auto res = config_formatter_->to_flag(item);
                bool converted{false};
                if(op->get_disable_flag_override()) {
                    auto val = detail::to_flag_value(res);
                    if(val == 1) {
                        res = op->get_flag_value(item.name, "{}");
                        converted = true;
                    }
                }

                if(!converted) {
                    errno = 0;
                    if(res != "{}" || op->get_expected_max() <= 1) {
                        res = op->get_flag_value(item.name, res);
                    }
                }

                op->add_result(res);
                return true;
            }
            if(static_cast<int>(inputs.size()) > op->get_items_expected_max() &&
               op->get_multi_option_policy() != MultiOptionPolicy::TakeAll) {
                if(op->get_items_expected_max() > 1) {
                    throw ArgumentMismatch::AtMost(item.fullname(), op->get_items_expected_max(), inputs.size());
                }

                if(!op->get_disable_flag_override()) {
                    throw ConversionError::TooManyInputsFlag(item.fullname());
                }
                // if the disable flag override is set then we must have the flag values match a known flag value
                // this is true regardless of the output value, so an array input is possible and must be accounted for
                for(const auto &res : inputs) {
                    bool valid_value{false};
                    if(op->default_flag_values_.empty()) {
                        if(res == "true" || res == "false" || res == "1" || res == "0") {
                            valid_value = true;
                        }
                    } else {
                        for(const auto &valid_res : op->default_flag_values_) {
                            if(valid_res.second == res) {
                                valid_value = true;
                                break;
                            }
                        }
                    }

                    if(valid_value) {
                        op->add_result(res);
                    } else {
                        throw InvalidError("invalid flag argument given");
                    }
                }
                return true;
            }
        }
        op->add_result(inputs);
        op->run_callback();
    }

    return true;
}

CLI11_INLINE bool App::_parse_single(std::vector<std::string> &args, bool &positional_only) {
    bool retval = true;
    detail::Classifier classifier = positional_only ? detail::Classifier::NONE : _recognize(args.back());
    switch(classifier) {
    case detail::Classifier::POSITIONAL_MARK:
        args.pop_back();
        positional_only = true;
        if((!_has_remaining_positionals()) && (parent_ != nullptr)) {
            retval = false;
        } else {
            _move_to_missing(classifier, "--");
        }
        break;
    case detail::Classifier::SUBCOMMAND_TERMINATOR:
        // treat this like a positional mark if in the parent app
        args.pop_back();
        retval = false;
        break;
    case detail::Classifier::SUBCOMMAND:
        retval = _parse_subcommand(args);
        break;
    case detail::Classifier::LONG:
    case detail::Classifier::SHORT:
    case detail::Classifier::WINDOWS_STYLE:
        // If already parsed a subcommand, don't accept options_
        retval = _parse_arg(args, classifier, false);
        break;
    case detail::Classifier::NONE:
        // Probably a positional or something for a parent (sub)command
        retval = _parse_positional(args, false);
        if(retval && positionals_at_end_) {
            positional_only = true;
        }
        break;
        // LCOV_EXCL_START
    default:
        throw HorribleError("unrecognized classifier (you should not see this!)");
        // LCOV_EXCL_STOP
    }
    return retval;
}

CLI11_NODISCARD CLI11_INLINE std::size_t App::_count_remaining_positionals(bool required_only) const {
    std::size_t retval = 0;
    for(const Option_p &opt : options_) {
        if(opt->get_positional() && (!required_only || opt->get_required())) {
            if(opt->get_items_expected_min() > 0 && static_cast<int>(opt->count()) < opt->get_items_expected_min()) {
                retval += static_cast<std::size_t>(opt->get_items_expected_min()) - opt->count();
            }
        }
    }
    return retval;
}

CLI11_NODISCARD CLI11_INLINE bool App::_has_remaining_positionals() const {
    for(const Option_p &opt : options_) {
        if(opt->get_positional() && ((static_cast<int>(opt->count()) < opt->get_items_expected_min()))) {
            return true;
        }
    }

    return false;
}

CLI11_INLINE bool App::_parse_positional(std::vector<std::string> &args, bool haltOnSubcommand) {

    const std::string &positional = args.back();
    Option *posOpt{nullptr};

    if(positionals_at_end_) {
        // deal with the case of required arguments at the end which should take precedence over other arguments
        auto arg_rem = args.size();
        auto remreq = _count_remaining_positionals(true);
        if(arg_rem <= remreq) {
            for(const Option_p &opt : options_) {
                if(opt->get_positional() && opt->required_) {
                    if(static_cast<int>(opt->count()) < opt->get_items_expected_min()) {
                        if(validate_positionals_) {
                            std::string pos = positional;
                            pos = opt->_validate(pos, 0);
                            if(!pos.empty()) {
                                continue;
                            }
                        }
                        posOpt = opt.get();
                        break;
                    }
                }
            }
        }
    }
    if(posOpt == nullptr) {
        for(const Option_p &opt : options_) {
            // Eat options, one by one, until done
            if(opt->get_positional() &&
               (static_cast<int>(opt->count()) < opt->get_items_expected_max() || opt->get_allow_extra_args())) {
                if(validate_positionals_) {
                    std::string pos = positional;
                    pos = opt->_validate(pos, 0);
                    if(!pos.empty()) {
                        continue;
                    }
                }
                posOpt = opt.get();
                break;
            }
        }
    }
    if(posOpt != nullptr) {
        parse_order_.push_back(posOpt);
        if(posOpt->get_inject_separator()) {
            if(!posOpt->results().empty() && !posOpt->results().back().empty()) {
                posOpt->add_result(std::string{});
            }
        }
        if(posOpt->get_trigger_on_parse() && posOpt->current_option_state_ == Option::option_state::callback_run) {
            posOpt->clear();
        }
        posOpt->add_result(positional);
        if(posOpt->get_trigger_on_parse()) {
            posOpt->run_callback();
        }

        args.pop_back();
        return true;
    }

    for(auto &subc : subcommands_) {
        if((subc->name_.empty()) && (!subc->disabled_)) {
            if(subc->_parse_positional(args, false)) {
                if(!subc->pre_parse_called_) {
                    subc->_trigger_pre_parse(args.size());
                }
                return true;
            }
        }
    }
    // let the parent deal with it if possible
    if(parent_ != nullptr && fallthrough_) {
        return _get_fallthrough_parent()->_parse_positional(args, static_cast<bool>(parse_complete_callback_));
    }
    /// Try to find a local subcommand that is repeated
    auto *com = _find_subcommand(args.back(), true, false);
    if(com != nullptr && (require_subcommand_max_ == 0 || require_subcommand_max_ > parsed_subcommands_.size())) {
        if(haltOnSubcommand) {
            return false;
        }
        args.pop_back();
        com->_parse(args);
        return true;
    }
    if(subcommand_fallthrough_) {
        /// now try one last gasp at subcommands that have been executed before, go to root app and try to find a
        /// subcommand in a broader way, if one exists let the parent deal with it
        auto *parent_app = (parent_ != nullptr) ? _get_fallthrough_parent() : this;
        com = parent_app->_find_subcommand(args.back(), true, false);
        if(com != nullptr && (com->parent_->require_subcommand_max_ == 0 ||
                              com->parent_->require_subcommand_max_ > com->parent_->parsed_subcommands_.size())) {
            return false;
        }
    }
    if(positionals_at_end_) {
        throw CLI::ExtrasError(name_, args);
    }
    /// If this is an option group don't deal with it
    if(parent_ != nullptr && name_.empty()) {
        return false;
    }
    /// We are out of other options this goes to missing
    _move_to_missing(detail::Classifier::NONE, positional);
    args.pop_back();
    if(prefix_command_) {
        while(!args.empty()) {
            _move_to_missing(detail::Classifier::NONE, args.back());
            args.pop_back();
        }
    }

    return true;
}

CLI11_NODISCARD CLI11_INLINE App *
App::_find_subcommand(const std::string &subc_name, bool ignore_disabled, bool ignore_used) const noexcept {
    for(const App_p &com : subcommands_) {
        if(com->disabled_ && ignore_disabled)
            continue;
        if(com->get_name().empty()) {
            auto *subc = com->_find_subcommand(subc_name, ignore_disabled, ignore_used);
            if(subc != nullptr) {
                return subc;
            }
        }
        if(com->check_name(subc_name)) {
            if((!*com) || !ignore_used)
                return com.get();
        }
    }
    return nullptr;
}

CLI11_INLINE bool App::_parse_subcommand(std::vector<std::string> &args) {
    if(_count_remaining_positionals(/* required */ true) > 0) {
        _parse_positional(args, false);
        return true;
    }
    auto *com = _find_subcommand(args.back(), true, true);
    if(com == nullptr) {
        // the main way to get here is using .notation
        auto dotloc = args.back().find_first_of('.');
        if(dotloc != std::string::npos) {
            com = _find_subcommand(args.back().substr(0, dotloc), true, true);
            if(com != nullptr) {
                args.back() = args.back().substr(dotloc + 1);
                args.push_back(com->get_display_name());
            }
        }
    }
    if(com != nullptr) {
        args.pop_back();
        if(!com->silent_) {
            parsed_subcommands_.push_back(com);
        }
        com->_parse(args);
        auto *parent_app = com->parent_;
        while(parent_app != this) {
            parent_app->_trigger_pre_parse(args.size());
            if(!com->silent_) {
                parent_app->parsed_subcommands_.push_back(com);
            }
            parent_app = parent_app->parent_;
        }
        return true;
    }

    if(parent_ == nullptr)
        throw HorribleError("Subcommand " + args.back() + " missing");
    return false;
}

CLI11_INLINE bool
App::_parse_arg(std::vector<std::string> &args, detail::Classifier current_type, bool local_processing_only) {

    std::string current = args.back();

    std::string arg_name;
    std::string value;
    std::string rest;

    switch(current_type) {
    case detail::Classifier::LONG:
        if(!detail::split_long(current, arg_name, value))
            throw HorribleError("Long parsed but missing (you should not see this):" + args.back());
        break;
    case detail::Classifier::SHORT:
        if(!detail::split_short(current, arg_name, rest))
            throw HorribleError("Short parsed but missing! You should not see this");
        break;
    case detail::Classifier::WINDOWS_STYLE:
        if(!detail::split_windows_style(current, arg_name, value))
            throw HorribleError("windows option parsed but missing! You should not see this");
        break;
    case detail::Classifier::SUBCOMMAND:
    case detail::Classifier::SUBCOMMAND_TERMINATOR:
    case detail::Classifier::POSITIONAL_MARK:
    case detail::Classifier::NONE:
    default:
        throw HorribleError("parsing got called with invalid option! You should not see this");
    }

    auto op_ptr = std::find_if(std::begin(options_), std::end(options_), [arg_name, current_type](const Option_p &opt) {
        if(current_type == detail::Classifier::LONG)
            return opt->check_lname(arg_name);
        if(current_type == detail::Classifier::SHORT)
            return opt->check_sname(arg_name);
        // this will only get called for detail::Classifier::WINDOWS_STYLE
        return opt->check_lname(arg_name) || opt->check_sname(arg_name);
    });

    // Option not found
    while(op_ptr == std::end(options_)) {
        // using while so we can break
        for(auto &subc : subcommands_) {
            if(subc->name_.empty() && !subc->disabled_) {
                if(subc->_parse_arg(args, current_type, local_processing_only)) {
                    if(!subc->pre_parse_called_) {
                        subc->_trigger_pre_parse(args.size());
                    }
                    return true;
                }
            }
        }
        if(allow_non_standard_options_ && current_type == detail::Classifier::SHORT && current.size() > 2) {
            std::string narg_name;
            std::string nvalue;
            detail::split_long(std::string{'-'} + current, narg_name, nvalue);
            op_ptr = std::find_if(std::begin(options_), std::end(options_), [narg_name](const Option_p &opt) {
                return opt->check_sname(narg_name);
            });
            if(op_ptr != std::end(options_)) {
                arg_name = narg_name;
                value = nvalue;
                rest.clear();
                break;
            }
        }

        // don't capture missing if this is a nameless subcommand and nameless subcommands can't fallthrough
        if(parent_ != nullptr && name_.empty()) {
            return false;
        }

        // now check for '.' notation of subcommands
        auto dotloc = arg_name.find_first_of('.', 1);
        if(dotloc != std::string::npos) {
            // using dot notation is equivalent to single argument subcommand
            auto *sub = _find_subcommand(arg_name.substr(0, dotloc), true, false);
            if(sub != nullptr) {
                std::string v = args.back();
                args.pop_back();
                arg_name = arg_name.substr(dotloc + 1);
                if(arg_name.size() > 1) {
                    args.push_back(std::string("--") + v.substr(dotloc + 3));
                    current_type = detail::Classifier::LONG;
                } else {
                    auto nval = v.substr(dotloc + 2);
                    nval.front() = '-';
                    if(nval.size() > 2) {
                        // '=' not allowed in short form arguments
                        args.push_back(nval.substr(3));
                        nval.resize(2);
                    }
                    args.push_back(nval);
                    current_type = detail::Classifier::SHORT;
                }
                auto val = sub->_parse_arg(args, current_type, true);
                if(val) {
                    if(!sub->silent_) {
                        parsed_subcommands_.push_back(sub);
                    }
                    // deal with preparsing
                    increment_parsed();
                    _trigger_pre_parse(args.size());
                    // run the parse complete callback since the subcommand processing is now complete
                    if(sub->parse_complete_callback_) {
                        sub->_process_env();
                        sub->_process_callbacks();
                        sub->_process_help_flags();
                        sub->_process_requirements();
                        sub->run_callback(false, true);
                    }
                    return true;
                }
                args.pop_back();
                args.push_back(v);
            }
        }
        if(local_processing_only) {
            return false;
        }
        // If a subcommand, try the main command
        if(parent_ != nullptr && fallthrough_)
            return _get_fallthrough_parent()->_parse_arg(args, current_type, false);

        // Otherwise, add to missing
        args.pop_back();
        _move_to_missing(current_type, current);
        return true;
    }

    args.pop_back();

    // Get a reference to the pointer to make syntax bearable
    Option_p &op = *op_ptr;
    /// if we require a separator add it here
    if(op->get_inject_separator()) {
        if(!op->results().empty() && !op->results().back().empty()) {
            op->add_result(std::string{});
        }
    }
    if(op->get_trigger_on_parse() && op->current_option_state_ == Option::option_state::callback_run) {
        op->clear();
    }
    int min_num = (std::min)(op->get_type_size_min(), op->get_items_expected_min());
    int max_num = op->get_items_expected_max();
    // check container like options to limit the argument size to a single type if the allow_extra_flags argument is
    // set. 16 is somewhat arbitrary (needs to be at least 4)
    if(max_num >= detail::expected_max_vector_size / 16 && !op->get_allow_extra_args()) {
        auto tmax = op->get_type_size_max();
        max_num = detail::checked_multiply(tmax, op->get_expected_min()) ? tmax : detail::expected_max_vector_size;
    }
    // Make sure we always eat the minimum for unlimited vectors
    int collected = 0;     // total number of arguments collected
    int result_count = 0;  // local variable for number of results in a single arg string
    // deal with purely flag like things
    if(max_num == 0) {
        auto res = op->get_flag_value(arg_name, value);
        op->add_result(res);
        parse_order_.push_back(op.get());
    } else if(!value.empty()) {  // --this=value
        op->add_result(value, result_count);
        parse_order_.push_back(op.get());
        collected += result_count;
        // -Trest
    } else if(!rest.empty()) {
        op->add_result(rest, result_count);
        parse_order_.push_back(op.get());
        rest = "";
        collected += result_count;
    }

    // gather the minimum number of arguments
    while(min_num > collected && !args.empty()) {
        std::string current_ = args.back();
        args.pop_back();
        op->add_result(current_, result_count);
        parse_order_.push_back(op.get());
        collected += result_count;
    }

    if(min_num > collected) {  // if we have run out of arguments and the minimum was not met
        throw ArgumentMismatch::TypedAtLeast(op->get_name(), min_num, op->get_type_name());
    }

    // now check for optional arguments
    if(max_num > collected || op->get_allow_extra_args()) {  // we allow optional arguments
        auto remreqpos = _count_remaining_positionals(true);
        // we have met the minimum now optionally check up to the maximum
        while((collected < max_num || op->get_allow_extra_args()) && !args.empty() &&
              _recognize(args.back(), false) == detail::Classifier::NONE) {
            // If any required positionals remain, don't keep eating
            if(remreqpos >= args.size()) {
                break;
            }
            if(validate_optional_arguments_) {
                std::string arg = args.back();
                arg = op->_validate(arg, 0);
                if(!arg.empty()) {
                    break;
                }
            }
            op->add_result(args.back(), result_count);
            parse_order_.push_back(op.get());
            args.pop_back();
            collected += result_count;
        }

        // Allow -- to end an unlimited list and "eat" it
        if(!args.empty() && _recognize(args.back()) == detail::Classifier::POSITIONAL_MARK)
            args.pop_back();
        // optional flag that didn't receive anything now get the default value
        if(min_num == 0 && max_num > 0 && collected == 0) {
            auto res = op->get_flag_value(arg_name, std::string{});
            op->add_result(res);
            parse_order_.push_back(op.get());
        }
    }
    // if we only partially completed a type then add an empty string if allowed for later processing
    if(min_num > 0 && (collected % op->get_type_size_max()) != 0) {
        if(op->get_type_size_max() != op->get_type_size_min()) {
            op->add_result(std::string{});
        } else {
            throw ArgumentMismatch::PartialType(op->get_name(), op->get_type_size_min(), op->get_type_name());
        }
    }
    if(op->get_trigger_on_parse()) {
        op->run_callback();
    }
    if(!rest.empty()) {
        rest = "-" + rest;
        args.push_back(rest);
    }
    return true;
}

CLI11_INLINE void App::_trigger_pre_parse(std::size_t remaining_args) {
    if(!pre_parse_called_) {
        pre_parse_called_ = true;
        if(pre_parse_callback_) {
            pre_parse_callback_(remaining_args);
        }
    } else if(immediate_callback_) {
        if(!name_.empty()) {
            auto pcnt = parsed_;
            missing_t extras = std::move(missing_);
            clear();
            parsed_ = pcnt;
            pre_parse_called_ = true;
            missing_ = std::move(extras);
        }
    }
}

CLI11_INLINE App *App::_get_fallthrough_parent() {
    if(parent_ == nullptr) {
        throw(HorribleError("No Valid parent"));
    }
    auto *fallthrough_parent = parent_;
    while((fallthrough_parent->parent_ != nullptr) && (fallthrough_parent->get_name().empty())) {
        fallthrough_parent = fallthrough_parent->parent_;
    }
    return fallthrough_parent;
}

CLI11_NODISCARD CLI11_INLINE const std::string &App::_compare_subcommand_names(const App &subcom,
                                                                               const App &base) const {
    static const std::string estring;
    if(subcom.disabled_) {
        return estring;
    }
    for(const auto &subc : base.subcommands_) {
        if(subc.get() != &subcom) {
            if(subc->disabled_) {
                continue;
            }
            if(!subcom.get_name().empty()) {
                if(subc->check_name(subcom.get_name())) {
                    return subcom.get_name();
                }
            }
            if(!subc->get_name().empty()) {
                if(subcom.check_name(subc->get_name())) {
                    return subc->get_name();
                }
            }
            for(const auto &les : subcom.aliases_) {
                if(subc->check_name(les)) {
                    return les;
                }
            }
            // this loop is needed in case of ignore_underscore or ignore_case on one but not the other
            for(const auto &les : subc->aliases_) {
                if(subcom.check_name(les)) {
                    return les;
                }
            }
            // if the subcommand is an option group we need to check deeper
            if(subc->get_name().empty()) {
                const auto &cmpres = _compare_subcommand_names(subcom, *subc);
                if(!cmpres.empty()) {
                    return cmpres;
                }
            }
            // if the test subcommand is an option group we need to check deeper
            if(subcom.get_name().empty()) {
                const auto &cmpres = _compare_subcommand_names(*subc, subcom);
                if(!cmpres.empty()) {
                    return cmpres;
                }
            }
        }
    }
    return estring;
}

CLI11_INLINE void App::_move_to_missing(detail::Classifier val_type, const std::string &val) {
    if(allow_extras_ || subcommands_.empty()) {
        missing_.emplace_back(val_type, val);
        return;
    }
    // allow extra arguments to be places in an option group if it is allowed there
    for(auto &subc : subcommands_) {
        if(subc->name_.empty() && subc->allow_extras_) {
            subc->missing_.emplace_back(val_type, val);
            return;
        }
    }
    // if we haven't found any place to put them yet put them in missing
    missing_.emplace_back(val_type, val);
}

CLI11_INLINE void App::_move_option(Option *opt, App *app) {
    if(opt == nullptr) {
        throw OptionNotFound("the option is NULL");
    }
    // verify that the give app is actually a subcommand
    bool found = false;
    for(auto &subc : subcommands_) {
        if(app == subc.get()) {
            found = true;
        }
    }
    if(!found) {
        throw OptionNotFound("The Given app is not a subcommand");
    }

    if((help_ptr_ == opt) || (help_all_ptr_ == opt))
        throw OptionAlreadyAdded("cannot move help options");

    if(config_ptr_ == opt)
        throw OptionAlreadyAdded("cannot move config file options");

    auto iterator =
        std::find_if(std::begin(options_), std::end(options_), [opt](const Option_p &v) { return v.get() == opt; });
    if(iterator != std::end(options_)) {
        const auto &opt_p = *iterator;
        if(std::find_if(std::begin(app->options_), std::end(app->options_), [&opt_p](const Option_p &v) {
               return (*v == *opt_p);
           }) == std::end(app->options_)) {
            // only erase after the insertion was successful
            app->options_.push_back(std::move(*iterator));
            options_.erase(iterator);
        } else {
            throw OptionAlreadyAdded("option was not located: " + opt->get_name());
        }
    } else {
        throw OptionNotFound("could not locate the given Option");
    }
}

CLI11_INLINE void TriggerOn(App *trigger_app, App *app_to_enable) {
    app_to_enable->enabled_by_default(false);
    app_to_enable->disabled_by_default();
    trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(false); });
}

CLI11_INLINE void TriggerOn(App *trigger_app, std::vector<App *> apps_to_enable) {
    for(auto &app : apps_to_enable) {
        app->enabled_by_default(false);
        app->disabled_by_default();
    }

    trigger_app->preparse_callback([apps_to_enable](std::size_t) {
        for(const auto &app : apps_to_enable) {
            app->disabled(false);
        }
    });
}

CLI11_INLINE void TriggerOff(App *trigger_app, App *app_to_enable) {
    app_to_enable->disabled_by_default(false);
    app_to_enable->enabled_by_default();
    trigger_app->preparse_callback([app_to_enable](std::size_t) { app_to_enable->disabled(); });
}

CLI11_INLINE void TriggerOff(App *trigger_app, std::vector<App *> apps_to_enable) {
    for(auto &app : apps_to_enable) {
        app->disabled_by_default(false);
        app->enabled_by_default();
    }

    trigger_app->preparse_callback([apps_to_enable](std::size_t) {
        for(const auto &app : apps_to_enable) {
            app->disabled();
        }
    });
}

CLI11_INLINE void deprecate_option(Option *opt, const std::string &replacement) {
    Validator deprecate_warning{[opt, replacement](std::string &) {
                                    std::cout << opt->get_name() << " is deprecated please use '" << replacement
                                              << "' instead\n";
                                    return std::string();
                                },
                                "DEPRECATED"};
    deprecate_warning.application_index(0);
    opt->check(deprecate_warning);
    if(!replacement.empty()) {
        opt->description(opt->get_description() + " DEPRECATED: please use '" + replacement + "' instead");
    }
}

CLI11_INLINE void retire_option(App *app, Option *opt) {
    App temp;
    auto *option_copy = temp.add_option(opt->get_name(false, true))
                            ->type_size(opt->get_type_size_min(), opt->get_type_size_max())
                            ->expected(opt->get_expected_min(), opt->get_expected_max())
                            ->allow_extra_args(opt->get_allow_extra_args());

    app->remove_option(opt);
    auto *opt2 = app->add_option(option_copy->get_name(false, true), "option has been retired and has no effect");
    opt2->type_name("RETIRED")
        ->default_str("RETIRED")
        ->type_size(option_copy->get_type_size_min(), option_copy->get_type_size_max())
        ->expected(option_copy->get_expected_min(), option_copy->get_expected_max())
        ->allow_extra_args(option_copy->get_allow_extra_args());

    // LCOV_EXCL_START
    // something odd with coverage on new compilers
    Validator retired_warning{[opt2](std::string &) {
                                  std::cout << "WARNING " << opt2->get_name() << " is retired and has no effect\n";
                                  return std::string();
                              },
                              ""};
    // LCOV_EXCL_STOP
    retired_warning.application_index(0);
    opt2->check(retired_warning);
}

CLI11_INLINE void retire_option(App &app, Option *opt) { retire_option(&app, opt); }

CLI11_INLINE void retire_option(App *app, const std::string &option_name) {

    auto *opt = app->get_option_no_throw(option_name);
    if(opt != nullptr) {
        retire_option(app, opt);
        return;
    }
    auto *opt2 = app->add_option(option_name, "option has been retired and has no effect")
                     ->type_name("RETIRED")
                     ->expected(0, 1)
                     ->default_str("RETIRED");
    // LCOV_EXCL_START
    // something odd with coverage on new compilers
    Validator retired_warning{[opt2](std::string &) {
                                  std::cout << "WARNING " << opt2->get_name() << " is retired and has no effect\n";
                                  return std::string();
                              },
                              ""};
    // LCOV_EXCL_STOP
    retired_warning.application_index(0);
    opt2->check(retired_warning);
}

CLI11_INLINE void retire_option(App &app, const std::string &option_name) { retire_option(&app, option_name); }

namespace FailureMessage {

CLI11_INLINE std::string simple(const App *app, const Error &e) {
    std::string header = std::string(e.what()) + "\n";
    std::vector<std::string> names;

    // Collect names
    if(app->get_help_ptr() != nullptr)
        names.push_back(app->get_help_ptr()->get_name());

    if(app->get_help_all_ptr() != nullptr)
        names.push_back(app->get_help_all_ptr()->get_name());

    // If any names found, suggest those
    if(!names.empty())
        header += "Run with " + detail::join(names, " or ") + " for more information.\n";

    return header;
}

CLI11_INLINE std::string help(const App *app, const Error &e) {
    std::string header = std::string("ERROR: ") + e.get_name() + ": " + e.what() + "\n";
    header += app->help();
    return header;
}

}  // namespace FailureMessage

// [CLI11:app_inl_hpp:end]
}  // namespace CLI
