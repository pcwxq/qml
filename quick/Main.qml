import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 920
    minimumWidth: 1180
    minimumHeight: 880
    visible: true
    title: "Qt Quick Text 刷新对照"
    property bool settingsDirty: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 9
        spacing: 6
        Label { text: "Qt Quick / Text · 刷新对照" }
        RowLayout {
            Layout.fillWidth: true
            spacing: 20
            enabled: !controller.running
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "控件总数（个）"; font.bold: true }
                SpinBox {
                    id: countInput
                    Layout.fillWidth: true
                    Accessible.name: "控件总数（个）"
                    from: 1; to: 10000; value: 300; editable: true
                    onValueModified: window.settingsDirty = true
                }
                Label { text: "创建多少个文本控件（含屏幕外）。" }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "每行控件数（个/行）"; font.bold: true }
                SpinBox {
                    id: columnsInput
                    Layout.fillWidth: true
                    Accessible.name: "每行控件数（个/行）"
                    from: 1; to: 50; value: 10; editable: true
                    onValueModified: window.settingsDirty = true
                }
                Label { text: "只调整排列宽度，不改变总数。" }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "刷新间隔（毫秒）"; font.bold: true }
                SpinBox {
                    id: periodInput
                    Layout.fillWidth: true
                    Accessible.name: "刷新间隔（毫秒）"
                    from: 10; to: 80; value: 20; editable: true
                    onValueModified: window.settingsDirty = true
                }
                Label { text: "越小越频繁：10 毫秒约 100 次/秒。" }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: "每次更新比例（%）"; font.bold: true }
                SpinBox {
                    id: percentInput
                    Layout.fillWidth: true
                    Accessible.name: "每次更新比例（%）"
                    from: 1; to: 100; value: 100; editable: true
                    onValueModified: window.settingsDirty = true
                }
                Label { text: "100% 为全部；10% 只更新前十分之一。" }
            }
        }
        RowLayout {
            Button {
                text: "应用参数"
                enabled: !controller.running
                onClicked: {
                    if (controller.configure(countInput.value, columnsInput.value,
                                             periodInput.value, percentInput.value))
                        window.settingsDirty = false
                }
            }
            Button {
                text: "开始刷新"
                enabled: !controller.running && !window.settingsDirty
                onClicked: controller.start()
            }
            Button {
                text: "停止刷新"
                enabled: controller.running
                onClicked: controller.stop()
            }
            Button {
                text: "导出 CSV"
                enabled: !controller.running
                onClicked: controller.exportCsv()
            }
            Label {
                text: window.settingsDirty ? "参数已修改，尚未生效：请点击「应用参数」。" : "参数已应用。应用参数或重新开始会清空上次样本。"
            }
        }
        Label { text: controller.configurationSummary }
        Label {
            text: "操作顺序：停止刷新 → 修改参数 → 应用参数 → 开始刷新。可滚动查看屏幕外的控件。"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Rectangle {
            Layout.minimumWidth: 1140
            Layout.maximumWidth: 1140
            Layout.minimumHeight: 540
            Layout.maximumHeight: 540
            color: "white"
            Flickable {
                id: viewport
                anchors.fill: parent
                clip: true
                contentWidth: controller.columns * 114 - 2
                contentHeight: Math.ceil(controller.count / controller.columns) * 30 - 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                ScrollBar.horizontal: ScrollBar { }
                // Eager creation intentionally matches QLabel object counts.
                Repeater {
                    model: controller
                    delegate: Text {
                        required property int index
                        required property string labelText
                        x: (index % controller.columns) * 114
                        y: Math.floor(index / controller.columns) * 30
                        width: 112
                        height: 28
                        text: labelText
                        textFormat: Text.PlainText
                        wrapMode: Text.NoWrap
                        font.family: "Consolas"
                        font.pixelSize: 16
                        color: "#202020"
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Label {
            text: controller.status
            Layout.fillWidth: true
            Layout.minimumHeight: 64
            wrapMode: Text.WrapAnywhere
        }
        Label { text: "统计为同步更新调用耗时，不代表完整绘制耗时或屏幕显示延迟。" }
    }
}
