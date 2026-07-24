// gui/tests/test_camera_controller_lifecycle.cpp -- file-open lifecycle regression tests.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "app/camera_controller.h"

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

TEST(CameraControllerLifecycle, EmptyRawFailureIsCaught) {
    const std::filesystem::path artifact_dir =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "camera_controller_lifecycle";
    const std::filesystem::path empty_raw = artifact_dir / "empty.raw";
    std::filesystem::create_directories(artifact_dir);
    std::ofstream raw_file(empty_raw, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(raw_file.is_open());
    raw_file.close();

    gui::CameraController controller;
    int disconnected_count = 0;
    int error_count = 0;
    QString error_message;
    QObject::connect(&controller, &gui::CameraController::disconnected, [&]() {
        ++disconnected_count;
    });
    QObject::connect(&controller, &gui::CameraController::error, [&](const QString& message) {
        ++error_count;
        error_message = message;
    });

    bool opened = true;
    EXPECT_NO_THROW(opened = controller.connect_file(empty_raw.string()));

    EXPECT_FALSE(opened);
    EXPECT_EQ(disconnected_count, 1);
    EXPECT_EQ(error_count, 1);
    EXPECT_FALSE(error_message.isEmpty());
    EXPECT_FALSE(controller.is_connected());
}
