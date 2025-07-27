// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <CLI/CLI.hpp>
#include <string>

int main(int argc, char **argv) {

    CLI::App app("Vision Application");
    app.set_help_all_flag("--help-all", "Expand all help");
    app.add_flag("--version", "Get version");

    CLI::App *cameraApp = app.add_subcommand("camera", "Configure the app camera");
    cameraApp->require_subcommand(0, 1);  // 0 (default) or 1 camera

    std::string mvcamera_config_file = "mvcamera_config.json";
    CLI::App *mvcameraApp = cameraApp->add_subcommand("mvcamera", "MatrixVision Camera Configuration");
    mvcameraApp->add_option("-c,--config", mvcamera_config_file, "Config filename")
        ->capture_default_str()
        ->check(CLI::ExistingFile);

    std::string mock_camera_path;
    CLI::App *mockcameraApp = cameraApp->add_subcommand("mock", "Mock Camera Configuration");
    mockcameraApp->add_option("-p,--path", mock_camera_path, "Path")->required()->check(CLI::ExistingPath);

    CLI11_PARSE(app, argc, argv);
}
