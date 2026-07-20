#include <metavision/sdk/base/events/event_cd.h>
#include <metavision/sdk/stream/camera.h>
#include <metavision/sdk/stream/camera_exception.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr std::uint64_t kDefaultDurationMs = 3000;
constexpr std::uint64_t kMinimumDurationMs = 100;
constexpr std::uint64_t kMaximumDurationMs = 5000;
constexpr std::uint64_t kDefaultMinimumEvents = 1;

struct Options {
    std::string serial;
    std::uint64_t duration_ms = kDefaultDurationMs;
    std::uint64_t min_events  = kDefaultMinimumEvents;
};

struct SharedStats {
    std::mutex mutex;
    std::uint64_t callbacks = 0;
    std::uint64_t events    = 0;
    std::optional<Metavision::timestamp> first_timestamp;
    std::optional<Metavision::timestamp> last_timestamp;
    std::atomic<bool> runtime_error{false};
};

struct Result {
    std::string status;
    std::uint64_t duration_requested_ms = 0;
    std::uint64_t duration_observed_ms  = 0;
    std::uint64_t callbacks             = 0;
    std::uint64_t events                = 0;
    std::optional<Metavision::timestamp> first_timestamp;
    std::optional<Metavision::timestamp> last_timestamp;
    bool runtime_error = false;
    bool started       = false;
    bool stopped       = false;
};

void print_usage(std::ostream &stream) {
    stream << "Usage: centuryarks_live_event_smoke --serial <full-runtime-selector> "
              "[--duration-ms <100..5000>] [--min-events <N>]\n";
}

bool parse_unsigned(std::string_view text, std::uint64_t &value) {
    if (text.empty()) {
        return false;
    }

    const char *begin = text.data();
    const char *end   = text.data() + text.size();
    const auto parsed = std::from_chars(begin, end, value);
    return parsed.ec == std::errc() && parsed.ptr == end;
}

bool is_full_runtime_selector(const std::string &selector) {
    const auto first_separator = selector.find(':');
    if (first_separator == std::string::npos || first_separator == 0) {
        return false;
    }

    const auto second_separator = selector.find(':', first_separator + 1);
    return second_separator != std::string::npos && second_separator > first_separator + 1 &&
           second_separator + 1 < selector.size() && selector.find(':', second_separator + 1) == std::string::npos;
}

void report_camera_error(const char *context, const Metavision::CameraException &exception) {
    std::cerr << context << " (error code " << exception.code().value() << ").\n";
}

std::optional<Options> parse_options(int argc, char **argv, bool &help_requested) {
    Options options;
    bool serial_seen   = false;
    bool duration_seen = false;
    bool minimum_seen  = false;
    help_requested     = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);

        if (argument == "--help") {
            help_requested = true;
            continue;
        }

        if (argument == "--serial") {
            if (serial_seen || index + 1 >= argc) {
                std::cerr << "Invalid or duplicate --serial argument.\n";
                return std::nullopt;
            }
            options.serial = argv[++index];
            serial_seen    = true;
            continue;
        }

        if (argument == "--duration-ms") {
            if (duration_seen || index + 1 >= argc ||
                !parse_unsigned(argv[++index], options.duration_ms)) {
                std::cerr << "Invalid or duplicate --duration-ms argument.\n";
                return std::nullopt;
            }
            duration_seen = true;
            continue;
        }

        if (argument == "--min-events") {
            if (minimum_seen || index + 1 >= argc || !parse_unsigned(argv[++index], options.min_events)) {
                std::cerr << "Invalid or duplicate --min-events argument.\n";
                return std::nullopt;
            }
            minimum_seen = true;
            continue;
        }

        std::cerr << "Unknown argument.\n";
        return std::nullopt;
    }

    if (help_requested) {
        return options;
    }

    if (!serial_seen || !is_full_runtime_selector(options.serial)) {
        std::cerr << "--serial must contain a non-empty full runtime selector.\n";
        return std::nullopt;
    }

    if (options.duration_ms < kMinimumDurationMs || options.duration_ms > kMaximumDurationMs) {
        std::cerr << "--duration-ms must be between 100 and 5000.\n";
        return std::nullopt;
    }

    return options;
}

void print_optional_timestamp(const std::optional<Metavision::timestamp> &timestamp) {
    if (timestamp) {
        std::cout << *timestamp;
    } else {
        std::cout << "null";
    }
}

void print_result(const Result &result) {
    std::cout << '{'
              << "\"status\":\"" << result.status << "\","
              << "\"duration_requested_ms\":" << result.duration_requested_ms << ','
              << "\"duration_observed_ms\":" << result.duration_observed_ms << ','
              << "\"callbacks\":" << result.callbacks << ','
              << "\"events\":" << result.events << ','
              << "\"first_timestamp_us\":";
    print_optional_timestamp(result.first_timestamp);
    std::cout << ",\"last_timestamp_us\":";
    print_optional_timestamp(result.last_timestamp);
    std::cout << ",\"timestamp_span_us\":";
    if (result.first_timestamp && result.last_timestamp) {
        std::cout << (*result.last_timestamp - *result.first_timestamp);
    } else {
        std::cout << "null";
    }
    std::cout << ",\"runtime_error\":" << (result.runtime_error ? "true" : "false")
              << ",\"started\":" << (result.started ? "true" : "false")
              << ",\"stopped\":" << (result.stopped ? "true" : "false") << "}\n";
}

