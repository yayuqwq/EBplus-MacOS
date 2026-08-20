#include "startup_environment.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace gui::startup_environment {
namespace {

constexpr const char* kHalPluginPath = "MV_HAL_PLUGIN_PATH";
constexpr const char* kHalPluginSearchMode = "MV_HAL_PLUGIN_SEARCH_MODE";
constexpr const char* kHdf5PluginPath = "HDF5_PLUGIN_PATH";
constexpr const char* kQtPlatform = "QT_QPA_PLATFORM";
constexpr const char* kRhiBackend = "QSG_RHI_BACKEND";
constexpr const char* kWaylandDisplay = "WAYLAND_DISPLAY";

constexpr std::array<const char*, 5> kObservedVariables = {
    kHalPluginPath,
    kHdf5PluginPath,
    kQtPlatform,
    kRhiBackend,
    kWaylandDisplay,
};

bool is_set(const Environment& environment, const char* name) {
    return environment.find(name) != environment.end();
}

void capture_if_set(Environment& environment, const char* name) {
    if (const char* value = std::getenv(name)) {
        environment.emplace(name, value);
    }
}

bool is_existing_directory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

bool is_existing_regular_file(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

} // namespace

Platform current_platform() noexcept {
#if defined(__APPLE__)
    return Platform::MacOS;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Other;
#endif
}

Environment compute_default_updates(
    Platform platform,
    const Environment& current_environment) {
    Environment updates;
    if (platform != Platform::Linux) {
        return updates;
    }

    if (!is_set(current_environment, kHalPluginPath)) {
        updates.emplace(kHalPluginPath,
                        "/usr/local/lib/metavision/hal/plugins");
    }
    if (!is_set(current_environment, kHdf5PluginPath)) {
        updates.emplace(kHdf5PluginPath, "/usr/local/lib/hdf5/plugin");
    }
    if (!is_set(current_environment, kQtPlatform) &&
        is_set(current_environment, kWaylandDisplay)) {
        updates.emplace(kQtPlatform, "xcb");
    }
    if (!is_set(current_environment, kRhiBackend)) {
        updates.emplace(kRhiBackend, "opengl");
    }

    return updates;
}

std::optional<BundleRuntimePaths> find_bundle_runtime_paths(
    Platform platform,
    const std::string& application_directory) {
    if (platform != Platform::MacOS) {
        return std::nullopt;
    }

    const std::filesystem::path macos_directory(application_directory);
    const std::filesystem::path contents_directory = macos_directory.parent_path();
    const std::filesystem::path bundle_directory = contents_directory.parent_path();
    if (macos_directory.filename() != "MacOS" ||
        contents_directory.filename() != "Contents" ||
        bundle_directory.extension() != ".app" ||
        !is_existing_directory(macos_directory) ||
        !is_existing_regular_file(contents_directory / "Info.plist")) {
        return std::nullopt;
    }

    const std::filesystem::path frameworks_directory =
        contents_directory / "Frameworks";
    const std::filesystem::path hal_plugins_directory =
        frameworks_directory / "metavision" / "hal" / "plugins";
    const std::filesystem::path hdf5_plugins_directory =
        frameworks_directory / "hdf5" / "plugin";

    if (!is_existing_directory(hal_plugins_directory) ||
        !is_existing_directory(hdf5_plugins_directory) ||
        !is_existing_regular_file(
            hal_plugins_directory / "libhal_plugin_prophesee.dylib") ||
        !is_existing_regular_file(
            hal_plugins_directory / "libhal_plugin_centuryarks.dylib") ||
        !is_existing_regular_file(
            hal_plugins_directory / "libmetavision_psee_hw_layer.dylib") ||
        !is_existing_regular_file(hdf5_plugins_directory / "libH5Zecf.dylib")) {
        return std::nullopt;
    }

    return BundleRuntimePaths{
        hal_plugins_directory.string(),
        hdf5_plugins_directory.string(),
    };
}

Environment compute_bundle_runtime_updates(
    Platform platform,
    const BundleRuntimePaths& paths) {
    Environment updates;
    if (platform != Platform::MacOS || paths.hal_plugin_path.empty() ||
        paths.hdf5_plugin_path.empty()) {
        return updates;
    }

    // A valid bundle is self-contained. Override inherited plugin locations so
    // OpenEB cannot fall back to the producer prefix (or /usr/local) after the
    // bundle has supplied its own runtime components.
    updates.emplace(kHalPluginPath, paths.hal_plugin_path);
    updates.emplace(kHalPluginSearchMode, "PLUGIN_PATH_ONLY");
    updates.emplace(kHdf5PluginPath, paths.hdf5_plugin_path);
    return updates;
}

void apply_defaults_for_current_platform() {
    Environment current_environment;
    for (const char* name : kObservedVariables) {
        capture_if_set(current_environment, name);
    }

    const Environment updates =
        compute_default_updates(current_platform(), current_environment);

#if defined(__APPLE__) || defined(__linux__)
    for (const auto& update : updates) {
        (void)::setenv(update.first.c_str(), update.second.c_str(), 0);
    }
#else
    (void)updates;
#endif
}

void apply_bundle_runtime_environment_for_current_platform(
    const std::string& application_directory) {
    const auto paths = find_bundle_runtime_paths(
        current_platform(), application_directory);
    if (!paths) {
        return;
    }

    const Environment updates =
        compute_bundle_runtime_updates(current_platform(), *paths);

#if defined(__APPLE__) || defined(__linux__)
    for (const auto& update : updates) {
        (void)::setenv(update.first.c_str(), update.second.c_str(), 1);
    }
#else
    (void)updates;
#endif
}

} // namespace gui::startup_environment
