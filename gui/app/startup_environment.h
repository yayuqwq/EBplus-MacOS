#ifndef GUI_APP_STARTUP_ENVIRONMENT_H
#define GUI_APP_STARTUP_ENVIRONMENT_H

#include <map>
#include <optional>
#include <string>

namespace gui::startup_environment {

enum class Platform {
    Linux,
    MacOS,
    Other,
};

using Environment = std::map<std::string, std::string>;

struct BundleRuntimePaths {
    std::string hal_plugin_path;
    std::string hdf5_plugin_path;
};

Platform current_platform() noexcept;

Environment compute_default_updates(
    Platform platform,
    const Environment& current_environment);

std::optional<BundleRuntimePaths> find_bundle_runtime_paths(
    Platform platform,
    const std::string& application_directory);

Environment compute_bundle_runtime_updates(
    Platform platform,
    const BundleRuntimePaths& paths);

void apply_defaults_for_current_platform();

void apply_bundle_runtime_environment_for_current_platform(
    const std::string& application_directory);

} // namespace gui::startup_environment

#endif // GUI_APP_STARTUP_ENVIRONMENT_H
