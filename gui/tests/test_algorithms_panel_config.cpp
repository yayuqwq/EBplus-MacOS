// gui/tests/test_algorithms_panel_config.cpp -- passive config-load UI tests.

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QWidget>

#include <string>

#include "algo_bridge/algo_bridge.h"
#include "config/config_manager.h"
#include "panels/algorithms_panel.h"

namespace {

QJsonObject algo_config(const QJsonObject& entry) {
    return {{"format", QStringLiteral("GUI-for-openEB-algo-params")},
            {"version", 1},
            {"algorithms", QJsonObject{{"hot_pixel_filter", entry}}}};
}

QDoubleSpinBox* double_field(QWidget* host, const QString& label_text) {
    auto* form = qobject_cast<QFormLayout*>(host->layout());
    if (!form) return nullptr;
    for (int row = 0; row < form->rowCount(); ++row) {
        auto* label_item = form->itemAt(row, QFormLayout::LabelRole);
        auto* field_item = form->itemAt(row, QFormLayout::FieldRole);
        auto* label = label_item ? qobject_cast<QLabel*>(label_item->widget()) : nullptr;
        if (label && label->text() == label_text) {
            return field_item ? qobject_cast<QDoubleSpinBox*>(field_item->widget()) : nullptr;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(AlgorithmsPanelConfig, PassiveRefreshShowsLazyEnabledStateWithoutSignals) {
    gui::AlgoBridge bridge;
    gui::AlgorithmsPanel panel(&bridge);
    gui::ConfigManager manager;
    int toggled_count = 0;
    QObject::connect(&panel, &gui::AlgorithmsPanel::algorithm_toggled,
                     [&toggled_count](const QString&, bool) { ++toggled_count; });

    const QJsonObject entry{
        {"enabled", true},
        {"params", QJsonObject{{"fpn_target_rate_hz", QStringLiteral("654")}}},
    };
    QString error;
    ASSERT_TRUE(manager.apply_algo_state(&bridge, algo_config(entry), error))
        << error.toStdString();
    ASSERT_EQ(bridge.find_live("hot_pixel_filter"), nullptr);

    panel.refresh_config_state();

    EXPECT_TRUE(panel.is_algo_enabled("hot_pixel_filter"));
    auto* checkbox = panel.findChild<QCheckBox*>(QStringLiteral("algorithm_hot_pixel_filter"));
    ASSERT_NE(checkbox, nullptr);
    EXPECT_TRUE(checkbox->isChecked());

    auto* params_host = panel.findChild<QWidget*>(
        QStringLiteral("algorithm_params_hot_pixel_filter"));
    ASSERT_NE(params_host, nullptr);
    // The panel itself is not shown by this offscreen test; isHidden() checks
    // the host's explicit shown state independently of its ancestor visibility.
    EXPECT_FALSE(params_host->isHidden());

    auto* fpn = double_field(params_host, QStringLiteral("FPN target rate (Hz)"));
    ASSERT_NE(fpn, nullptr);
    EXPECT_DOUBLE_EQ(fpn->value(), 654.0);

    EXPECT_EQ(toggled_count, 0);
    EXPECT_EQ(bridge.find_live("hot_pixel_filter"), nullptr);

    auto instance = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(instance, nullptr);
    EXPECT_TRUE(instance->is_enabled());
    EXPECT_EQ(instance->get_param("fpn_target_rate_hz"), "654");
}

TEST(AlgorithmsPanelConfig, PassiveRefreshShowsDisabledStateWithoutSignals) {
    gui::AlgoBridge bridge;
    gui::AlgorithmsPanel panel(&bridge);
    gui::ConfigManager manager;
    auto instance = bridge.find_or_create("hot_pixel_filter");
    ASSERT_NE(instance, nullptr);
    ASSERT_TRUE(bridge.set_algo_enabled("hot_pixel_filter", true));

    int toggled_count = 0;
    QObject::connect(&panel, &gui::AlgorithmsPanel::algorithm_toggled,
                     [&toggled_count](const QString&, bool) { ++toggled_count; });

    const QJsonObject entry{{"enabled", false}, {"params", QJsonObject{}}};
    QString error;
    ASSERT_TRUE(manager.apply_algo_state(&bridge, algo_config(entry), error))
        << error.toStdString();
    panel.refresh_config_state();

    EXPECT_FALSE(panel.is_algo_enabled("hot_pixel_filter"));
    EXPECT_FALSE(instance->is_enabled());
    EXPECT_EQ(toggled_count, 0);
}

TEST(AlgorithmsPanelConfig, GlobalPreprocRefreshReturnsOnlyCatalogValuesWithoutSignals) {
    gui::AlgoBridge bridge;
    gui::AlgorithmsPanel panel(&bridge);
    bridge.apply_global_preproc("preproc_filter_enabled", "true");
    bridge.apply_global_preproc("preproc_filter_mode", "3");
    bridge.apply_global_preproc("preproc_undistort_enabled", "true");

    int display_change_count = 0;
    QObject::connect(&panel, &gui::AlgorithmsPanel::preproc_display_param_changed,
                     [&display_change_count](const QString&, const QString&) {
                         ++display_change_count;
                     });

    const auto values = panel.refresh_global_preproc_values();

    EXPECT_EQ(values.at("preproc_filter_enabled"), "true");
    EXPECT_EQ(values.at("preproc_filter_mode"), "3");
    EXPECT_EQ(values.count("preproc_undistort_enabled"), 0u);
    EXPECT_EQ(display_change_count, 0);
}
