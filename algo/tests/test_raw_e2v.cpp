// Standalone diagnostic: feed events from a .raw file to EventToVideo and
// report frame statistics. This mimics the GUI runtime pipeline.
//
// Build:
//   cmake --build build --target test_raw_e2v
// Run:
//   ./build/algo/tests/test_raw_e2v test/sparklers.raw

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include "algo/analytics/event_to_video.h"

using gui_algo::EventToVideo;
using gui_algo::Event;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file.raw>\n", argv[0]);
        return 1;
    }
    const std::string path = argv[1];

    Metavision::FileConfigHints hints;
    hints.real_time_playback(false);

    // Get sensor dimensions from a preliminary camera.
    int sensor_w = 0, sensor_h = 0;
    {
        Metavision::Camera cam = Metavision::Camera::from_file(path, hints);
        sensor_w = cam.geometry().get_width();
        sensor_h = cam.geometry().get_height();
    }
    std::printf("Sensor: %dx%d\n", sensor_w, sensor_h);

    const int roi_x = (sensor_w - 128) / 2;
    const int roi_y = (sensor_h - 128) / 2;

    // Test both BardowVariational and InteractingMaps modes.
    for (int mode_idx = 0; mode_idx <= 1; ++mode_idx) {
        const auto mode = static_cast<EventToVideo::Mode>(mode_idx);
        const char* mode_name = (mode_idx == 0) ? "BardowVariational" : "InteractingMaps";

        Metavision::Camera cam = Metavision::Camera::from_file(path, hints);

        EventToVideo v(128, 128, mode);
        v.set_downsample(true);
        v.set_output_fps(24);

        int total_events = 0;
        int frame_count = 0;
        int flat_count = 0;

        cam.cd().add_callback([&](const Metavision::EventCD* b, const Metavision::EventCD* e) {
            const std::size_t n = static_cast<std::size_t>(e - b);
            std::vector<Event> evs;
            evs.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                const int x = b[i].x, y = b[i].y;
                if (x < roi_x || x >= roi_x + 128 || y < roi_y || y >= roi_y + 128) continue;
                evs.emplace_back(
                    static_cast<uint16_t>(x - roi_x),
                    static_cast<uint16_t>(y - roi_y),
                    static_cast<uint8_t>(b[i].p),
                    static_cast<uint64_t>(b[i].t));
            }
            v.process(evs.data(), evs.size());
            total_events += static_cast<int>(evs.size());

            cv::Mat frame = v.get_frame();
            if (!frame.empty()) {
                frame_count++;
                double mn = 0, mx = 0;
                cv::minMaxLoc(frame, &mn, &mx);
                if (frame_count <= 10 || frame_count % 100 == 0) {
                    double mean = cv::mean(frame)[0];
                    std::printf("[%s] frame %d: %dx%d min=%.1f max=%.1f mean=%.1f (events: %d)\n",
                                mode_name, frame_count, frame.cols, frame.rows,
                                mn, mx, mean, total_events);
                }
                if (mx - mn < 1.0) flat_count++;
            }
        });

        cam.start();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        cam.stop();

        std::printf("[%s] Total: %d events, %d frames, %d flat (%.1f%%)\n",
                    mode_name, total_events, frame_count, flat_count,
                    frame_count > 0 ? 100.0 * flat_count / frame_count : 0.0);
        std::printf("---\n");
    }

    return 0;
}
