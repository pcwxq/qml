# QLabel / Qt Quick 刷新对照原型

两个独立程序，共用 C++ 数据驱动与采样逻辑：

- `widgets_refresh`：Qt Widgets + QLabel。
- `quick_refresh`：独立 Qt Quick 窗口 + QML Text，未使用 QQuickWidget。

第一版用于手动探索 10–80 ms 更新周期。允许跳过中间值，始终提交当前时间对应的最新值。没有数据库、网络和业务计算，也不包含自动测试或自动压测。

## 本机已生成的程序

已使用 `E:/qt/6.10.2/msvc2022_64` 和 MSVC 编译 Release，并部署 Qt DLL、平台插件与 QML 模块：

- `build/Release/widgets_refresh.exe`
- `build/Release/quick_refresh.exe`

可以在本机分别手动启动。交付时未启动 GUI、未运行自动测试或性能测试；运行效果需要你手动确认。若复制到另一台机器，应复制整个 Release 目录，并安装匹配的 Microsoft Visual C++ 运行库。

## 构建

需要 Qt **6.5 或更新版本**，安装匹配编译器的 Desktop Kit，包含 Widgets、Qt Declarative / Quick、Quick Controls 2；CMake 3.21+ 和 C++17 编译器。

### Qt Creator（推荐）

1. 打开本目录 `CMakeLists.txt`。
2. 选择 Qt 6 Desktop Kit，构建类型选 **Release**。
3. 构建后，在运行配置中分别选择 `widgets_refresh` 和 `quick_refresh`。
4. 每次只运行一个程序进行比较。

### Windows 命令行示例

在匹配 Qt Kit 的编译环境中执行。将路径替换为你实际安装的 Qt；MSVC Qt 不能和 MinGW 混用。

```powershell
# 示例：Visual Studio 2022 x64 + 匹配的 Qt MSVC Kit
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release

# 部署运行依赖，也复制 Qt Quick 所需的 QML 模块。
& "C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe" --release build/Release/widgets_refresh.exe
& "C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe" --release --qmldir quick build/Release/quick_refresh.exe

./build/Release/widgets_refresh.exe
./build/Release/quick_refresh.exe
```

不要同时启动两者进行正式对照。代码交付时是否经过构建，以交付说明为准；上述 Qt 路径只是示例。

## 使用

1. 设置 **控件总数**（1–10000 个）、**每行控件数**（1–50 个/行）、**刷新间隔**（10–80 毫秒）、**每次更新比例**（1–100%）。每个输入框上方为名称、下方为用途说明。
2. 点击 **应用参数** 创建全部对象；创建耗时不包含在刷新样本中。下方“已应用”一行展示真正生效的设置及每次更新的控件数量。
3. 等待初始界面显示稳定，然后点击 **开始刷新**。参数在运行中锁定；修改未应用时不能开始。
4. 观察数值、滚动响应；点击 **停止刷新** 会提交停止瞬间对应的最终批次。
5. 点击 **导出 CSV**，导出到系统“文档”目录的 `RefreshComparison` 子目录，状态栏显示完整路径。

**Apply 和重新 Start 都会清空之前的样本，请先导出。** 关闭程序不会自动保存。最多保留 200000 条样本，到达容量时自动停止并保留最终提交。10 ms 周期下约可记录 33 分钟，实际取决于触发次数。

默认配置：300 个标签、10 列、20 ms、100% 变化。参数名称、操作按钮和用途说明为中文。

## 公共负载定义

