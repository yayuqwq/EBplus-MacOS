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
#include <QJsonValue>
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
        const auto key = QString::fromStdString(info.name);
        EXPECT_TRUE(algos.contains(key))
            << "missing entry for " << info.name;
        const auto entry = algos.value(key).toObject();
        EXPECT_EQ(entry.value("category").toString(),
                  QString::fromStdString(info.category));
        EXPECT_TRUE(entry.contains("params"));
        if (info.source == "self") EXPECT_TRUE(entry.contains("enabled"));
        else EXPECT_FALSE(entry.contains("enabled"));
    }
}

TEST(ConfigManagerAlgoState, RoundTripAlgoParams) {
    ScopedTestArtifact artifact("config");
    const QString path = artifact.path();

    AlgoBridge bridge;
    ConfigManager cm;

    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("fpn_target_rate_hz", "250");
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
        EXPECT_EQ(params.value("fpn_target_rate_hz").toString(), "250");
        EXPECT_EQ(params.value("learning_window_s").toString(), "10.0");
        EXPECT_TRUE(entry.value("enabled").toBool());
    }

    // Load into a fresh bridge that already has a live instance — apply only
    // touches live instances (per the ConfigManager contract).
    AlgoBridge bridge2;
    auto inst2 = bridge2.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst2, nullptr);
    EXPECT_NE(inst2->get_param("fpn_target_rate_hz"), "250");  // default before load

    ConfigManager cm2;
    QString err2;
    ASSERT_TRUE(cm2.load_algo_params_from_file(&bridge2, path, err2))
        << err2.toStdString();

    EXPECT_EQ(inst2->get_param("fpn_target_rate_hz"), "250");
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
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("fpn_target_rate_hz", "321");
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));
    ConfigManager cm;
    QString err;
    QJsonObject bad;
    bad["format"] = QStringLiteral("something-else");
    bad["algorithms"] = QJsonObject{};
    EXPECT_FALSE(cm.apply_algo_state(&bridge, bad, err));
    EXPECT_FALSE(err.isEmpty());
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "321");
    EXPECT_TRUE(bridge.algo_enabled("hot_pixel_filter").value_or(false));
}

TEST(ConfigManagerAlgoState, MissingVersionIsAcceptedAsLegacyV1) {
    AlgoBridge bridge;
    ConfigManager cm;
    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject entry;
    entry["category"] = QStringLiteral("renamed-category-is-advisory");
    entry["enabled"] = true;
    entry["params"] = QJsonObject{{"fpn_target_rate_hz", QStringLiteral("777")}};
    obj["algorithms"] = QJsonObject{{"hot_pixel_filter", entry}};

    QString err;
    bool accepted = false;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err, nullptr, &accepted));
    EXPECT_TRUE(accepted);
    EXPECT_EQ(bridge.find_live("hot_pixel_filter"), nullptr);
    EXPECT_TRUE(bridge.desired_algo_enabled("hot_pixel_filter").value_or(false));

    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "777");
    EXPECT_TRUE(inst->is_enabled());
}

TEST(ConfigManagerAlgoState, RejectsUnsupportedOrMalformedVersionWithoutMutation) {
    ConfigManager cm;
    for (const QJsonValue& version : {QJsonValue(2), QJsonValue(QStringLiteral("1")),
                                      QJsonValue(1.5)}) {
        AlgoBridge bridge;
        auto inst = bridge.find_or_create("hot_pixel_filter");
        ASSERT_NE(inst, nullptr);
        inst->set_param("fpn_target_rate_hz", "321");
        ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));

        QJsonObject entry;
        entry["enabled"] = false;
        entry["params"] = QJsonObject{{"fpn_target_rate_hz", QStringLiteral("999")}};
        QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                        {"version", version},
                        {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
        QString err;
        bool accepted = true;
        EXPECT_FALSE(cm.apply_algo_state(&bridge, obj, err, nullptr, &accepted));
        EXPECT_FALSE(accepted);
        EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "321");
        EXPECT_TRUE(bridge.algo_enabled("hot_pixel_filter").value_or(false));
    }
}

