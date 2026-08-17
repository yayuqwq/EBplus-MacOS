// gui/tests/test_file_converter.cpp -- deterministic CSV and RAW cut contracts.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QString>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/file_config_hints.h>

#include "app/file_converter.h"

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

#ifndef EBPLUS_REPO_ROOT
#error "EBPLUS_REPO_ROOT must be defined"
#endif

namespace {

using namespace std::chrono_literals;

constexpr int kOperationTimeoutMs = 30000;
constexpr auto kDecodeTimeout     = 30s;

struct DecodedEventFile {
    int width{0};
    int height{0};
    std::string encoding;
    std::vector<Metavision::EventCD> events;
};

struct OperationOutcome {
    bool completed{false};
    bool failed{false};
    bool timed_out{false};
    QString output;
    QString failure;
};

struct CsvComparison {
    std::string header;
    std::size_t data_rows{0};
    std::string first_mismatch;
};

struct EventComparison {
    std::size_t actual_count{0};
    std::string first_mismatch;
};

const std::filesystem::path &source_fixture() {
    static const std::filesystem::path source =
        std::filesystem::path(EBPLUS_REPO_ROOT) / "algo/tests/sparklers.raw";
    return source;
}

std::filesystem::path make_artifact_root(const char *case_name) {
    static std::atomic<std::uint64_t> sequence{0};
    const std::filesystem::path base =
        std::filesystem::path(EBPLUS_GUI_TEST_ARTIFACT_DIR) / "file_converter";
    std::filesystem::create_directories(base);

    const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto serial = sequence.fetch_add(1, std::memory_order_relaxed);
    const std::filesystem::path root =
        base / (std::string(case_name) + "-" + std::to_string(unique_id) + "-" +
                std::to_string(serial));
    if (!std::filesystem::create_directory(root)) {
        throw std::runtime_error("could not create unique file-converter test artifact root");
    }
    if (std::filesystem::is_symlink(root)) {
        throw std::runtime_error("file-converter test artifact root must not be a symlink");
    }
    return root;
}

std::string describe_event(const Metavision::EventCD &event) {
    std::ostringstream stream;
    stream << "{t=" << static_cast<long long>(event.t) << ", x=" << event.x
           << ", y=" << event.y << ", p=" << static_cast<int>(event.p) << '}';
    return stream.str();
}

bool same_event(const Metavision::EventCD &expected, const Metavision::EventCD &actual) {
    return expected.t == actual.t && expected.x == actual.x && expected.y == actual.y &&
           expected.p == actual.p;
}

DecodedEventFile decode_event_file(const std::filesystem::path &path, bool time_shift) {
    Metavision::FileConfigHints hints;
    hints.real_time_playback(false);
    hints.time_shift(time_shift);

    Metavision::Camera camera = Metavision::Camera::from_file(path.string(), hints);
    const auto &geometry = camera.geometry();

    DecodedEventFile decoded;
    decoded.width = geometry.get_width();
    decoded.height = geometry.get_height();
    decoded.encoding = camera.get_camera_configuration().data_encoding_format;

    const auto callback_id = camera.cd().add_callback(
        [&](const Metavision::EventCD *begin, const Metavision::EventCD *end) {
            decoded.events.insert(decoded.events.end(), begin, end);
        });

    try {
        camera.start();
        const auto deadline = std::chrono::steady_clock::now() + kDecodeTimeout;
        while (camera.is_running() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(5ms);
        }
        if (camera.is_running()) {
            camera.stop();
            throw std::runtime_error("timed out while decoding " + path.string());
        }
    } catch (...) {
        try {
            if (camera.is_running()) {
                camera.stop();
            }
        } catch (...) {
        }
        camera.cd().remove_callback(callback_id);
        throw;
    }

    camera.cd().remove_callback(callback_id);
    return decoded;
}

template<typename StartOperation>
OperationOutcome run_file_converter(StartOperation &&start_operation) {
    gui::FileConverter converter;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    OperationOutcome outcome;
    QObject::connect(&converter, &gui::FileConverter::completed, &loop,
                     [&](const QString &output) {
                         outcome.completed = true;
                         outcome.output = output;
                         loop.quit();
                     });
    QObject::connect(&converter, &gui::FileConverter::failed, &loop,
                     [&](const QString &message) {
                         outcome.failed = true;
                         outcome.failure = message;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        outcome.timed_out = true;
        loop.quit();
    });

    std::forward<StartOperation>(start_operation)(converter);
    timeout.start(kOperationTimeoutMs);
    loop.exec();
    timeout.stop();
    return outcome;
}

long long parse_integer_field(const std::string &field) {
    std::size_t consumed = 0;
    const auto value = std::stoll(field, &consumed);
    if (consumed != field.size()) {
        throw std::runtime_error("trailing data in numeric CSV field: " + field);
    }
    return value;
}

CsvComparison compare_csv_to_events(const std::filesystem::path &csv_path,
                                    const std::vector<Metavision::EventCD> &expected) {
    std::ifstream stream(csv_path);
    if (!stream.is_open()) {
        throw std::runtime_error("could not open CSV output for readback");
    }

    CsvComparison comparison;
    if (!std::getline(stream, comparison.header)) {
        comparison.first_mismatch = "CSV output has no header";
        return comparison;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t index = comparison.data_rows++;
        if (!comparison.first_mismatch.empty()) {
            continue;
        }
        if (index >= expected.size()) {
            comparison.first_mismatch = "extra CSV row at index " + std::to_string(index);
            continue;
        }

        std::istringstream row_stream(line);
        std::string t_field;
        std::string x_field;
        std::string y_field;
        std::string p_field;
        std::string extra_field;
        if (!std::getline(row_stream, t_field, ',') ||
            !std::getline(row_stream, x_field, ',') ||
            !std::getline(row_stream, y_field, ',') ||
            !std::getline(row_stream, p_field, ',') ||
            std::getline(row_stream, extra_field, ',')) {
            comparison.first_mismatch = "malformed CSV row at index " + std::to_string(index) +
                                        ": " + line;
            continue;
        }

        try {
            const auto t = static_cast<Metavision::timestamp>(parse_integer_field(t_field));
            const auto x = parse_integer_field(x_field);
            const auto y = parse_integer_field(y_field);
            const auto p = parse_integer_field(p_field);
            const auto &event = expected[index];
            if (t != event.t || x != event.x || y != event.y || p != event.p) {
                std::ostringstream details;
                details << "CSV row mismatch at index " << index << ": expected "
                        << describe_event(event) << ", actual {t=" << t << ", x=" << x
                        << ", y=" << y << ", p=" << p << '}';
                comparison.first_mismatch = details.str();
            }
        } catch (const std::exception &error) {
            comparison.first_mismatch = "invalid CSV row at index " + std::to_string(index) +
                                        ": " + error.what();
        }
    }

    if (comparison.first_mismatch.empty() && comparison.data_rows != expected.size()) {
        comparison.first_mismatch = "CSV row count differs: expected " +
                                    std::to_string(expected.size()) + ", actual " +
                                    std::to_string(comparison.data_rows);
    }
    return comparison;
}

EventComparison compare_event_sequences(const std::vector<Metavision::EventCD> &expected,
                                        const std::vector<Metavision::EventCD> &actual) {
    EventComparison comparison;
    comparison.actual_count = actual.size();
    const std::size_t common_count = std::min(expected.size(), actual.size());
    for (std::size_t index = 0; index < common_count; ++index) {
        if (!same_event(expected[index], actual[index])) {
            comparison.first_mismatch = "event mismatch at index " + std::to_string(index) +
                                        ": expected " + describe_event(expected[index]) +
                                        ", actual " + describe_event(actual[index]);
            return comparison;
        }
    }
    if (expected.size() != actual.size()) {
        std::ostringstream details;
        details << "event count differs: expected " << expected.size() << ", actual "
                << actual.size();
        if (actual.size() > expected.size()) {
            details << ", first extra " << describe_event(actual[expected.size()]);
        } else if (expected.size() > actual.size()) {
            details << ", first missing " << describe_event(expected[actual.size()]);
        }
        comparison.first_mismatch = details.str();
    }
    return comparison;
}

std::vector<Metavision::EventCD> filter_inclusive(
    const std::vector<Metavision::EventCD> &events, Metavision::timestamp start_us,
    Metavision::timestamp end_us) {
    std::vector<Metavision::EventCD> filtered;
    for (const auto &event : events) {
        if (event.t >= start_us && event.t <= end_us) {
            filtered.push_back(event);
        }
    }
    return filtered;
}

} // namespace

