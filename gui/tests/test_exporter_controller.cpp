// gui/tests/test_exporter_controller.cpp -- HDF5 export metadata regression test.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <chrono>
#include <filesystem>

#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include "exporter/exporter_controller.h"

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

#ifndef EBPLUS_REPO_ROOT
#error "EBPLUS_REPO_ROOT must be defined"
#endif

TEST(ExporterController, Hdf5ExportPreservesGeometry) {
    const std::filesystem::path artifact_dir =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "exporter_controller";
    std::filesystem::create_directories(artifact_dir);
    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path output = artifact_dir / ("sparklers-" + std::to_string(unique_id) + ".h5");
    const std::filesystem::path source = std::filesystem::path(EBPLUS_REPO_ROOT) / "algo/tests/sparklers.raw";

    gui::ExporterController controller;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    bool timed_out = false;
    QString failure;
    QObject::connect(&controller, &gui::ExporterController::completed, [&](const QString&) {
        completed = true;
        loop.quit();
    });
    QObject::connect(&controller, &gui::ExporterController::failed, [&](const QString& message) {
        failure = message;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        timed_out = true;
        loop.quit();
    });

    gui::ExportParams params;
    params.source_path = QString::fromStdString(source.string());
    params.output_path = QString::fromStdString(output.string());
    params.format = gui::ExportParams::Format::HDF5;
    ASSERT_TRUE(controller.start(params));
    timeout.start(30000);
    loop.exec();

    ASSERT_FALSE(timed_out);
    ASSERT_TRUE(completed) << failure.toStdString();
    ASSERT_TRUE(std::filesystem::is_regular_file(output));

    auto source_camera = Metavision::Camera::from_file(
        source.string(), Metavision::FileConfigHints().real_time_playback(false));
    const auto& source_geometry = source_camera.geometry();
    auto camera = Metavision::Camera::from_file(
        output.string(), Metavision::FileConfigHints().real_time_playback(false));
    const auto& geometry = camera.geometry();
    EXPECT_EQ(geometry.get_width(), source_geometry.get_width());
    EXPECT_EQ(geometry.get_height(), source_geometry.get_height());
    EXPECT_GT(camera.offline_streaming_control().get_duration(), 0);
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