void snapshot_stats(const std::shared_ptr<SharedStats> &stats, Result &result) {
    std::lock_guard<std::mutex> lock(stats->mutex);
    result.callbacks       = stats->callbacks;
    result.events          = stats->events;
    result.first_timestamp = stats->first_timestamp;
    result.last_timestamp  = stats->last_timestamp;
    result.runtime_error   = stats->runtime_error.load(std::memory_order_acquire);
}

int run_smoke(const Options &options) {
    Result result;
    result.duration_requested_ms = options.duration_ms;

    auto stats = std::make_shared<SharedStats>();
    std::optional<Metavision::Camera> camera;
    std::optional<Metavision::CallbackId> cd_callback_id;
    std::optional<Metavision::CallbackId> error_callback_id;
    std::optional<std::chrono::steady_clock::time_point> started_at;
    bool cleanup_failure = false;
    bool stop_attempted   = false;
    int primary_exit_code = 0;

    try {
        camera.emplace(Metavision::Camera::from_serial(options.serial));

        cd_callback_id = camera->cd().add_callback(
            [stats](const Metavision::EventCD *begin, const Metavision::EventCD *end) {
                std::lock_guard<std::mutex> lock(stats->mutex);
                ++stats->callbacks;

                if (begin == nullptr || end == nullptr || begin >= end) {
                    return;
                }

                stats->events += static_cast<std::uint64_t>(end - begin);
                const auto batch_first = begin->t;
                const auto batch_last  = (end - 1)->t;

                if (!stats->first_timestamp || batch_first < *stats->first_timestamp) {
                    stats->first_timestamp = batch_first;
                }
                if (!stats->last_timestamp || batch_last > *stats->last_timestamp) {
                    stats->last_timestamp = batch_last;
                }
            });

        error_callback_id = camera->add_runtime_error_callback(
            [stats](const Metavision::CameraException &) {
                stats->runtime_error.store(true, std::memory_order_release);
            });

        if (!camera->start()) {
            std::cerr << "Camera start returned false.\n";
            primary_exit_code = 5;
        } else {
            result.started = true;
            started_at     = std::chrono::steady_clock::now();
            const auto deadline = *started_at + std::chrono::milliseconds(options.duration_ms);

            while (std::chrono::steady_clock::now() < deadline &&
                   !stats->runtime_error.load(std::memory_order_acquire) && camera->is_running()) {
                const auto next_wakeup =
                    std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(10));
                std::this_thread::sleep_until(next_wakeup);
            }

            try {
                stop_attempted = true;
                result.stopped = camera->stop();
                if (!result.stopped && !stats->runtime_error.load(std::memory_order_acquire)) {
                    std::cerr << "Camera stop returned false.\n";
                    cleanup_failure = true;
                }
            } catch (const Metavision::CameraException &exception) {
                report_camera_error("Camera stop failed", exception);
                cleanup_failure = true;
            }

            const auto stopped_at = std::chrono::steady_clock::now();
            result.duration_observed_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(stopped_at - *started_at).count());
        }
    } catch (const Metavision::CameraException &exception) {
        report_camera_error("Camera operation failed", exception);
        primary_exit_code = result.started ? 6 : 5;
        cleanup_failure   = result.started;
    } catch (const std::exception &) {
        std::cerr << "Unexpected camera operation failure.\n";
        primary_exit_code = result.started ? 6 : 5;
        cleanup_failure   = result.started;
    }

    if (camera && result.started && !stop_attempted) {
        try {
            stop_attempted = true;
            result.stopped = camera->stop();
            if (!result.stopped && !stats->runtime_error.load(std::memory_order_acquire)) {
                std::cerr << "Emergency camera stop returned false.\n";
                cleanup_failure = true;
            }
        } catch (const Metavision::CameraException &exception) {
            report_camera_error("Emergency camera stop failed", exception);
            cleanup_failure = true;
        }

        if (started_at) {
            const auto stopped_at = std::chrono::steady_clock::now();
            result.duration_observed_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(stopped_at - *started_at).count());
        }
    }

    if (camera) {
        if (cd_callback_id) {
            try {
                if (!camera->cd().remove_callback(*cd_callback_id)) {
                    std::cerr << "CD callback removal returned false.\n";
                    cleanup_failure = true;
                }
            } catch (const Metavision::CameraException &exception) {
                report_camera_error("CD callback removal failed", exception);
                cleanup_failure = true;
            }
        }

        if (error_callback_id) {
            try {
                if (!camera->remove_runtime_error_callback(*error_callback_id)) {
                    std::cerr << "Runtime error callback removal returned false.\n";
                    cleanup_failure = true;
                }
            } catch (const Metavision::CameraException &exception) {
                report_camera_error("Runtime error callback removal failed", exception);
                cleanup_failure = true;
            }
        }
    }

    snapshot_stats(stats, result);

    int exit_code = 0;
    if (primary_exit_code == 5) {
        result.status = "open_or_start_failed";
        exit_code     = primary_exit_code;
    } else if (primary_exit_code == 6 || cleanup_failure) {
        result.status = "cleanup_failed";
        exit_code     = 6;
    } else if (result.runtime_error) {
        result.status = "runtime_error";
        exit_code     = 3;
    } else if (result.events < options.min_events) {
        result.status = "insufficient_events";
        exit_code     = 2;
    } else {
        result.status = "passed";
    }

    print_result(result);
    return exit_code;
}

} // namespace

int main(int argc, char **argv) {
    bool help_requested = false;
    const auto options  = parse_options(argc, argv, help_requested);
    if (!options) {
        print_usage(std::cerr);
        return 4;
    }

    if (help_requested) {
        print_usage(std::cout);
        return 0;
    }

    return run_smoke(*options);
}