TEST(FileConverter, CsvExportsEverySourceCdEventExactly) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const DecodedEventFile source = decode_event_file(source_fixture(), true);
    ASSERT_FALSE(source.events.empty());

    const std::filesystem::path artifact_root = make_artifact_root("csv-exact");
    const std::filesystem::path output = artifact_root / "sparklers.csv";
    const OperationOutcome outcome = run_file_converter([&](gui::FileConverter &converter) {
        converter.convert(QString::fromStdString(source_fixture().string()),
                          QString::fromStdString(output.string()), gui::FileConverter::Format::CSV);
    });

    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.failed) << outcome.failure.toStdString();
    ASSERT_TRUE(outcome.completed);
    EXPECT_EQ(outcome.output.toStdString(), output.string());
    ASSERT_TRUE(std::filesystem::is_regular_file(output));
    ASSERT_GT(std::filesystem::file_size(output), 0u);

    const CsvComparison comparison = compare_csv_to_events(output, source.events);
    EXPECT_EQ(comparison.header, "t,x,y,p");
    EXPECT_EQ(comparison.data_rows, source.events.size());
    EXPECT_TRUE(comparison.first_mismatch.empty()) << comparison.first_mismatch;
}

TEST(FileConverter, CsvMissingParentFailsWithoutCompleted) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const std::filesystem::path artifact_root = make_artifact_root("csv-missing-parent");
    const std::filesystem::path missing_parent = artifact_root / "missing-parent";
    const std::filesystem::path output = missing_parent / "output.csv";
    ASSERT_FALSE(std::filesystem::exists(missing_parent));

    const OperationOutcome outcome = run_file_converter([&](gui::FileConverter &converter) {
        converter.convert(QString::fromStdString(source_fixture().string()),
                          QString::fromStdString(output.string()), gui::FileConverter::Format::CSV);
    });

    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.completed);
    ASSERT_TRUE(outcome.failed);
    EXPECT_EQ(outcome.failure, QStringLiteral("Cannot open CSV output file."));
    EXPECT_FALSE(std::filesystem::exists(output));
}

