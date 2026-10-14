#include "RefreshController.h"

#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("Widgets Refresh Comparison");
    RefreshController controller("widgets");
    QWidget window;
    window.setWindowTitle("QLabel 刷新对照");
    window.resize(1280, 920);
    auto *layout = new QVBoxLayout(&window);
    auto *title = new QLabel("Qt Widgets / QLabel · 刷新对照");
    layout->addWidget(title);
    auto *controls = new QHBoxLayout;
    layout->addLayout(controls);
    const auto makeSpin = [&](const QString &name, const QString &unit, const QString &help,
                              int min, int max, int value) {
        auto *field = new QVBoxLayout;
        controls->addLayout(field, 1);
        auto *label = new QLabel(name);
        field->addWidget(label);
        auto *spin = new QSpinBox;
        spin->setRange(min, max);
        spin->setValue(value);
        spin->setSuffix(unit);
        spin->setAccessibleName(name);
        spin->setToolTip(help);
        label->setBuddy(spin);
        field->addWidget(spin);
        auto *description = new QLabel(help);
        description->setWordWrap(true);
        field->addWidget(description);
        return spin;
    };
    auto *count = makeSpin("控件总数", " 个", "创建多少个文本控件（含屏幕外）。", 1, 10000, 300);
    auto *columns = makeSpin("每行控件数", " 个/行", "只调整排列宽度，不改变总数。", 1, 50, 10);
    auto *period = makeSpin("刷新间隔", " 毫秒", "越小更新越频繁：10 毫秒约 100 次/秒。", 10, 80, 20);
    auto *percent = makeSpin("每次更新比例", " %", "100% 为全部；10% 只更新前十分之一。", 1, 100, 100);
    auto *actions = new QHBoxLayout;
    layout->addLayout(actions);
    auto *apply = new QPushButton("应用参数");
    auto *start = new QPushButton("开始刷新");
    auto *stop = new QPushButton("停止刷新");
    auto *exportButton = new QPushButton("导出 CSV");
    for (auto *button : {apply, start, stop, exportButton}) actions->addWidget(button);
    auto *editState = new QLabel("参数已应用，可以开始。应用参数或重新开始会清空上次样本。");
    layout->addWidget(editState);
    auto *applied = new QLabel(controller.configurationSummary());
    layout->addWidget(applied);
    QObject::connect(&controller, &RefreshController::configurationChanged, applied,
                     [&]() { applied->setText(controller.configurationSummary()); });
    auto *hint = new QLabel("操作顺序：停止刷新 → 修改参数 → 应用参数 → 开始刷新。可滚动查看屏幕外的控件。");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setFixedSize(1140, 540);
    scroll->viewport()->setStyleSheet("background: #ffffff;");
    layout->addWidget(scroll);
    std::vector<QLabel *> labels;
    const auto rebuild = [&]() {
        labels.clear();
        auto *canvas = new QWidget;
        canvas->setStyleSheet("background: #ffffff; color: #202020;");
        const int rows = (controller.count() + controller.columns() - 1) / controller.columns();
        canvas->setFixedSize(controller.columns() * 114 - 2, rows * 30 - 2);
        QFont font("Consolas");
        font.setPixelSize(16);
        labels.reserve(size_t(controller.count()));
        for (int i = 0; i < controller.count(); ++i) {
            auto *label = new QLabel(controller.textAt(i), canvas);
            label->setGeometry((i % controller.columns()) * 114,
                               (i / controller.columns()) * 30, 112, 28);
            label->setFont(font);
            label->setTextFormat(Qt::PlainText);
            label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            label->setMargin(0);
            label->setWordWrap(false);
            labels.push_back(label);
        }
        scroll->setWidget(canvas); // QScrollArea owns and deletes the previous canvas.
    };
    QObject::connect(&controller, &QAbstractItemModel::modelReset, &window, rebuild);
    QObject::connect(&controller, &QAbstractItemModel::dataChanged, &window,
        [&](const QModelIndex &first, const QModelIndex &last, const QList<int> &) {
            for (int i = first.row(); i <= last.row(); ++i)
                labels[size_t(i)]->setText(controller.textAt(i));
        });
    auto *status = new QLabel;
    status->setWordWrap(true);
    status->setMinimumHeight(64);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);
    layout->addWidget(new QLabel("统计为同步更新调用耗时，不代表完整绘制耗时或屏幕显示延迟。"));
    QObject::connect(&controller, &RefreshController::statusChanged, status,
                     [&]() { status->setText(controller.status()); });
    QObject::connect(apply, &QPushButton::clicked, &window, [&]() {
        if (controller.configure(count->value(), columns->value(), period->value(), percent->value()))
            editState->setText("参数已应用，可以开始。应用参数或重新开始会清空上次样本。");
    });
    QObject::connect(start, &QPushButton::clicked, &controller, &RefreshController::start);
    QObject::connect(stop, &QPushButton::clicked, &controller, &RefreshController::stop);
    QObject::connect(exportButton, &QPushButton::clicked, &window, [&]() { controller.exportCsv(); });
    const auto updateControls = [&]() {
        for (auto *spin : {count, columns, period, percent}) spin->setEnabled(!controller.running());
        apply->setEnabled(!controller.running());
        start->setEnabled(!controller.running());
        stop->setEnabled(controller.running());
        exportButton->setEnabled(!controller.running());
    };
    QObject::connect(&controller, &RefreshController::runningChanged, &window, updateControls);
    // Editing a setting requires Apply; Start always uses the last applied configuration.
    for (auto *spin : {count, columns, period, percent})
        QObject::connect(spin, &QSpinBox::valueChanged, &window, [=]() {
            start->setEnabled(false);
            editState->setText("参数已修改，尚未生效：请点击「应用参数」。");
        });
    QObject::connect(&controller, &RefreshController::configurationChanged, &window, updateControls);
    rebuild();
    status->setText(controller.status());
    updateControls();
    window.show();
    return app.exec();
}
