#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>

#include "app/startup_environment.h"

namespace {

using gui::startup_environment::Environment;
using gui::startup_environment::Platform;
using gui::startup_environment::compute_default_updates;

constexpr const char* kHalPluginPath = "MV_HAL_PLUGIN_PATH";
constexpr const char* kHdf5PluginPath = "HDF5_PLUGIN_PATH";
constexpr const char* kQtPlatform = "QT_QPA_PLATFORM";
constexpr const char* kRhiBackend = "QSG_RHI_BACKEND";
constexpr const char* kWaylandDisplay = "WAYLAND_DISPLAY";

bool has_key(const Environment& environment, const char* name) {
    return environment.find(name) != environment.end();
}

TEST(StartupEnvironmentPolicy, LinuxAddsFourDefaultsWhenWaylandPresent) {
    const Environment current = {{kWaylandDisplay, "wayland-0"}};

    const Environment updates =
        compute_default_updates(Platform::Linux, current);

    ASSERT_EQ(updates.size(), 4U);
    EXPECT_EQ(updates.at(kHalPluginPath),
              "/usr/local/lib/metavision/hal/plugins");
    EXPECT_EQ(updates.at(kHdf5PluginPath), "/usr/local/lib/hdf5/plugin");
    EXPECT_EQ(updates.at(kQtPlatform), "xcb");
    EXPECT_EQ(updates.at(kRhiBackend), "opengl");
}

TEST(StartupEnvironmentPolicy, LinuxPreservesExistingHalPluginPath) {
    const Environment current = {
        {kHalPluginPath, "/custom/hal"},
        {kWaylandDisplay, "wayland-0"},
    };

    const Environment updates =
        compute_default_updates(Platform::Linux, current);

    EXPECT_FALSE(has_key(updates, kHalPluginPath));
    EXPECT_EQ(updates.size(), 3U);
}

TEST(StartupEnvironmentPolicy, LinuxPreservesExistingHdf5PluginPath) {
    const Environment current = {
        {kHdf5PluginPath, "/custom/hdf5"},
        {kWaylandDisplay, "wayland-0"},
    };

    const Environment updates =
        compute_default_updates(Platform::Linux, current);

    EXPECT_FALSE(has_key(updates, kHdf5PluginPath));
    EXPECT_EQ(updates.size(), 3U);
}

TEST(StartupEnvironmentPolicy, LinuxPreservesExistingQtPlatform) {
    const Environment current = {
        {kQtPlatform, "wayland"},
        {kWaylandDisplay, "wayland-0"},
    };

    const Environment updates =
        compute_default_updates(Platform::Linux, current);

    EXPECT_FALSE(has_key(updates, kQtPlatform));
    EXPECT_EQ(updates.size(), 3U);
}

TEST(StartupEnvironmentPolicy, LinuxOmitsQtPlatformWithoutWayland) {
    const Environment updates =
        compute_default_updates(Platform::Linux, Environment{});

    EXPECT_FALSE(has_key(updates, kQtPlatform));
    EXPECT_EQ(updates.size(), 3U);
}

TEST(StartupEnvironmentPolicy, LinuxPreservesExistingRhiBackend) {
    const Environment current = {
        {kRhiBackend, "software"},
        {kWaylandDisplay, "wayland-0"},
    };

    const Environment updates =
        compute_default_updates(Platform::Linux, current);

    EXPECT_FALSE(has_key(updates, kRhiBackend));
    EXPECT_EQ(updates.size(), 3U);
}

TEST(StartupEnvironmentPolicy, LinuxTreatsEmptyStringsAsPresent) {
    const Environment empty_managed_values = {
        {kHalPluginPath, ""},
        {kHdf5PluginPath, ""},
        {kQtPlatform, ""},
        {kRhiBackend, ""},
        {kWaylandDisplay, ""},
    };

    EXPECT_TRUE(compute_default_updates(Platform::Linux,
                                        empty_managed_values).empty());

    const Environment empty_wayland = {{kWaylandDisplay, ""}};
    const Environment updates =
        compute_default_updates(Platform::Linux, empty_wayland);
    ASSERT_EQ(updates.size(), 4U);
    EXPECT_EQ(updates.at(kQtPlatform), "xcb");
}

TEST(StartupEnvironmentPolicy, MacOSAddsNoDefaults) {
    EXPECT_TRUE(compute_default_updates(Platform::MacOS,
                                        Environment{}).empty());
}

TEST(StartupEnvironmentPolicy, MacOSIgnoresWaylandDisplay) {
    const Environment current = {{kWaylandDisplay, "wayland-0"}};

    EXPECT_TRUE(compute_default_updates(Platform::MacOS, current).empty());
}

TEST(StartupEnvironmentPolicy, MacOSPreservesUserValues) {
    const Environment current = {
        {kHalPluginPath, "/custom/hal"},
        {kHdf5PluginPath, "/custom/hdf5"},
        {kQtPlatform, "cocoa"},
        {kRhiBackend, "metal"},
        {kWaylandDisplay, "wayland-0"},
    };
    const Environment original = current;

    EXPECT_TRUE(compute_default_updates(Platform::MacOS, current).empty());
    EXPECT_EQ(current, original);
}

TEST(StartupEnvironmentPolicy, OtherAddsNoDefaultsOrOverrides) {
    const Environment current = {
        {kHalPluginPath, "/custom/hal"},
        {kHdf5PluginPath, "/custom/hdf5"},
        {kQtPlatform, "custom"},
        {kRhiBackend, "custom"},
        {kWaylandDisplay, "custom"},
    };
    const Environment original = current;

    EXPECT_TRUE(compute_default_updates(Platform::Other, current).empty());
    EXPECT_EQ(current, original);
}

TEST(StartupEnvironmentPolicy, EmitsOnlyApprovedKeys) {
    const std::array<std::string, 4> approved_keys = {
        kHalPluginPath,
        kHdf5PluginPath,
        kQtPlatform,
        kRhiBackend,
    };
    const std::array<Platform, 3> platforms = {
        Platform::Linux,
        Platform::MacOS,
        Platform::Other,
    };
    const Environment current = {{kWaylandDisplay, "wayland-0"}};

    for (Platform platform : platforms) {
        const Environment updates =
            compute_default_updates(platform, current);
        for (const auto& update : updates) {
            EXPECT_NE(std::find(approved_keys.begin(), approved_keys.end(),
                                update.first),
                      approved_keys.end());
        }
    }
}

TEST(StartupEnvironmentPlatform, CurrentCompilerTargetMapsToExpectedPlatform) {
#if defined(__APPLE__)
    EXPECT_EQ(gui::startup_environment::current_platform(), Platform::MacOS);
#elif defined(__linux__)
    EXPECT_EQ(gui::startup_environment::current_platform(), Platform::Linux);
#else
    EXPECT_EQ(gui::startup_environment::current_platform(), Platform::Other);
#endif
}

} // namespace