TEST(FileConverter, RawCutPreservesExactInclusiveInterval) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const DecodedEventFile source = decode_event_file(source_fixture(), true);
    ASSERT_GE(source.events.size(), 4u);

    std::size_t start_index = source.events.size() / 4;
    while (start_index < source.events.size() &&
           source.events[start_index].t <= source.events.front().t) {
        ++start_index;
    }
    std::size_t end_index = source.events.size() * 3 / 4;
    while (end_index > start_index && source.events[end_index].t >= source.events.back().t) {
        --end_index;
    }
    ASSERT_LT(start_index, end_index);
    const Metavision::timestamp start_us = source.events[start_index].t;
    const Metavision::timestamp end_us = source.events[end_index].t;
    ASSERT_GT(start_us, source.events.front().t);
    ASSERT_LT(end_us, source.events.back().t);
    ASSERT_LT(start_us, end_us);
    const std::vector<Metavision::EventCD> expected =
        filter_inclusive(source.events, start_us, end_us);
    ASSERT_GT(expected.size(), source.events.size() / 4);

    const std::filesystem::path artifact_root = make_artifact_root("raw-interval");
    const std::filesystem::path output = artifact_root / "sparklers-interval.raw";
    const OperationOutcome outcome = run_file_converter([&](gui::FileConverter &converter) {
        converter.cut(QString::fromStdString(source_fixture().string()),
                      QString::fromStdString(output.string()), start_us, end_us);
    });

    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.failed) << outcome.failure.toStdString();
    ASSERT_TRUE(outcome.completed);
    ASSERT_TRUE(std::filesystem::is_regular_file(output));
    ASSERT_GT(std::filesystem::file_size(output), 0u);

    // The input uses FileConverter's default shifted timeline. Read the written
    // EVT2 payload without another reader-level shift before exact comparison.
    const DecodedEventFile clipped = decode_event_file(output, false);
    EXPECT_EQ(clipped.width, source.width);
    EXPECT_EQ(clipped.height, source.height);
    EXPECT_EQ(clipped.encoding, "EVT2");
    const EventComparison comparison = compare_event_sequences(expected, clipped.events);
    EXPECT_EQ(comparison.actual_count, expected.size());
    EXPECT_TRUE(comparison.first_mismatch.empty()) << comparison.first_mismatch;
    EXPECT_TRUE(std::all_of(clipped.events.begin(), clipped.events.end(),
                            [start_us](const Metavision::EventCD &event) {
                                return event.t >= start_us;
                            }));
    EXPECT_TRUE(std::all_of(clipped.events.begin(), clipped.events.end(),
                            [end_us](const Metavision::EventCD &event) {
                                return event.t <= end_us;
                            }));
}

