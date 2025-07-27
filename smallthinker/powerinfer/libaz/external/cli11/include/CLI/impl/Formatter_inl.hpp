// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../Formatter.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

namespace CLI {
// [CLI11:formatter_inl_hpp:verbatim]
CLI11_INLINE std::string
Formatter::make_group(std::string group, bool is_positional, std::vector<const Option *> opts) const {
    std::stringstream out;

    out << "\n" << group << ":\n";
    for(const Option *opt : opts) {
        out << make_option(opt, is_positional);
    }

    return out.str();
}

CLI11_INLINE std::string Formatter::make_positionals(const App *app) const {
    std::vector<const Option *> opts =
        app->get_options([](const Option *opt) { return !opt->get_group().empty() && opt->get_positional(); });

    if(opts.empty())
        return {};

    return make_group(get_label("POSITIONALS"), true, opts);
}

CLI11_INLINE std::string Formatter::make_groups(const App *app, AppFormatMode mode) const {
    std::stringstream out;
    std::vector<std::string> groups = app->get_groups();

    // Options
    for(const std::string &group : groups) {
        std::vector<const Option *> opts = app->get_options([app, mode, &group](const Option *opt) {
            return opt->get_group() == group                     // Must be in the right group
                   && opt->nonpositional()                       // Must not be a positional
                   && (mode != AppFormatMode::Sub                // If mode is Sub, then
                       || (app->get_help_ptr() != opt            // Ignore help pointer
                           && app->get_help_all_ptr() != opt));  // Ignore help all pointer
        });
        if(!group.empty() && !opts.empty()) {
            out << make_group(group, false, opts);

            // Removed double newline between groups for consistency of help text
            // if(group != groups.back())
            //    out << "\n";
        }
    }

    return out.str();
}

CLI11_INLINE std::string Formatter::make_description(const App *app) const {
    std::string desc = app->get_description();
    auto min_options = app->get_require_option_min();
    auto max_options = app->get_require_option_max();

    if(app->get_required()) {
        desc += " " + get_label("REQUIRED") + " ";
    }

    if(min_options > 0) {
        if(max_options == min_options) {
            desc += " \n[Exactly " + std::to_string(min_options) + " of the following options are required]";
        } else if(max_options > 0) {
            desc += " \n[Between " + std::to_string(min_options) + " and " + std::to_string(max_options) +
                    " of the following options are required]";
        } else {
            desc += " \n[At least " + std::to_string(min_options) + " of the following options are required]";
        }
    } else if(max_options > 0) {
        desc += " \n[At most " + std::to_string(max_options) + " of the following options are allowed]";
    }

    return (!desc.empty()) ? desc + "\n\n" : std::string{};
}

CLI11_INLINE std::string Formatter::make_usage(const App *app, std::string name) const {
    std::string usage = app->get_usage();
    if(!usage.empty()) {
        return usage + "\n\n";
    }

    std::stringstream out;
    out << '\n';

    if(name.empty())
        out << get_label("Usage") << ':';
    else
        out << name;

    std::vector<std::string> groups = app->get_groups();

    // Print an Options badge if any options exist
    std::vector<const Option *> non_pos_options =
        app->get_options([](const Option *opt) { return opt->nonpositional(); });
    if(!non_pos_options.empty())
        out << " [" << get_label("OPTIONS") << "]";

    // Positionals need to be listed here
    std::vector<const Option *> positionals = app->get_options([](const Option *opt) { return opt->get_positional(); });

    // Print out positionals if any are left
    if(!positionals.empty()) {
        // Convert to help names
        std::vector<std::string> positional_names(positionals.size());
        std::transform(positionals.begin(), positionals.end(), positional_names.begin(), [this](const Option *opt) {
            return make_option_usage(opt);
        });

        out << " " << detail::join(positional_names, " ");
    }

    // Add a marker if subcommands are expected or optional
    if(!app->get_subcommands(
               [](const CLI::App *subc) { return ((!subc->get_disabled()) && (!subc->get_name().empty())); })
            .empty()) {
        out << ' ' << (app->get_require_subcommand_min() == 0 ? "[" : "")
            << get_label(app->get_require_subcommand_max() < 2 || app->get_require_subcommand_min() > 1 ? "SUBCOMMAND"
                                                                                                        : "SUBCOMMANDS")
            << (app->get_require_subcommand_min() == 0 ? "]" : "");
    }

    out << "\n\n";

    return out.str();
}

CLI11_INLINE std::string Formatter::make_footer(const App *app) const {
    std::string footer = app->get_footer();
    if(footer.empty()) {
        return std::string{};
    }
    return '\n' + footer + "\n\n";
}

CLI11_INLINE std::string Formatter::make_help(const App *app, std::string name, AppFormatMode mode) const {
    // This immediately forwards to the make_expanded method. This is done this way so that subcommands can
    // have overridden formatters
    if(mode == AppFormatMode::Sub)
        return make_expanded(app, mode);

    std::stringstream out;
    if((app->get_name().empty()) && (app->get_parent() != nullptr)) {
        if(app->get_group() != "SUBCOMMANDS") {
            out << app->get_group() << ':';
        }
    }

    detail::streamOutAsParagraph(
        out, make_description(app), description_paragraph_width_, "");  // Format description as paragraph
    out << make_usage(app, name);
    out << make_positionals(app);
    out << make_groups(app, mode);
    out << make_subcommands(app, mode);
    detail::streamOutAsParagraph(out, make_footer(app), footer_paragraph_width_);  // Format footer as paragraph

    return out.str();
}

CLI11_INLINE std::string Formatter::make_subcommands(const App *app, AppFormatMode mode) const {
    std::stringstream out;

    std::vector<const App *> subcommands = app->get_subcommands({});

    // Make a list in definition order of the groups seen
    std::vector<std::string> subcmd_groups_seen;
    for(const App *com : subcommands) {
        if(com->get_name().empty()) {
            if(!com->get_group().empty() && com->get_group().front() != '+') {
                out << make_expanded(com, mode);
            }
            continue;
        }
        std::string group_key = com->get_group();
        if(!group_key.empty() &&
           std::find_if(subcmd_groups_seen.begin(), subcmd_groups_seen.end(), [&group_key](std::string a) {
               return detail::to_lower(a) == detail::to_lower(group_key);
           }) == subcmd_groups_seen.end())
            subcmd_groups_seen.push_back(group_key);
    }

    // For each group, filter out and print subcommands
    for(const std::string &group : subcmd_groups_seen) {
        out << '\n' << group << ":\n";
        std::vector<const App *> subcommands_group = app->get_subcommands(
            [&group](const App *sub_app) { return detail::to_lower(sub_app->get_group()) == detail::to_lower(group); });
        for(const App *new_com : subcommands_group) {
            if(new_com->get_name().empty())
                continue;
            if(mode != AppFormatMode::All) {
                out << make_subcommand(new_com);
            } else {
                out << new_com->help(new_com->get_name(), AppFormatMode::Sub);
                out << '\n';
            }
        }
    }

    return out.str();
}

CLI11_INLINE std::string Formatter::make_subcommand(const App *sub) const {
    std::stringstream out;
    std::string name = "  " + sub->get_display_name(true) + (sub->get_required() ? " " + get_label("REQUIRED") : "");

    out << std::setw(static_cast<int>(column_width_)) << std::left << name;
    detail::streamOutAsParagraph(
        out, sub->get_description(), right_column_width_, std::string(column_width_, ' '), true);
    out << '\n';
    return out.str();
}

CLI11_INLINE std::string Formatter::make_expanded(const App *sub, AppFormatMode mode) const {
    std::stringstream out;
    out << sub->get_display_name(true) << '\n';

    detail::streamOutAsParagraph(
        out, make_description(sub), description_paragraph_width_, "  ");  // Format description as paragraph

    if(sub->get_name().empty() && !sub->get_aliases().empty()) {
        detail::format_aliases(out, sub->get_aliases(), column_width_ + 2);
    }

    out << make_positionals(sub);
    out << make_groups(sub, mode);
    out << make_subcommands(sub, mode);
    detail::streamOutAsParagraph(out, make_footer(sub), footer_paragraph_width_);  // Format footer as paragraph

    out << '\n';
    return out.str();
}

CLI11_INLINE std::string Formatter::make_option(const Option *opt, bool is_positional) const {
    std::stringstream out;
    if(is_positional) {
        const std::string left = "  " + make_option_name(opt, true) + make_option_opts(opt);
        const std::string desc = make_option_desc(opt);
        out << std::setw(static_cast<int>(column_width_)) << std::left << left;

        if(!desc.empty()) {
            bool skipFirstLinePrefix = true;
            if(left.length() >= column_width_) {
                out << '\n';
                skipFirstLinePrefix = false;
            }
            detail::streamOutAsParagraph(
                out, desc, right_column_width_, std::string(column_width_, ' '), skipFirstLinePrefix);
        }
    } else {
        const std::string namesCombined = make_option_name(opt, false);
        const std::string opts = make_option_opts(opt);
        const std::string desc = make_option_desc(opt);

        // Split all names at comma and sort them into short names and long names
        const auto names = detail::split(namesCombined, ',');
        std::vector<std::string> vshortNames;
        std::vector<std::string> vlongNames;
        std::for_each(names.begin(), names.end(), [&vshortNames, &vlongNames](const std::string &name) {
            if(name.find("--", 0) != std::string::npos)
                vlongNames.push_back(name);
            else
                vshortNames.push_back(name);
        });

        // Assemble short and long names
        std::string shortNames = detail::join(vshortNames, ", ");
        std::string longNames = detail::join(vlongNames, ", ");

        // Calculate setw sizes
        const auto shortNamesColumnWidth = static_cast<int>(column_width_ / 3);  // 33% left for short names
        const auto longNamesColumnWidth = static_cast<int>(std::ceil(
            static_cast<float>(column_width_) / 3.0f * 2.0f));  // 66% right for long names and options, ceil result
        int shortNamesOverSize = 0;

        // Print short names
        if(shortNames.length() > 0) {
            shortNames = "  " + shortNames;  // Indent
            if(longNames.length() == 0 && opts.length() > 0)
                shortNames += opts;  // Add opts if only short names and no long names
            if(longNames.length() > 0)
                shortNames += ",";
            if(static_cast<int>(shortNames.length()) >= shortNamesColumnWidth) {
                shortNames += " ";
                shortNamesOverSize = static_cast<int>(shortNames.length()) - shortNamesColumnWidth;
            }
            out << std::setw(shortNamesColumnWidth) << std::left << shortNames;
        } else {
            out << std::setw(shortNamesColumnWidth) << std::left << "";
        }

        // Adjust long name column width in case of short names column reaching into long names column
        shortNamesOverSize =
            (std::min)(shortNamesOverSize, longNamesColumnWidth);  // Prevent negative result with unsigned integers
        const auto adjustedLongNamesColumnWidth = longNamesColumnWidth - shortNamesOverSize;

        // Print long names
        if(longNames.length() > 0) {
            if(opts.length() > 0)
                longNames += opts;
            if(static_cast<int>(longNames.length()) >= adjustedLongNamesColumnWidth)
                longNames += " ";

            out << std::setw(adjustedLongNamesColumnWidth) << std::left << longNames;
        } else {
            out << std::setw(adjustedLongNamesColumnWidth) << std::left << "";
        }

        if(!desc.empty()) {
            bool skipFirstLinePrefix = true;
            if(out.str().length() > column_width_) {
                out << '\n';
                skipFirstLinePrefix = false;
            }
            detail::streamOutAsParagraph(
                out, desc, right_column_width_, std::string(column_width_, ' '), skipFirstLinePrefix);
        }
    }

    out << '\n';
    return out.str();
}

CLI11_INLINE std::string Formatter::make_option_name(const Option *opt, bool is_positional) const {
    if(is_positional)
        return opt->get_name(true, false);

    return opt->get_name(false, true);
}

CLI11_INLINE std::string Formatter::make_option_opts(const Option *opt) const {
    std::stringstream out;

    if(!opt->get_option_text().empty()) {
        out << " " << opt->get_option_text();
    } else {
        if(opt->get_type_size() != 0) {
            if(!opt->get_type_name().empty())
                out << " " << get_label(opt->get_type_name());
            if(!opt->get_default_str().empty())
                out << " [" << opt->get_default_str() << "] ";
            if(opt->get_expected_max() == detail::expected_max_vector_size)
                out << " ...";
            else if(opt->get_expected_min() > 1)
                out << " x " << opt->get_expected();

            if(opt->get_required())
                out << " " << get_label("REQUIRED");
        }
        if(!opt->get_envname().empty())
            out << " (" << get_label("Env") << ":" << opt->get_envname() << ")";
        if(!opt->get_needs().empty()) {
            out << " " << get_label("Needs") << ":";
            for(const Option *op : opt->get_needs())
                out << " " << op->get_name();
        }
        if(!opt->get_excludes().empty()) {
            out << " " << get_label("Excludes") << ":";
            for(const Option *op : opt->get_excludes())
                out << " " << op->get_name();
        }
    }
    return out.str();
}

CLI11_INLINE std::string Formatter::make_option_desc(const Option *opt) const { return opt->get_description(); }

CLI11_INLINE std::string Formatter::make_option_usage(const Option *opt) const {
    // Note that these are positionals usages
    std::stringstream out;
    out << make_option_name(opt, true);
    if(opt->get_expected_max() >= detail::expected_max_vector_size)
        out << "...";
    else if(opt->get_expected_max() > 1)
        out << "(" << opt->get_expected() << "x)";

    return opt->get_required() ? out.str() : "[" + out.str() + "]";
}
// [CLI11:formatter_inl_hpp:end]
}  // namespace CLI