TEST(ConfigManagerAlgoState, UnknownAlgorithmAppliesKnownEntriesWithWarning) {
    AlgoBridge bridge;
    ConfigManager cm;
    QJsonObject known;
    known["enabled"] = true;
    known["params"] = QJsonObject{{"fpn_target_rate_hz", QStringLiteral("444")}};
    QJsonObject obsolete;
    obsolete["params"] = QJsonObject{};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"obsolete_algorithm", obsolete},
                                                {"hot_pixel_filter", known}}}};
    QString err;
    bool accepted = false;
    EXPECT_FALSE(cm.apply_algo_state(&bridge, obj, err, nullptr, &accepted));
    EXPECT_TRUE(accepted);
    EXPECT_TRUE(err.contains("obsolete_algorithm"));
    EXPECT_EQ(bridge.find_live("hot_pixel_filter"), nullptr);
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "444");
    EXPECT_TRUE(inst->is_enabled());
}

TEST(ConfigManagerAlgoState, RejectsMalformedUnknownEntryWithoutMutation) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("fpn_target_rate_hz", "321");
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));
    ConfigManager cm;

    QJsonObject known{{"enabled", false},
                      {"params", QJsonObject{{"fpn_target_rate_hz", "999"}}}};
    QJsonObject malformed_unknown{{"params", QJsonObject{{"obsolete_parameter", 1}}}};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"hot_pixel_filter", known},
                                                {"obsolete_algorithm", malformed_unknown}}}};
    QString err;
    bool accepted = true;
    EXPECT_FALSE(cm.apply_algo_state(&bridge, obj, err, nullptr, &accepted));
    EXPECT_FALSE(accepted);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "321");
    EXPECT_TRUE(bridge.algo_enabled("hot_pixel_filter").value_or(false));
}

TEST(ConfigManagerAlgoState, RejectsMultipleSelfEnabledWithoutMutation) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("fpn_target_rate_hz", "321");
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));
    ConfigManager cm;
    QJsonObject first{{"enabled", true}, {"params", QJsonObject{{"fpn_target_rate_hz", "999"}}}};
    QJsonObject second{{"enabled", true}, {"params", QJsonObject{}}};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"hot_pixel_filter", first},
                                                {"background_mask", second}}}};
    QString err;
    EXPECT_FALSE(cm.apply_algo_state(&bridge, obj, err));
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "321");
    EXPECT_TRUE(bridge.algo_enabled("hot_pixel_filter").value_or(false));
}

TEST(ConfigManagerAlgoState, UnknownParameterIsNotCapturedAgain) {
    AlgoBridge bridge;
    ConfigManager cm;
    QJsonObject entry;
    entry["params"] = QJsonObject{{"fpn_target_rate_hz", "555"},
                                    {"obsolete_parameter", "old"}};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&bridge, obj, err));
    const auto captured = cm.capture_algo_state(&bridge)
                              .value("algorithms").toObject()
                              .value("hot_pixel_filter").toObject()
                              .value("params").toObject();
    EXPECT_EQ(captured.value("fpn_target_rate_hz").toString(), "555");
    EXPECT_FALSE(captured.contains("obsolete_parameter"));
}

TEST(ConfigManagerAlgoState, CanonicalParamWinsOverRenamedLegacyKey) {
    AlgoBridge bridge;
    ConfigManager cm;
    QJsonObject entry;
    entry["params"] = QJsonObject{{"learning_rate", "0.2"},
                                    {"learning_window_s", "7.0"}};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"background_mask", entry}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&bridge, obj, err));
    auto inst = bridge.find_or_create("background_mask");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("learning_window_s"), "7.0");
}