TEST(FileConverter, RawCutStartAtZeroPreservesExactInterval) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const DecodedEventFile source = decode_event_file(source_fixture(), true);
    ASSERT_GE(source.events.size(), 3u);

    const Metavision::timestamp start_us = 0;
    const Metavision::timestamp end_us = source.events[source.events.size() / 2].t;
    ASSERT_GT(end_us, start_us);
    ASSERT_LT(end_us, source.events.back().t);
    const std::vector<Metavision::EventCD> expected =
        filter_inclusive(source.events, start_us, end_us);
    ASSERT_FALSE(expected.empty());

    const std::filesystem::path artifact_root = make_artifact_root("raw-zero-start");
    const std::filesystem::path output = artifact_root / "sparklers-zero-start.raw";
    const OperationOutcome outcome = run_file_converter([&](gui::FileConverter &converter) {
        converter.cut(QString::fromStdString(source_fixture().string()),
                      QString::fromStdString(output.string()), start_us, end_us);
    });

    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.failed) << outcome.failure.toStdString();
    ASSERT_TRUE(outcome.completed);
    ASSERT_TRUE(std::filesystem::is_regular_file(output));
    ASSERT_GT(std::filesystem::file_size(output), 0u);

    const DecodedEventFile clipped = decode_event_file(output, false);
    const EventComparison comparison = compare_event_sequences(expected, clipped.events);
    EXPECT_EQ(comparison.actual_count, expected.size());
    EXPECT_TRUE(comparison.first_mismatch.empty()) << comparison.first_mismatch;
    EXPECT_TRUE(std::all_of(clipped.events.begin(), clipped.events.end(),
                            [end_us](const Metavision::EventCD &event) {
                                return event.t <= end_us;
                            }));
}

TEST(FileConverter, RawCutMissingParentFailsWithoutCompleted) {
    ASSERT_TRUE(std::filesystem::is_regular_file(source_fixture()));
    const std::filesystem::path artifact_root = make_artifact_root("raw-missing-parent");
    const std::filesystem::path missing_parent = artifact_root / "missing-parent";
    const std::filesystem::path output = missing_parent / "output.raw";
    ASSERT_FALSE(std::filesystem::exists(missing_parent));

    const OperationOutcome outcome = run_file_converter([&](gui::FileConverter &converter) {
        converter.cut(QString::fromStdString(source_fixture().string()),
                      QString::fromStdString(output.string()), 0, 1);
    });

    ASSERT_FALSE(outcome.timed_out);
    ASSERT_FALSE(outcome.completed);
    ASSERT_TRUE(outcome.failed);
    EXPECT_FALSE(outcome.failure.isEmpty());
    EXPECT_FALSE(std::filesystem::exists(output));
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