- 一个 GUI 线程中的 `QTimer`，类型 `Qt::PreciseTimer`。
- 每格 112 × 28 **逻辑像素**，水平/垂直间隔 2，Consolas 16 像素，八位纯数字，不换行。系统缺少字体时会回退。
- 固定位置、固定尺寸；本版不包含动态布局、富文本、变长文本模式。
- 两边都一次性创建全部对象；Quick 使用 Repeater，未使用 ListView 虚拟化。视口之外的对象仍存在，不能将总数理解为可见数。
- 两边滚动外框尺寸相同，但原生与 Quick 滚动条占用、文字渲染方式可能不同。比较时保持窗口大小、DPI、滚动位置一致，记录实际可见数量。
- 预生成 10000 个 QString。以 `floor(经过时间 / 周期)` 作为数据批次，控件值为 `(批次 + 行索引) % 10000` 对应的字符串。不生成随机数、不做每格数字格式化；字符串池按周期重复，状态栏显示未取模的批次号。
- 部分更新固定选择前 `ceil(数量 × 比例 / 100)` 个对象，其他对象保持起始值。这是**空间集中的局部更新**，不是随机分散更新；不要将结论泛化到所有局部更新分布。
- 迟到直接选择最新批次，不补旧批次。两边通过同一个模型通知前端，Widgets 转成 setText，Quick 通过角色绑定更新 Text。
- 状态栏每 500 ms 更新一次；存在少量共有的观察开销。运行时不排序、不逐条打印或写磁盘。

## 已实现的指标

停止后显示批量提交耗时 P50/P95/P99、回调间隔 P99/最大值。百分位使用 nearest-rank；最终 flush 不参与这些统计。未跨入新批次的回调仅参与间隔统计，不参与提交耗时分位数。

| CSV 字段 | 含义 |
|---|---|
| frontend / qt_version | 前端名称及实际运行的 Qt 版本 |
| count / columns / interval_ms / change_percent | 本轮已应用参数 |
| elapsed_ns | 本次回调入口距 Start 计时点的时间 |
| callback_gap_ns | 距上次回调入口的间隔；第一条从 Start 算起 |
| source_batch | 本次按时间选中的数据批次 |
| skipped_batches | 两次提交之间跨过但未提交的源批次数量，不是丢帧数 |
| submit_ns | 模型值写入与同步 dataChanged 处理的墙钟耗时 |
| final_flush | 1 为 Stop 的最终提交，不是周期定时器样本 |
| updated | 1 表示本次提交了新的数据批次；0 表示批次未变化，无需通知前端 |

**submit_ns 不是完整刷新耗时，也不是呈现延迟。** Widgets 包含同步 setText；Quick 包含当时同步执行的模型通知/绑定处理，而后续布局、绘制、场景图处理未必在此期间发生。不能仅凭该值宣布某个前端更流畅。

CSV 的 callback_gap 可以评估调度稳定性；其中包含操作系统和 GUI 线程繁忙等因素，不能直接称为“底层定时器误差”。最终值提交后仍需事件循环完成绘制。

## 手动观察建议

先固定列数、字体和窗口尺寸，从 100、300、1000 个对象与 10、20、40、80 ms 周期中选择配置。先比较 100% 变化，再看 10% 和 50%。不要用不可见对象的数量代表屏幕绘制压力。

正式比较时先预热，重新 Start 后再采样；两程序交替、分别运行。保持 Windows 电源模式、屏幕刷新率、DPI 和 Qt Kit 相同。可用任务管理器手动观察 CPU/GPU/内存，但注意进程 CPU 百分比可能掩盖单个线程满载。

核验最终值：Stop 后，第一格应与状态栏 `first value` 一致；修改比例覆盖的第 i 格应为 `(batch + i) % 10000` 的八位数字。长时间阻塞跨越整个字符串池时可能出现同值，这是池的重复，不代表完整展示了中间数据。

## 本版范围与后续项

已实现两个前端、手动参数控制、最新值驱动、停止最终提交、内部更新/调度采样、CSV 导出。

尚未实现：实际屏幕呈现批次追踪、新鲜度/最长画面停滞测量、自动输入延迟测量、进程 CPU/GPU/内存采样、启动到首屏计时、动态布局等扩展场景。因此本版提供可运行的对照载体，不提供“谁更快”的结论。后续应在两边采用同口径的呈现测量，不能把 QLabel paintEvent 和 Qt Quick frameSwapped 直接当作等价时间点。