TEST(ConfigManagerAlgoState, RejectsMalformedKnownContentWithoutMutation) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    inst->set_param("fpn_target_rate_hz", "321");
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));
    ConfigManager cm;
    const QJsonObject entry{{"enabled", QStringLiteral("true")},
                            {"params", QJsonObject{{"fpn_target_rate_hz", 999}}}};
    const QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                          {"version", 1},
                          {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
    QString err;
    bool accepted = true;
    EXPECT_FALSE(cm.apply_algo_state(&bridge, obj, err, nullptr, &accepted));
    EXPECT_FALSE(accepted);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "321");
    EXPECT_TRUE(bridge.algo_enabled("hot_pixel_filter").value_or(false));
}

TEST(ConfigManagerAlgoState, CategoryIsAdvisoryForKnownAlgorithmParameters) {
    AlgoBridge bridge;
    ConfigManager cm;
    QJsonObject entry{{"category", QStringLiteral("obsolete-category")},
                      {"params", QJsonObject{{"fpn_target_rate_hz", "456"}}}};
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"version", 1},
                    {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "456");
}

TEST(ConfigManagerAlgoState, SharedPreprocRoundTripsThroughLazyCatalog) {
    AlgoBridge source;
    ConfigManager cm;
    QJsonObject hot{{"params", QJsonObject{{"preproc_filter_enabled", "true"},
                                            {"preproc_filter_mode", "3"}}}};
    QJsonObject background{{"params", QJsonObject{{"preproc_filter_enabled", "true"},
                                                   {"preproc_filter_mode", "3"}}}};
    QJsonObject input{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                      {"version", 1},
                      {"algorithms", QJsonObject{{"hot_pixel_filter", hot},
                                                  {"background_mask", background}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&source, input, err));
    EXPECT_EQ(source.get_global_preproc_param("preproc_filter_enabled"),
              std::optional<std::string>("true"));
    EXPECT_EQ(source.get_global_preproc_param("preproc_filter_mode"),
              std::optional<std::string>("3"));

    const auto captured = cm.capture_algo_state(&source);
    const auto params = captured.value("algorithms").toObject()
                            .value("hot_pixel_filter").toObject()
                            .value("params").toObject();
    EXPECT_EQ(params.value("preproc_filter_enabled").toString(), "true");
    EXPECT_EQ(params.value("preproc_filter_mode").toString(), "3");

    AlgoBridge restored;
    ASSERT_TRUE(cm.apply_algo_state(&restored, captured, err));
    auto inst = restored.find_or_create("background_mask");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("preproc_filter_enabled"), "true");
    EXPECT_EQ(inst->get_param("preproc_filter_mode"), "3");
}

TEST(ConfigManagerAlgoState, RejectsConflictingSharedPreprocWithoutMutation) {
    AlgoBridge bridge;
    bridge.apply_global_preproc("preproc_filter_enabled", "false");
    ConfigManager cm;
    QJsonObject hot{{"params", QJsonObject{{"preproc_filter_enabled", "true"}}}};
    QJsonObject background{{"params", QJsonObject{{"preproc_filter_enabled", "false"}}}};
    QJsonObject input{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                      {"version", 1},
                      {"algorithms", QJsonObject{{"hot_pixel_filter", hot},
                                                  {"background_mask", background}}}};
    QString err;
    bool accepted = true;
    EXPECT_FALSE(cm.apply_algo_state(&bridge, input, err, nullptr, &accepted));
    EXPECT_FALSE(accepted);
    EXPECT_EQ(bridge.get_global_preproc_param("preproc_filter_enabled"),
              std::optional<std::string>("false"));
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
    params["fpn_target_rate_hz"] = QStringLiteral("999");
    entry["params"] = params;
    entry["enabled"] = true;
    algos["hot_pixel_filter"] = entry;
    obj["algorithms"] = algos;

    QString err;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // Create the instance — cached params should be replayed by create().
    auto inst = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "999");
    EXPECT_TRUE(inst->is_enabled());

    // Applying again on the now-live instance should also work.
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "999");
}

