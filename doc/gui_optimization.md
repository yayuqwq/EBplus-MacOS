# GUI 优化设计文档

> 版本：1.0
> 日期：2026-07-08
> 基于：[design.md](file:///home/justin/GUI-for-openEB/doc/design.md) v2.0
> 范围：`gui/` 目录的架构与视觉优化，不涉及 `algo/` 算法逻辑

---

## 一、概述

### 1.1 文档目的

本文档是 `design.md` 的补充，针对 GUI 层（`gui/`）的架构与视觉设计进行专项优化。
`design.md` 规定了功能需求与分层架构，本文档聚焦**如何更好地实现**这些需求：

- **架构层面**：消除 MainWindow 上帝类、拆分巨石文件、引入 Panel 基类与事件解耦
- **视觉层面**：建立设计令牌系统、引入图标、提升间距与微交互

本文档**不改变** `design.md` 已定义的功能需求、分层架构、数据流、算法桥接接口。
所有优化在 `design.md` §2.1 的 GUI Layer 内部进行。

### 1.2 设计原则

1. **保留既有架构契约**——`AlgoBridge` 注册表 + 工厂 + `weak_ptr` live instances（[algo_bridge.h:121-177](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_bridge.h#L121-L177)）、`FramePipeline` 线程边界、`AlgoParamSpec.mode_filter` 声明式参数、`AlgoInstance` 洪水保护（[design.md §5.6.7](file:///home/justin/GUI-for-openEB/doc/design.md)）全部保留
2. **保留粉彩主题**——本 GUI 是事件相机可视化工具，不是代码开发工具；彩色背景不干扰事件帧感知（事件帧在独立的 OpenGL 显示区，与窗口背景分离）。5 色 × 明暗共 10 个主题变体全部保留
3. **演进式重构**——不推倒重来，每个阶段可独立交付、独立验证、独立回滚
4. **不过度设计**——不引入 AppController 中介者（`design.md` §2.2 的设想，但实现已证明 MainWindow 直接持有 + 事件解耦更轻量）；不引入命令系统（当前规模不需要）

### 1.3 与 design.md 的关系

| design.md 章节 | 关系 | 说明 |
|----------------|------|------|
| §2.1 分层架构 | 不变 | 优化在 GUI Layer 内部 |
| §2.2 模块交互 | 偏离修正 | design.md 设想 AppController 中介者，实现中 MainWindow 兼任；方案用事件总线替代中介者，更轻量 |
| §3.7 ConfigManager | 不变 | 配置序列化范围不变 |
| §3.8 AlgoBridge | 不变 | 接口不变，仅拆分 algo_backend.cpp 实现 |
| §5.1 主窗口布局 | 回归 + 增强 | design.md 画的是垂直堆叠面板，实现变成了两个 tab；方案的可折叠分组回归 §5.1 原始设计 |
| §5.4 菜单栏 | 偏离修正 | design.md 列了 Algorithm 顶级菜单，实现已移除（[main_window.h:10-12](file:///home/justin/GUI-for-openEB/gui/main_window.h#L10-L12)）；方案不恢复已废弃菜单 |
| §5.6.1 显示模式分类 | 不变 | 四种模式（Passive/Overlay/Replace/Standalone）不变，实现从 switch 改多态 |
| §5.6.5 QDockWindow 机制 | 不变 | 多窗口基于 QDockWidget 不变 |
| §5.6.6 算法 ROI | 不变 | 全算法 ROI 支持不变 |
| §5.6.7 Flood Guard | 不变 | 事件涌入保护不变 |
| §7.1 性能需求 | 不变 | 令牌化/拆分/事件总线不引入运行时开销 |

**结论**：方案与 `design.md` 无矛盾，部分改进修正了实现偏离设计的问题。

---

## 二、现状分析

### 2.1 代码规模

| 文件 | 行数 | 问题 |
|------|------|------|
| [algo_backend.cpp](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_backend.cpp) | 2897 | 50 个 `*Backend` 子类挤在一个文件 |
| [main_window.cpp](file:///home/justin/GUI-for-openEB/gui/main_window.cpp) | 1538 | 上帝类，54 个 `connect()` |
| [algo_bridge.cpp](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_bridge.cpp) | 707 | 可接受 |
| [config_manager.cpp](file:///home/justin/GUI-for-openEB/gui/config/config_manager.cpp) | 499 | 可接受 |
| gui/ 总计 | 16324 | 零测试覆盖 |

### 2.2 架构问题

#### 问题 A：MainWindow 上帝类

[main_window.h:135-228](file:///home/justin/GUI-for-openEB/gui/main_window.h#L135-L228) 直接持有 13+ 个 controller/manager，[main_window.cpp:552-651](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L552-L651) 的 `wire_signals()` 手工编织 54 条信号槽。

相机连接成功后，MainWindow 要逐个调用 6 个 panel 的 `on_camera_connected()`：
```cpp
settings_->biases_panel()->on_camera_connected(&camera_);
settings_->roi_panel()->on_camera_connected(&camera_);
settings_->esp_panel()->on_camera_connected(&camera_);
settings_->trigger_panel()->on_camera_connected(&camera_);
settings_->preprocessing_panel()->on_camera_connected(&camera_);
```
每加一个 panel 就要改 MainWindow。

#### 问题 B：algo_backend.cpp 巨石文件

50 个 backend 子类（[algo_backend.cpp:241-2815](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_backend.cpp)）跨越 7 个类别全挤一个文件。改一个 backend 全文重编，无法独立测试。

#### 问题 C：Panel 无公共基类

12 个 Panel 全部 `public QWidget`，无 `AbstractPanel`。`on_camera_connected`/`on_camera_disconnected`/`info_message`/`error_message` 在多个 panel 重复声明。

#### 问题 D：process_algo_results 类型 switch

[main_window.cpp:1032-1121](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L1032-L1121) 用 `if (mode == Overlay)... else if (mode == Replace)...` 分支处理 4 种显示模式。增加新模式要改 MainWindow。

#### 问题 E：GUI 零测试

`gui/**/test*` 返回空。16324 行 GUI 代码无单元测试。

### 2.3 视觉问题

#### 问题 F：无设计令牌，裸 hex 散落

0 个 .qss 文件，所有样式内联。`color: #888` / `#444` 硬编码出现于 [roi_panel.cpp](file:///home/justin/GUI-for-openEB/gui/panels/roi_panel.cpp)、[biases_panel.cpp](file:///home/justin/GUI-for-openEB/gui/panels/biases_panel.cpp)、[trigger_panel.cpp](file:///home/justin/GUI-for-openEB/gui/panels/trigger_panel.cpp)、[esp_panel.cpp](file:///home/justin/GUI-for-openEB/gui/panels/esp_panel.cpp)、[settings_panel.cpp](file:///home/justin/GUI-for-openEB/gui/panels/settings_panel.cpp) 共 ~15 处。

`#888` 在暗色背景（如 `#1E2A3D`）上对比度仅 2.3:1，低于 WCAG AA 标准 4.5:1——提示文字在暗色模式下几乎不可见。

#### 问题 G：无图标资源系统

0 个 .qrc 文件，0 个自定义图标。全靠 `style()->standardIcon(QStyle::SP_*)`（[main_window.cpp:277](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L277)），跨平台风格不一致。

#### 问题 H：圆角与间距过小

`border-radius: 2px`（[theme_controller.cpp:237](file:///home/justin/GUI-for-openEB/gui/app/theme_controller.cpp#L237)）、`setSpacing(4)` + `setContentsMargins(4,4,4,4)`（[biases_panel.cpp:24,38](file:///home/justin/GUI-for-openEB/gui/panels/biases_panel.cpp#L24)），视觉拥挤。

#### 问题 I：QMenuBar 充当标题栏（hack）

[main_window.cpp:129-136](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L129-L136) 把窗口标题和控制按钮塞进 QMenuBar 角落，视觉层级混乱。

#### 问题 J：无字体系统

无全局 QFont 设置，依赖系统默认。

#### 问题 K：无微交互

样式表只有 `:hover` 背景 +6% 灰，无 `:focus` outline、无 `:pressed` 反馈、无过渡。

---

## 三、优化设计

### 3.1 视觉令牌系统

#### 3.1.1 设计目标

保留 5 色粉彩主题，但把每个颜色拆成**语义令牌表**，让暗色模式自动达标 WCAG AA 对比度。

#### 3.1.2 令牌定义

每个粉彩色 × 明暗模式预生成 9 个语义令牌：

| 令牌 | 用途 | Light Gray | Dark Gray | Light Blue | Dark Blue | ... |
|------|------|-----------|-----------|-----------|-----------|-----|
| `bg-primary` | 窗口背景 | #E8E8E8 | #2D2D30 | #D4E6F1 | #1E2A3D | ... |
| `bg-panel` | 面板/dock 背景 | #F0F0F0 | #252525 | #E0EEF7 | #243349 | ... |
| `bg-input` | 输入控件背景 | #FFFFFF | #3C3C3C | #FFFFFF | #2A3B52 | ... |
| `bg-hover` | 悬停态背景 | #DCDCDC | #3A3A3A | #C4DAEC | #2E405A | ... |
| `fg-primary` | 主文字 | #1A1A1A | #F0F0F0 | #14233A | #E8EEF5 | ... |
| `fg-secondary` | 次要文字 | #5A5A5A | #B0B0B0 | #3A5A7A | #A8BCD0 | ... |
| `fg-muted` | 提示/占位文字 | #6A6A6A | #9A9A9A | #5A7590 | #8898AC | ... |
| `accent` | 强调色（选中/焦点） | #0066B8 | #4A9FE0 | #005A9E | #5AA8E0 | ... |
| `border` | 控件边框 | #C0C0C0 | #555555 | #A8C4DC | #3A4D66 | ... |

关键改进：`fg-muted` 在暗色下用 `#9A9A9A`（对比度 4.6:1）替代硬编码 `#888`（2.3:1）。

#### 3.1.3 文件组织

```
gui/resources/theme/
  ├── tokens.h          // C++ 常量数组，索引 [color][mode][token]
  ├── base.qss.in       // QSS 模板，用 %token% 占位符
  └── (运行时由 ThemeController 注入实际颜色)
```

[theme_controller.cpp:231-250](file:///home/justin/GUI-for-openEB/gui/app/theme_controller.cpp#L231-L250) 的内联 qss 迁入 `base.qss.in`，`apply_stylesheet()` 读取模板并 `QString::replace()` 注入令牌值。

#### 3.1.4 清除裸 hex

所有 panel 中的 `setStyleSheet("color: #888; font-style: italic;")` 替换为：
```cpp
label->setProperty("class", "hint");
```
QSS 统一：
```css
QLabel[class="hint"] { color: %fg-muted%; font-style: italic; }
```

涉及文件：[roi_panel.cpp:47,52,184](file:///home/justin/GUI-for-openEB/gui/panels/roi_panel.cpp#L47)、[biases_panel.cpp:28,56,119,144,200](file:///home/justin/GUI-for-openEB/gui/panels/biases_panel.cpp#L28)、[trigger_panel.cpp:39,48,150,161,164](file:///home/justin/GUI-for-openEB/gui/panels/trigger_panel.cpp#L39)、[esp_panel.cpp:45,254,268,271](file:///home/justin/GUI-for-openEB/gui/panels/esp_panel.cpp#L45)、[settings_panel.cpp:64](file:///home/justin/GUI-for-openEB/gui/panels/settings_panel.cpp#L64)。

### 3.2 图标系统

#### 3.2.1 图标资源

引入 [Lucide](https://lucide.dev)（MIT 许可，~1000 个 SVG 图标），打包进 .qrc：
```
gui/resources/
  ├── icons.qrc
  └── icons/
       ├── camera.svg  camera-off.svg  record.svg  stop.svg
       ├── play.svg  pause.svg  step-forward.svg  step-back.svg
       ├── folder.svg  save.svg  load.svg  export.svg
       ├── settings.svg  sidebar.svg  layout.svg  fullscreen.svg
       ├── chart.svg  info.svg  filter.svg  roi.svg
       ├── calibration.svg  tools.svg  refresh.svg  connect.svg
       └── ...
```

#### 3.2.2 图标提供器

新建 `gui/app/icon_provider.h`：
```cpp
class IconProvider {
public:
    /// 返回随主题 currentColor 变色的图标
    static QIcon get(const QString& name);
    /// 返回指定颜色的图标（用于状态指示）
    static QIcon get(const QString& name, const QColor& color);
};
```

实现用 `QSvgRenderer` 渲染 SVG，根据当前令牌的 `fg-primary` / `accent` 着色。

#### 3.2.3 替换点

替换所有 `style()->standardIcon()` 调用：
- [main_window.cpp:277](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L277)（窗口控制按钮）
- [custom_title_bar.cpp:41](file:///home/justin/GUI-for-openEB/gui/widgets/custom_title_bar.cpp#L41)
- 工具栏所有 QAction 配图标
- 状态栏指示器配图标

### 3.3 Panel 抽象基类与事件解耦

#### 3.3.1 AbstractPanel 基类

新建 `gui/panels/abstract_panel.h`：
```cpp
class AbstractPanel : public QWidget {
    Q_OBJECT
public:
    explicit AbstractPanel(QWidget* parent = nullptr) : QWidget(parent) {}

    /// 面板唯一标识（用于布局持久化、配置序列化）
    virtual QString panel_id() const = 0;
    /// 面板显示标题（用于折叠分组的标题栏）
    virtual QString panel_title() const = 0;
    /// 所属分组（用于可折叠分组，见 §3.7）
    virtual QString panel_group() const = 0;

public slots:
    /// 相机连接时调用（默认空实现，子类按需重写）
    virtual void on_camera_connected(CameraController*) {}
    /// 相机断开时调用
    virtual void on_camera_disconnected() {}

signals:
    void info_message(const QString& msg);
    void error_message(const QString& msg);
};
```

12 个 Panel 继承之，删除重复的 `info_message`/`error_message` 声明和 `on_camera_connected`/`on_camera_disconnected` 签名。

#### 3.3.2 相机生命周期事件解耦

`CameraController` 已有 `connected`/`disconnected` 信号。各 Panel 在构造时自行订阅：
```cpp
BiasesPanel::BiasesPanel(QWidget* parent) : AbstractPanel(parent) {
    // panel 构造时不知道 camera_，由 MainWindow 在 setup 阶段注入
}
void BiasesPanel::bind_camera(CameraController* cam) {
    if (camera_) disconnect(camera_, nullptr, this, nullptr);
    camera_ = cam;
    if (camera_) {
        connect(camera_, &CameraController::connected, this, [this](const SensorInfo& i){
            on_camera_connected(camera_);
        });
        connect(camera_, &CameraController::disconnected, this, [this](){
            on_camera_disconnected();
        });
    }
}
```

MainWindow 的 `wire_signals()` 中 6 行 `settings_->xxx_panel()->on_camera_connected()` 删除，改为在 setup 阶段调用一次 `panel->bind_camera(&camera_)`。

#### 3.3.3 SettingsPanel 注册式管理

[settings_panel.h](file:///home/justin/GUI-for-openEB/gui/panels/settings_panel.h) 改为注册式：
```cpp
class SettingsPanel : public QWidget {
public:
    void register_panel(std::unique_ptr<AbstractPanel> panel);
    AbstractPanel* find_panel(const QString& id) const;
    std::vector<AbstractPanel*> panels_in_group(const QString& group) const;
private:
    std::vector<std::unique_ptr<AbstractPanel>> panels_;
};
```

去掉 11 个硬编码成员（`information_`/`statistics_`/`display_`/...）。MainWindow 通过 `find_panel("biases")` 按需访问。

**预期收益**：MainWindow 从 1538 行降到 ~700 行；`connect()` 从 54 降到 ~20；新增 panel 不需改 MainWindow。

### 3.4 algo_backend 拆分

#### 3.4.1 拆分方案

`algo_backend.cpp`（2897 行，50 个子类）按类别拆成 7 个文件：

```
gui/algo_bridge/backends/
  ├── cv_backends.cpp          // 13 个：NoiseFilter, HotPixel, OpticalGyro,
  │                            //        PerspectiveUndistort, ObjectTracker,
  │                            //        CornerDetector, BlobDetector,
  │                            //        SparseOpticalFlow, HoughLine,
  │                            //        HoughCircle, LineSegment,
  │                            //        OrientationCluster, ClusterLif
  ├── analytics_backends.cpp   // 8 个：EventToVideo, FlowStatistics,
  │                            //        ISIAnalyzer, FreqDetector,
  │                            //        ActiveMarker, ParticleCounter,
  │                            //        AutoBias, TriggerSynced
  ├── display_backends.cpp     // 4 个：TimeSurface, UltraSlowMotion,
  │                            //        XYTVisualizer, Overlay
  ├── filter_backends.cpp      // 4 个：OrientationFilter, DirectionSelective,
  │                            //        BackgroundMask, BandpassFilter
  ├── openeb_filter_backends.cpp  // RoiMask, AdaptiveRateSplit 等
  ├── openeb_frame_backends.cpp   // FrameIntegration, FrameDiff, FrameHisto 等
  └── openeb_util_backends.cpp    // UtilFrameComposer, UtilRateEstimator 等
```

`algo_backend.h` 保留 `AlgoBackend` 基类、`AlgoResult` 结构、`ProcessRegion`/`RoiFilter` helper。

注册逻辑仍在 [algo_bridge.cpp](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_bridge.cpp) 的 `register_self_cv()` / `register_self_analytics()` 等函数里，这些函数已存在（[algo_bridge.h:161-167](file:///home/justin/GUI-for-openEB/gui/algo_bridge/algo_bridge.h#L161-L167)）。

#### 3.4.2 CMakeLists 更新

[gui/CMakeLists.txt](file:///home/justin/GUI-for-openEB/gui/CMakeLists.txt) 的源文件列表更新，`algo_bridge/algo_backend.cpp` 替换为 7 个 `backends/*.cpp`。

**收益**：单文件 < 500 行；改一个 backend 只重编该文件；可独立测试。

### 3.5 显示策略多态化

#### 3.5.1 IDisplayStrategy 接口

新建 `gui/display/display_strategy.h`：
```cpp
struct DisplayContext {
    FrameAnnotator* annotator;
    QHash<std::string, QPointer<AlgoWindow>>* algo_windows;
    SpaceTimeDisplay* xyt_display;
    MainWindow* window;  // 用于 invokeMethod 到 GUI 线程
};

class IDisplayStrategy {
public:
    virtual ~IDisplayStrategy() = default;
    /// 将算法结果应用到主显示帧或路由到独立窗口
    virtual void apply(QImage& frame, AlgoResult& result,
                       const AlgoInfo& info, DisplayContext& ctx) = 0;
};
```

#### 3.5.2 四个具体策略

```cpp
class PassiveStrategy : public IDisplayStrategy {
    // 仅更新 AlgoWindow 状态文本，不绘制
};

class OverlayStrategy : public IDisplayStrategy {
    // 调用 annotator->draw_boxes/lines/points/circles/texts
    // 若 roi_enabled 且 AlgoWindow 已开，裁剪 ROI 区域推送到 AlgoWindow
};

class ReplaceStrategy : public IDisplayStrategy {
    // 用 result.frame 替换主显示帧
};

class StandaloneStrategy : public IDisplayStrategy {
    // 将 result.frame 推送到 AlgoWindow 或 SpaceTimeDisplay
};
```

#### 3.5.3 AlgoInstance 持有策略

`AlgoInstance` 在构造时根据 `AlgoInfo.display_mode` 创建对应策略：
```cpp
AlgoInstance::AlgoInstance(const AlgoInfo& info, int w, int h) {
    switch (info.display_mode) {
        case AlgoDisplayMode::Passive:   strategy_ = std::make_unique<PassiveStrategy>(); break;
        case AlgoDisplayMode::Overlay:   strategy_ = std::make_unique<OverlayStrategy>(); break;
        case AlgoDisplayMode::Replace:   strategy_ = std::make_unique<ReplaceStrategy>(); break;
        case AlgoDisplayMode::Standalone:strategy_ = std::make_unique<StandaloneStrategy>(); break;
    }
}
```

#### 3.5.4 MainWindow 简化

[main_window.cpp:1032-1121](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L1032-L1121) 的 `process_algo_results()` 坍缩为：
```cpp
void MainWindow::process_algo_results(QImage& frame) {
    DisplayContext ctx{&annotator_, &algo_windows_, xyt_display_.data(), this};
    for (auto& inst : algo_bridge_.list_live()) {
        if (inst->is_overloaded()) { /* 状态栏提示 */ continue; }
        if (!inst->is_enabled()) continue;
        AlgoResult r;
        try { r = inst->pull_result(); } catch (...) { continue; }
        inst->apply_strategy(frame, r, ctx);
    }
    draw_roi_overlays(frame);
}
```

增加新显示模式只需新增策略类，不改 MainWindow。

### 3.6 标题栏与工具栏重设计

#### 3.6.1 启用 CustomTitleBar

项目已有 [custom_title_bar.h](file:///home/justin/GUI-for-openEB/gui/widgets/custom_title_bar.h) 但未真正启用（main_window 用 QMenuBar hack）。

改为启用真正的自定义标题栏 widget，布局：
```
┌─────────────────────────────────────────────────────────────┐
│ [图标][EB plus]  [文件▾][视图▾][相机▾][工具▾][帮助▾]  [— □ ✕]│
│  应用名+图标       菜单按钮(下拉)              窗口控制       │
└─────────────────────────────────────────────────────────────┘
```

- 标题栏高度 36px，背景 `bg-panel`，底部 1px `border` 分隔
- 菜单从"顶部常驻 QMenuBar"改为"标题栏内下拉按钮"，释放垂直空间
- 窗口控制按钮用 Lucide SVG，悬停态 `bg-hover`
- 拖拽区域：标题栏空白处 `startSystemMove`，缩放仍用 [ResizeGrip](file:///home/justin/GUI-for-openEB/gui/widgets/custom_title_bar.h)

#### 3.6.2 工具栏分组

[main_window.cpp:468](file:///home/justin/GUI-for-openEB/gui/main_window.cpp#L468) 的 `main_toolbar_` 改为分组工具栏：
```
[📷连接▾] | [▶播放 ⏮ ⏯ ⏭] | [⏺录制] | [🛈侧栏 🗗布局] | [📤导出]
```

- 用 `addSeparator()` 分组，每组语义明确
- 图标 20px，按钮间距 4px，组间距 12px
- 检查态按钮用 `accent` 背景标识

#### 3.6.3 菜单结构

保留 [design.md §5.4](file:///home/justin/GUI-for-openEB/doc/design.md) 的菜单功能，但合并为 5 个下拉（已废弃的 Algorithm 菜单不恢复）：
- **文件**：Open Camera/File, Save/Load Settings, Export, File Tools, Exit
- **视图**：Toggle Sidebar, Toggle Playback, Reset/Save/Load Layout, Fullscreen
- **相机**：Connect/Disconnect, Device List, Platform Info, HAL Showcase
- **工具**：Add Display Window, Tile/Cascade Windows, Calibration Wizard
- **帮助**：About, Documentation

### 3.7 面板可折叠分组

#### 3.7.1 回归 design.md §5.1

[design.md §5.1](file:///home/justin/GUI-for-openEB/doc/design.md) 的布局图是**垂直堆叠**面板，实现变成了两个 tab。改为 VSCode 风格的可折叠 Section，回归原始设计：

```
▼ 相机设备        [Devices] [Information]
▼ 显示与统计      [Display] [Statistics]
▼ 硬件配置        [Biases] [ROI] [ESP] [Trigger]
▼ 算法模块        [Preprocessing] [Algorithms]
▼ 工具            [File Tools] [Calibration]
```

#### 3.7.2 CollapsibleSection 组件

新建 `gui/widgets/collapsible_section.h`：
```cpp
class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    CollapsibleSection(const QString& title, QWidget* parent = nullptr);
    void add_panel(AbstractPanel* panel);
    void set_collapsed(bool collapsed);
    bool is_collapsed() const;
    QString title() const;
signals:
    void collapsed_changed(const QString& title, bool collapsed);
};
```

- 标题栏 28px 高，左侧箭头图标（▶/▼），点击切换折叠
- 折叠状态持久化到 QSettings（`layout/section_<title>_collapsed`）
- 展开时面板垂直堆叠，`spacing=8`

#### 3.7.3 分组定义

| 分组 | 包含面板 | 默认状态 |
|------|----------|----------|
| 相机设备 | Devices, Information | 展开 |
| 显示与统计 | Display, Statistics | 展开 |
| 硬件配置 | Biases, ROI, ESP, Trigger | 展开 |
| 算法模块 | Preprocessing, Algorithms | 折叠（节省空间，用户按需展开） |
| 工具 | File Tools, Calibration | 折叠 |

分组通过 `AbstractPanel::panel_group()` 声明，`SettingsPanel` 按分组聚合。

### 3.8 微交互与状态反馈

#### 3.8.1 控件状态样式

`base.qss.in` 补充：
```css
QPushButton {
    background-color: %bg-input%;
    color: %fg-primary%;
    border: 1px solid %border%;
    border-radius: 5px;
    padding: 6px 14px;
    min-height: 26px;
}
QPushButton:hover    { background-color: %bg-hover%; }
QPushButton:pressed  { background-color: %bg-panel%; }
QPushButton:focus    { outline: 2px solid %accent%; outline-offset: 1px; }
QPushButton:disabled { color: %fg-muted%; background-color: %bg-panel%; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: %bg-input%;
    color: %fg-primary%;
    border: 1px solid %border%;
    border-radius: 5px;
    padding: 4px 8px;
    min-height: 26px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border: 1px solid %accent%;
}

QToolBar QToolButton {
    background: transparent;
    border: none;
    border-radius: 5px;
    padding: 6px;
}
QToolBar QToolButton:hover    { background-color: %bg-hover%; }
QToolBar QToolButton:checked  { background-color: %accent%; color: white; }
QToolBar QToolButton:disabled { color: %fg-muted%; }
```

#### 3.8.2 状态栏增强

[main_window.h:140-143](file:///home/justin/GUI-for-openEB/gui/main_window.h#L140-L143) 的 4 个 QLabel 增加图标和颜色编码：

| 状态 | 显示 | 颜色 |
|------|------|------|
| 连接成功 | `🟢 Connected: IMX636` | 绿色圆点 + `fg-primary` |
| 未连接 | `⚫ Disconnected` | 灰色圆点 + `fg-muted` |
| 录制中 | `🔴 REC 00:01:23` | 红色圆点（闪烁）+ `fg-primary` |
| 事件率 | `▶ 12.3 Mev/s` | 图标 + 等宽数值 |
| 时间戳 | `⏱ 12.345s` | 图标 + 等宽数值 |

圆点用 QLabel + 小尺寸 QPixmap 或 SVG，录制中圆点用 QTimer 闪烁（500ms 间隔）。

### 3.9 字体系统

#### 3.9.1 全局字体

[main.cpp](file:///home/justin/GUI-for-openEB/gui/main.cpp) 设置全局字体：
```cpp
QFont font(QStringLiteral("Inter"), 10);
font.setFamilies({QStringLiteral("Inter"),
                  QStringLiteral("Segoe UI"),
                  QStringLiteral("Ubuntu"),
                  QStringLiteral("Noto Sans"),
                  QStringLiteral("Sans Serif")});
font.setStyleStrategy(QFont::PreferAntialiasing);
app->setFont(font);
```

#### 3.9.2 等宽字体

数值类 QLabel（事件率、时间戳、坐标、像素值）用等宽：
```cpp
QFont mono(QStringLiteral("JetBrains Mono"), 9);
mono.setFamilies({QStringLiteral("JetBrains Mono"),
                  QStringLiteral("Consolas"),
                  QStringLiteral("Menlo"),
                  QStringLiteral("Monospace")});
status_rate_->setFont(mono);
status_ts_->setFont(mono);
```

[frame_annotator.h:100](file:///home/justin/GUI-for-openEB/gui/display/frame_annotator.h#L100) 已用 `QFont("Monospace", 9)`，统一为上述 mono 设置。

### 3.10 间距与圆角统一

`base.qss.in` 全局：
- `border-radius`: 2px → **5px**
- `padding`: 3px 10px → **6px 14px**（按钮）、2px → **4px 8px**（输入框）
- 控件最小高度：**26px**

各 panel：
- `setSpacing(4)` → `setSpacing(8)`
- `setContentsMargins(4,4,4,4)` → `setContentsMargins(8,8,8,8)`

### 3.11 GUI 测试

#### 3.11.1 测试目录

新建 `gui/tests/`：
```
gui/tests/
  ├── CMakeLists.txt
  ├── test_algo_bridge.cpp      // registry 完整性、find_or_create 幂等、flood guard
  ├── test_config_manager.cpp   // 序列化往返
  ├── test_layout_manager.cpp   // save/load 对称
  ├── test_display_strategy.cpp // 各策略 apply 正确性
  └── test_theme_tokens.cpp     // 令牌对比度校验（WCAG AA）
```

#### 3.11.2 优先测试项

| 测试 | 验证内容 | 依赖 |
|------|----------|------|
| `test_algo_bridge` | 31 自研 + 30 openEB 注册完整；`find_or_create` 幂等；flood guard 连续 4 批超限触发停用 | AlgoBridge |
| `test_config_manager` | save → load 往返一致性；算法参数、bias、ROI、ESP 序列化 | ConfigManager |
| `test_layout_manager` | save → load 几何对称；折叠状态持久化 | LayoutManager |
| `test_display_strategy` | Overlay 绘制 boxes/lines/points；Replace 替换帧；Standalone 推送到 window | IDisplayStrategy |
| `test_theme_tokens` | 10 个主题变体的 `fg-muted` vs `bg-primary` 对比度 ≥ 4.5:1 | ThemeController |

#### 3.11.3 接入 CI

[gui/CMakeLists.txt](file:///home/justin/GUI-for-openEB/gui/CMakeLists.txt) 启用 GTest + CTest（algo/ 已有先例，见 [algo/tests/CMakeLists.txt](file:///home/justin/GUI-for-openEB/algo/tests/CMakeLists.txt)）。

---

## 四、实施路线图

### 4.1 阶段划分

```
阶段一（视觉基础）  ──┐
                     ├──→ 阶段三（交互提升）
阶段二（架构瘦身） ──┘
                     └──→ 阶段四（测试）
```

阶段一、二可并行，互不依赖。

### 4.2 阶段一：视觉基础

**目标**：解决暗色模式对比度、跨平台图标、间距拥挤。

| 任务 | 改动范围 | 依赖 |
|------|----------|------|
| 3.1 令牌系统 | theme_controller 重构 + base.qss + tokens.h | 无 |
| 3.2 图标系统 | 新增 icons.qrc + icon_provider + 替换 5 处 standardIcon | 无 |
| 3.10 间距圆角 | base.qss + 各 panel setSpacing/setContentsMargins | 3.1 |
| 3.9 字体系统 | main.cpp 全局字体 + 数值类等宽 | 无 |
| 3.8.1 微交互 | base.qss 补充 :focus/:pressed/:checked | 3.1 |

**验收标准**：
- 暗色模式下所有提示文字对比度 ≥ 4.5:1
- 跨平台（Ubuntu/Windows）图标风格一致
- 控件间距视觉舒适，无拥挤感
- 焦点态有清晰 outline

### 4.3 阶段二：架构瘦身

**目标**：MainWindow 降权、拆分巨石、消除重复。

| 任务 | 改动范围 | 依赖 |
|------|----------|------|
| 3.3 Panel 基类 + 事件解耦 | 新增 abstract_panel.h + 12 panel 改继承 + SettingsPanel 注册式 | 无 |
| 3.4 algo_backend 拆分 | 2897 行拆成 7 文件 + CMakeLists 更新 | 无 |
| 3.5 显示策略多态化 | 新增 display_strategy.h + 4 策略 + process_algo_results 简化 | 无 |

**验收标准**：
- MainWindow 行数 < 700
- `connect()` 数量 < 25
- algo_backend 单文件 < 500 行
- 新增 panel 不需改 MainWindow
- 新增显示模式不需改 MainWindow

### 4.4 阶段三：交互提升

**目标**：标题栏专业、面板分组、状态栏信息密度。

| 任务 | 改动范围 | 依赖 |
|------|----------|------|
| 3.6 标题栏工具栏 | 启用 CustomTitleBar + 菜单下拉化 + 工具栏分组 | 阶段一（图标） |
| 3.7 面板可折叠分组 | 新增 CollapsibleSection + SettingsPanel 重构 | 阶段二（Panel 基类） |
| 3.8.2 状态栏增强 | 4 个 QLabel 加图标 + 颜色编码 + 录制闪烁 | 阶段一（图标） |

**验收标准**：
- 标题栏视觉专业，无 QMenuBar hack
- 面板可折叠且记忆状态
- 状态栏信息一眼可读

### 4.5 阶段四：测试

**目标**：GUI 核心逻辑有测试覆盖。

| 任务 | 改动范围 | 依赖 |
|------|----------|------|
| 3.11 GUI 测试 | 新增 gui/tests/ + CMakeLists + CI 接入 | 阶段二（策略可测） |

**验收标准**：
- 5 个测试文件通过
- `ctest` 包含 gui 测试
- AlgoBridge / ConfigManager / LayoutManager / DisplayStrategy 覆盖

---

## 五、保留项

以下明确**不动**：

- ✅ 5 色粉彩主题（Gray/Green/Yellow/Pink/Blue × Light/Dark）
- ✅ `ThemeController` 的 `FollowSystem` 模式
- ✅ `AlgoBridge` 注册表 + 工厂 + `weak_ptr` 架构
- ✅ `AlgoInstance` 洪水保护（[design.md §5.6.7](file:///home/justin/GUI-for-openEB/doc/design.md)）
- ✅ `FramePipeline` 线程边界划分
- ✅ `AlgoParamSpec.mode_filter` 声明式参数可见性
- ✅ `QDockWidget` 多窗口机制（[design.md §5.6.5](file:///home/justin/GUI-for-openEB/doc/design.md)）
- ✅ 算法 ROI 全算法支持（[design.md §5.6.6](file:///home/justin/GUI-for-openEB/doc/design.md)）
- ✅ 四种显示模式分类（Passive/Overlay/Replace/Standalone）

---

## 六、风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 令牌迁移工作量 | ~15 处裸 hex + theme_controller 重构 | 分批替换，每改一个 panel 验证 10 个主题变体 |
| Panel 基类引入 | 12 个 panel 改继承，Q_OBJECT 宏需重新 moc | 清理构建目录后重新 cmake |
| algo_backend 拆分 | 50 个类移文件，CMakeLists 同步 | 保持头文件不变，只拆 .cpp；先拆再编译验证 |
| 标题栏重做 | frameless 窗口拖拽/缩放在 Wayland 下行为 | 保留 ResizeGrip 方案，Wayland 下用 startSystemMove/Resize |
| 事件总线引入 | 信号槽从直接调用改为 panel 自订阅 | 相机连接逻辑先加日志验证，确保所有 panel 仍收到事件 |

---

## 七、附录

### 7.1 令牌对比度校验

WCAG AA 标准要求文字对比度 ≥ 4.5:1。当前 `#888` 在各暗色背景上的对比度：

| 背景 | `#888` 对比度 | 达标？ | `fg-muted` 暗色令牌对比度 | 达标？ |
|------|--------------|--------|--------------------------|--------|
| `#2D2D30` (Dark Gray) | 2.8:1 | ❌ | `#9A9A9A` → 4.6:1 | ✅ |
| `#1E3A22` (Dark Green) | 2.5:1 | ❌ | `#88A088` → 5.1:1 | ✅ |
| `#3D3520` (Dark Yellow) | 3.1:1 | ❌ | `#A89A78` → 4.8:1 | ✅ |
| `#3D2030` (Dark Pink) | 2.6:1 | ❌ | `#A88898` → 5.0:1 | ✅ |
| `#1E2A3D` (Dark Blue) | 2.3:1 | ❌ | `#8898AC` → 4.7:1 | ✅ |

### 7.2 文件变更清单

**新增文件**：
- `gui/resources/theme/tokens.h`
- `gui/resources/theme/base.qss.in`
- `gui/resources/icons.qrc` + `gui/resources/icons/*.svg`
- `gui/app/icon_provider.h` / `.cpp`
- `gui/panels/abstract_panel.h`
- `gui/display/display_strategy.h` / `.cpp`
- `gui/widgets/collapsible_section.h` / `.cpp`
- `gui/algo_bridge/backends/*.cpp`（7 个）
- `gui/tests/*.cpp`（5 个）+ `gui/tests/CMakeLists.txt`

**修改文件**：
- `gui/app/theme_controller.cpp`（令牌化 + 外置 QSS）
- `gui/main_window.cpp` / `.h`（瘦身 + 标题栏 + 工具栏 + 状态栏）
- `gui/main.cpp`（全局字体）
- `gui/panels/*.cpp` / `.h`（12 个 panel 改继承 + 清除裸 hex + 间距）
- `gui/panels/settings_panel.cpp` / `.h`（注册式 + 可折叠分组）
- `gui/algo_bridge/algo_bridge.h` / `.cpp`（AlgoInstance 持有 strategy）
- `gui/algo_bridge/algo_backend.cpp`（拆分后删除或仅保留基类）
- `gui/CMakeLists.txt`（源文件列表 + GTest）

**删除文件**：
- 无（algo_backend.cpp 内容拆分到 backends/ 后可删除原文件）

---

*本文档与 [design.md](file:///home/justin/GUI-for-openEB/doc/design.md) 配合使用，design.md 定义"做什么"，本文档定义"怎么做得更好"。*
