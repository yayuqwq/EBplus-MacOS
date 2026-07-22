#include "startup_environment.h"

#include <array>
#include <cstdlib>

namespace gui::startup_environment {
namespace {

constexpr const char* kHalPluginPath = "MV_HAL_PLUGIN_PATH";
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

} // namespace gui::startup_environment