TEST(ConfigManagerAlgoState, SparseLegacyEntryPreservesDesiredEnabledState) {
    AlgoBridge bridge;
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));
    ConfigManager cm;
    QJsonObject obj{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                    {"algorithms", QJsonObject{{"hot_pixel_filter", QJsonObject{}}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&bridge, obj, err));
    EXPECT_TRUE(bridge.desired_algo_enabled("hot_pixel_filter").value_or(false));
}

TEST(ConfigManagerAlgoState, LazyDesiredStateSurvivesCaptureAndFreshApply) {
    AlgoBridge source;
    ConfigManager cm;
    QJsonObject entry{{"enabled", true},
                      {"params", QJsonObject{{"fpn_target_rate_hz", "888"}}}};
    QJsonObject input{{"format", QStringLiteral("GUI-for-openEB-algo-params")},
                      {"version", 1},
                      {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
    QString err;
    ASSERT_TRUE(cm.apply_algo_state(&source, input, err));
    ASSERT_EQ(source.find_live("hot_pixel_filter"), nullptr);
    const auto captured = cm.capture_algo_state(&source);
    const auto captured_entry = captured.value("algorithms").toObject()
                                    .value("hot_pixel_filter").toObject();
    EXPECT_TRUE(captured_entry.value("enabled").toBool());
    EXPECT_EQ(captured_entry.value("params").toObject()
                  .value("fpn_target_rate_hz").toString(), "888");

    AlgoBridge restored;
    ASSERT_TRUE(cm.apply_algo_state(&restored, captured, err));
    ASSERT_EQ(restored.find_live("hot_pixel_filter"), nullptr);
    auto inst = restored.find_or_create("hot_pixel_filter");
    ASSERT_NE(inst, nullptr);
    EXPECT_TRUE(inst->is_enabled());
    EXPECT_EQ(inst->get_param("fpn_target_rate_hz"), "888");
}

// §11.2-G: a saved config that still uses the pre-rename key "learning_rate"
// for background_mask must be migrated to "learning_window_s" on load. Without
// the migration table the old key would be silently dropped (set_param only
// stores keys present in info->params) and background_mask would start with
// the default 5.0s instead of the user's saved value.
TEST(ConfigManagerAlgoState, MigratesRenamedParamLearningRateToLearningWindowS) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("background_mask");
    ASSERT_NE(inst, nullptr);
    // Sanity: the new key is recognised, the old key is not.
    ASSERT_FALSE(inst->info().params.empty());
    bool has_new = false, has_old = false;
    for (const auto& p : inst->info().params) {
        if (p.key == "learning_window_s") has_new = true;
        if (p.key == "learning_rate")     has_old = true;
    }
    ASSERT_TRUE(has_new) << "background_mask must register learning_window_s";
    ASSERT_FALSE(has_old) << "background_mask must NOT register learning_rate";

    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject algos;
    QJsonObject entry;
    QJsonObject params;
    // Old config file used the pre-rename key with a value (0.05) that was
    // typical of the old "rate" semantics.
    params["learning_rate"] = QStringLiteral("0.05");
    entry["params"] = params;
    algos["background_mask"] = entry;
    obj["algorithms"] = algos;

    QString err;
    ConfigManager cm;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // The value must be reachable under the NEW key. (0.05 is within the
    // registered [0.1, 60] range — actually below the min, so it gets clamped
    // UP to 0.1 by §11.2-F. This also exercises the clamp path.)
    const auto got = inst->get_param("learning_window_s");
    EXPECT_FALSE(got.empty()) << "value not migrated to learning_window_s";
    EXPECT_EQ(got, "0.100000") << "expected 0.05 clamped up to 0.1 (6 decimals)";
    // The old key must NOT be stored.
    EXPECT_TRUE(inst->get_param("learning_rate").empty());
}

// §11.2-G negative: blob_detector legitimately registers "learning_rate" as a
// rate (not a window). The migration table is scoped by algo name, so this
// key must NOT be renamed for blob_detector.
TEST(ConfigManagerAlgoState, DoesNotMigrateLearningRateForBlobDetector) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("blob_detector");
    ASSERT_NE(inst, nullptr);
    bool has_rate = false;
    for (const auto& p : inst->info().params) {
        if (p.key == "learning_rate") has_rate = true;
    }
    ASSERT_TRUE(has_rate) << "blob_detector must register learning_rate";

    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject algos;
    QJsonObject entry;
    QJsonObject params;
    params["learning_rate"] = QStringLiteral("0.05");
    entry["params"] = params;
    algos["blob_detector"] = entry;
    obj["algorithms"] = algos;

    QString err;
    ConfigManager cm;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // blob_detector keeps learning_rate=0.05 (within [0.001, 1.0] — no clamp).
    EXPECT_EQ(inst->get_param("learning_rate"), "0.05");
}

