// gui/tests/test_config_manager.cpp — ConfigManager serialization tests
// (design §3.11.2).
//
// Tests the algorithm-parameter serialization round-trip
// (capture_algo_state / apply_algo_state / save_algo_params_to_file /
// load_algo_params_from_file). These paths only require an AlgoBridge (no live
// camera), so the test is fully hermetic. The camera-state capture()/apply()
// paths require HAL facilities and are out of scope for headless unit tests.

#include <gtest/gtest.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include "algo_bridge/algo_bridge.h"
#include "config/config_manager.h"

using gui::AlgoBridge;
using gui::ConfigManager;

#ifndef EBPLUS_GUI_TEST_ARTIFACT_DIR
#error "EBPLUS_GUI_TEST_ARTIFACT_DIR must be defined"
#endif

namespace {

std::string sanitize_component(const char* value) {
    std::string sanitized;
    for (const unsigned char ch : std::string(value)) {
        const bool is_alphanumeric =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9');
        sanitized.push_back(is_alphanumeric ? static_cast<char>(ch) : '_');
    }
    return sanitized;
}

std::filesystem::path current_test_artifact_path(const std::string& prefix) {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    if (!info) {
        throw std::logic_error("No active GoogleTest case");
    }

    const std::filesystem::path root(EBPLUS_GUI_TEST_ARTIFACT_DIR);
    std::filesystem::create_directories(root);
    const std::string filename =
        sanitize_component(prefix.c_str()) + "_" +
        sanitize_component(info->test_suite_name()) + "_" +
        sanitize_component(info->name()) + ".json";
    return root / filename;
}

class ScopedTestArtifact {
public:
    explicit ScopedTestArtifact(const std::string& prefix) :
        path_(current_test_artifact_path(prefix)) {
        std::filesystem::remove(path_);
    }

    ~ScopedTestArtifact() noexcept {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    QString path() const {
        return QString::fromStdString(path_.string());
    }

private:
    std::filesystem::path path_;
};

} // namespace

TEST(ConfigManagerAlgoState, CaptureCoversAllRegisteredAlgos) {
    AlgoBridge bridge;
    ConfigManager cm;
    const auto obj = cm.capture_algo_state(&bridge);
    EXPECT_EQ(obj.value("format").toString(), "GUI-for-openEB-algo-params");
    EXPECT_EQ(obj.value("version").toInt(), 1);

    const auto algos = obj.value("algorithms").toObject();
    const auto registered = bridge.list_algos();
    EXPECT_EQ(static_cast<std::size_t>(algos.size()), registered.size());
    for (const auto& info : registered) {
        EXPECT_TRUE(algos.contains(QString::fromStdString(info.name)))
            << "missing entry for " << info.name;
    }
}

TEST(ConfigManagerAlgoState, RoundTripAlgoParams) {
    ScopedTestArtifact artifact("config");
    const QString path = artifact.path();

    AlgoBridge bridge;
    ConfigManager cm;

    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("n_sigma", "5.5");
    inst->set_param("learning_window_s", "10.0");
    inst->set_enabled(true);

    QString err;
    ASSERT_TRUE(cm.save_algo_params_to_file(&bridge, path, err))
        << err.toStdString();
    ASSERT_TRUE(QFile::exists(path));

    // Inspect the serialized JSON before reloading.
    {
        QFile f(path);
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(f.readAll());
        ASSERT_TRUE(doc.isObject());
        const auto root = doc.object();
        EXPECT_EQ(root.value("format").toString(), "GUI-for-openEB-algo-params");
        const auto algos = root.value("algorithms").toObject();
        ASSERT_TRUE(algos.contains("hot_pixel_filter"));
        const auto entry = algos.value("hot_pixel_filter").toObject();
        const auto params = entry.value("params").toObject();
        EXPECT_EQ(params.value("n_sigma").toString(), "5.5");
        EXPECT_EQ(params.value("learning_window_s").toString(), "10.0");
        EXPECT_TRUE(entry.value("enabled").toBool());
    }

    // Load into a fresh bridge that already has a live instance — apply only
    // touches live instances (per the ConfigManager contract).
    AlgoBridge bridge2;
    auto inst2 = bridge2.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst2, nullptr);
    EXPECT_NE(inst2->get_param("n_sigma"), "5.5");  // default before load

    ConfigManager cm2;
    QString err2;
    ASSERT_TRUE(cm2.load_algo_params_from_file(&bridge2, path, err2))
        << err2.toStdString();

    EXPECT_EQ(inst2->get_param("n_sigma"), "5.5");
    EXPECT_EQ(inst2->get_param("learning_window_s"), "10.0");
    EXPECT_TRUE(inst2->is_enabled());
}

TEST(ConfigManagerAlgoState, LoadNonexistentFileFails) {
    AlgoBridge bridge;
    ConfigManager cm;
    QString err;
    EXPECT_FALSE(cm.load_algo_params_from_file(
        &bridge, QStringLiteral("/no/such/path/here.json"), err));
    EXPECT_FALSE(err.isEmpty());
}

TEST(ConfigManagerAlgoState, RejectsWrongFormat) {
    AlgoBridge bridge;
    bridge.find_or_create("hot_pixel_filter");
    ConfigManager cm;
    QString err;
    QJsonObject bad;
    bad["format"] = QStringLiteral("something-else");
    bad["algorithms"] = QJsonObject{};
    EXPECT_FALSE(cm.apply_algo_state(&bridge, bad, err));
    EXPECT_FALSE(err.isEmpty());
}

TEST(ConfigManagerAlgoState, ApplyStateCachesParamsForNonLiveAlgos) {
    // A fresh bridge with no live instances: apply_algo_state should succeed
    // and cache the params (N1). When the instance is later created via
    // find_or_create, the cached params are replayed so they are not lost.
    AlgoBridge bridge;
    ConfigManager cm;

    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject algos;
    QJsonObject entry;
    QJsonObject params;
    params["n_sigma"] = QStringLiteral("9.9");
    entry["params"] = params;
    entry["enabled"] = true;
    algos["hot_pixel_filter"] = entry;
    obj["algorithms"] = algos;

    QString err;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // Create the instance — cached params should be replayed by create().
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("n_sigma"), "9.9");

    // Applying again on the now-live instance should also work.
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));
    EXPECT_EQ(inst->get_param("n_sigma"), "9.9");
}
