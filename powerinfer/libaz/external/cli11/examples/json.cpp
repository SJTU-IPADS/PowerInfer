// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// This example is only built on GCC 7 on Travis due to mismatch in stdlib
// for clang (CLI11 is forgiving about mismatches, json.hpp is not)

using nlohmann::json;

class ConfigJSON : public CLI::Config {
  public:
    std::string to_config(const CLI::App *app, bool default_also, bool, std::string) const override {

        json j;

        for(const CLI::Option *opt : app->get_options({})) {

            // Only process option with a long-name and configurable
            if(!opt->get_lnames().empty() && opt->get_configurable()) {
                std::string name = opt->get_lnames()[0];

                // Non-flags
                if(opt->get_type_size() != 0) {

                    // If the option was found on command line
                    if(opt->count() == 1)
                        j[name] = opt->results().at(0);
                    else if(opt->count() > 1)
                        j[name] = opt->results();

                    // If the option has a default and is requested by optional argument
                    else if(default_also && !opt->get_default_str().empty())
                        j[name] = opt->get_default_str();

                    // Flag, one passed
                } else if(opt->count() == 1) {
                    j[name] = true;

                    // Flag, multiple passed
                } else if(opt->count() > 1) {
                    j[name] = opt->count();

                    // Flag, not present
                } else if(opt->count() == 0 && default_also) {
                    j[name] = false;
                }
            }
        }

        for(const CLI::App *subcom : app->get_subcommands({}))
            j[subcom->get_name()] = json(to_config(subcom, default_also, false, ""));

        return j.dump(4);
    }

    std::vector<CLI::ConfigItem> from_config(std::istream &input) const override {
        json j;
        input >> j;
        return _from_config(j);
    }

    std::vector<CLI::ConfigItem>
    _from_config(json j, std::string name = "", std::vector<std::string> prefix = {}) const {
        std::vector<CLI::ConfigItem> results;

        if(j.is_object()) {
            for(json::iterator item = j.begin(); item != j.end(); ++item) {
                auto copy_prefix = prefix;
                if(!name.empty())
                    copy_prefix.push_back(name);
                auto sub_results = _from_config(*item, item.key(), copy_prefix);
                results.insert(results.end(), sub_results.begin(), sub_results.end());
            }
        } else if(!name.empty()) {
            results.emplace_back();
            CLI::ConfigItem &res = results.back();
            res.name = name;
            res.parents = prefix;
            if(j.is_boolean()) {
                res.inputs = {j.get<bool>() ? "true" : "false"};
            } else if(j.is_number()) {
                std::stringstream ss;
                ss << j.get<double>();
                res.inputs = {ss.str()};
            } else if(j.is_string()) {
                res.inputs = {j.get<std::string>()};
            } else if(j.is_array()) {
                for(std::string ival : j)
                    res.inputs.push_back(ival);
            } else {
                throw CLI::ConversionError("Failed to convert " + name);
            }
        } else {
            throw CLI::ConversionError("You must make all top level values objects in json!");
        }

        return results;
    }
};

int main(int argc, char **argv) {
    CLI::App app;
    app.config_formatter(std::make_shared<ConfigJSON>());

    int item;

    app.add_flag("--simple");
    app.add_option("--item", item);
    app.set_config("--config");

    CLI11_PARSE(app, argc, argv);

    std::cout << app.config_to_str(true, true) << std::endl;
}