// §11.2-F: a saved config with an out-of-range numeric value must be clamped
// to the registered [min, max] so the stored param_values_ matches the algo's
// runtime value (the algo setter already clamps internally; without this the
// displayed value would diverge until the user touches the spinbox).
TEST(ConfigManagerAlgoState, ClampsOutOfRangeFloatParamToRegisteredRange) {
    AlgoBridge bridge;
    auto inst = bridge.find_or_create("background_mask");
    ASSERT_NE(inst, nullptr);
    // Find the registered range for learning_window_s to make the test
    // self-checking against future registry changes.
    std::string lo, hi;
    for (const auto& p : inst->info().params) {
        if (p.key == "learning_window_s") { lo = p.min_value; hi = p.max_value; }
    }
    ASSERT_FALSE(lo.empty()) << "learning_window_s must register a min";
    ASSERT_FALSE(hi.empty()) << "learning_window_s must register a max";

    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject algos;
    QJsonObject entry;
    QJsonObject params;
    // 9999.0 is far above the registered max (60.0) — typical of old configs
    // saved when the GUI spinbox allowed ±1e9.
    params["learning_window_s"] = QStringLiteral("9999.0");
    entry["params"] = params;
    algos["background_mask"] = entry;
    obj["algorithms"] = algos;

    QString err;
    ConfigManager cm;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // Stored value must be clamped to the registered max (6 decimals, matches
    // QDoubleSpinBox in algorithms_panel).
    const QString expected = QString::number(QString::fromStdString(hi).toDouble(), 'f', 6);
    EXPECT_EQ(inst->get_param("learning_window_s"), expected.toStdString());
}

// §11.2-F+G combined: migration + clamp on the cache-only path (no live
// instance yet). Ensures the migration is applied consistently for algos
// that are enabled later (N1 replay path).
TEST(ConfigManagerAlgoState, MigratesAndClampsOnCacheOnlyPath) {
    AlgoBridge bridge;
    // No find_or_create — leave background_mask non-live.

    QJsonObject obj;
    obj["format"] = QStringLiteral("GUI-for-openEB-algo-params");
    QJsonObject algos;
    QJsonObject entry;
    QJsonObject params;
    // Old key + out-of-range value: should be renamed AND clamped.
    params["learning_rate"] = QStringLiteral("9999.0");
    entry["params"] = params;
    algos["background_mask"] = entry;
    obj["algorithms"] = algos;

    QString err;
    ConfigManager cm;
    EXPECT_TRUE(cm.apply_algo_state(&bridge, obj, err));

    // Now create the instance — cached (migrated + clamped) params replay.
    auto inst = bridge.find_or_create("background_mask");
    ASSERT_NE(inst, nullptr);
    const QString expected = QString::number(60.0, 'f', 6);  // registered max
    EXPECT_EQ(inst->get_param("learning_window_s"), expected.toStdString());
    EXPECT_TRUE(inst->get_param("learning_rate").empty());
}
