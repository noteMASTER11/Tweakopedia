import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root
    required property var controller
    readonly property var overview: controller.systemOverview || ({})
    readonly property bool wideLayout: width >= 900
    padding: 0

    function value(key, fallback) {
        const result = overview[key]
        return result === undefined || result === null || result === "" ? fallback : result
    }

    function logoSource() {
        if (overview.logo === "windows11") return "qrc:/images/windows-11.svg"
        if (overview.logo === "windows10") return "qrc:/images/windows-10.svg"
        return "qrc:/images/windows.svg"
    }

    function healthColor(tone) {
        if (tone === "good") return FluentTheme.stateOn
        if (tone === "warning") return "#B36A00"
        if (tone === "bad") return FluentTheme.danger
        return FluentTheme.textSecondary
    }

    background: Rectangle {
        objectName: "overviewBackground"
        color: FluentTheme.canvas
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16

            Item { Layout.preferredHeight: 12 }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 12

                ColumnLayout {
                    spacing: 3
                    FluentText {
                        objectName: "overviewGreeting"
                        text: root.overview.greetingName
                              ? "Здравствуйте, " + root.overview.greetingName + "!"
                              : "Здравствуйте!"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }
                    FluentText {
                        text: "Паспорт компьютера"
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    id: refreshButton
                    objectName: "overviewRefreshButton"
                    text: "Обновить данные"
                    enabled: !root.controller.systemOverviewLoading
                    font.family: FluentTheme.fontFamily
                    onClicked: root.controller.refreshSystemOverview()
                    contentItem: FluentText {
                        text: refreshButton.text
                        color: refreshButton.enabled ? FluentTheme.accent : FluentTheme.disabledText
                        font: refreshButton.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: refreshButton.hovered ? FluentTheme.hover : "transparent"
                        radius: 6
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: 54
                visible: root.controller.systemOverviewLoading
                         || root.controller.systemOverviewError !== ""
                radius: 8
                color: root.controller.systemOverviewError !== "" ? "#FFF4CE" : FluentTheme.surface
                border.color: root.controller.systemOverviewError !== "" ? "#E5C365" : FluentTheme.stroke

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    BusyIndicator {
                        running: root.controller.systemOverviewLoading
                        visible: running
                        implicitWidth: 22
                        implicitHeight: 22
                    }
                    FluentText {
                        Layout.fillWidth: true
                        text: root.controller.systemOverviewError !== ""
                              ? root.controller.systemOverviewError
                              : "Получение сведений о компьютере…"
                        color: root.controller.systemOverviewError !== "" ? "#7A5412" : FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: root.wideLayout ? 2 : 1
                columnSpacing: 16
                rowSpacing: 16

                Rectangle {
                    Layout.row: 0
                    Layout.column: 0
                    Layout.fillWidth: !root.wideLayout
                    Layout.preferredWidth: root.wideLayout ? 350 : -1
                    Layout.fillHeight: root.wideLayout
                    implicitHeight: root.wideLayout ? 614 : 520
                    radius: 12
                    color: "#EEF6FF"
                    border.color: "#C8DCF4"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 14

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 210
                            radius: 10
                            color: FluentTheme.surface
                            border.color: "#D6E5F6"

                            Image {
                                objectName: "overviewWindowsLogo"
                                anchors.centerIn: parent
                                width: 142
                                height: 142
                                source: root.logoSource()
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                            }
                        }

                        FluentText {
                            objectName: "overviewComputerName"
                            Layout.fillWidth: true
                            text: root.value("computerName", "Этот компьютер")
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: root.value("osCaption", "Windows")
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: "#C8DCF4"
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 13
                            columnSpacing: 12

                            FluentText { text: "Производитель"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("manufacturer", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                            FluentText { text: "Модель"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("model", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                            FluentText { text: "Материнская плата"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("baseboard", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                            FluentText { text: "Версия BIOS"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("biosSummary", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight; elide: Text.ElideRight }
                            FluentText { text: "Режим BIOS"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("biosMode", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight }
                            FluentText { text: "Время работы"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            FluentText { Layout.fillWidth: true; text: root.value("uptime", "Нет данных"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; horizontalAlignment: Text.AlignRight }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                ColumnLayout {
                    Layout.row: root.wideLayout ? 0 : 1
                    Layout.column: root.wideLayout ? 1 : 0
                    Layout.fillWidth: true
                    Layout.fillHeight: root.wideLayout
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 116
                        radius: 10
                        color: FluentTheme.surface
                        border.color: FluentTheme.stroke
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 5
                            FluentText { text: "Процессор"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 17; font.weight: Font.DemiBold }
                            FluentText {
                                objectName: "overviewProcessorName"
                                Layout.fillWidth: true
                                text: root.overview.processor ? root.overview.processor.title : "Нет данных"
                                color: FluentTheme.textPrimary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            FluentText { text: root.overview.processor ? root.overview.processor.details : ""; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 13 }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 125
                        radius: 10
                        color: FluentTheme.surface
                        border.color: FluentTheme.stroke
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                FluentText { text: "Оперативная память"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 17; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                FluentText { text: root.overview.memory ? root.overview.memory.usedPercent + "% используется" : ""; color: FluentTheme.stateOn; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            }
                            FluentText {
                                objectName: "overviewMemoryTitle"
                                text: root.overview.memory ? root.overview.memory.title : "Нет данных"
                                color: FluentTheme.textPrimary
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                            }
                            FluentText { Layout.fillWidth: true; text: root.overview.memory ? root.overview.memory.details : ""; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 13; elide: Text.ElideRight }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 6
                                radius: 3
                                color: "#E8EBF0"
                                Rectangle {
                                    width: parent.width * Math.min(100, Math.max(0, root.overview.memory ? root.overview.memory.usedPercent : 0)) / 100
                                    height: parent.height
                                    radius: parent.radius
                                    color: FluentTheme.accent
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 76 + graphicsRepeater.count * 62
                        radius: 10
                        color: FluentTheme.surface
                        border.color: FluentTheme.stroke
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                FluentText { text: "Видеокарты"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 17; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                FluentText { text: graphicsRepeater.count + " адапт."; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            }
                            Repeater {
                                id: graphicsRepeater
                                model: root.overview.graphics || []
                                delegate: ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 1
                                    FluentText { Layout.fillWidth: true; text: modelData.name; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                    FluentText { Layout.fillWidth: true; text: modelData.details; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12; elide: Text.ElideRight }
                                    FluentText { Layout.fillWidth: true; visible: modelData.technical !== ""; text: modelData.technical; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 11; elide: Text.ElideRight }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: root.wideLayout
                        implicitHeight: Math.max(156, 70 + diskRepeater.count * 55)
                        radius: 10
                        color: FluentTheme.surface
                        border.color: FluentTheme.stroke
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                FluentText { text: "Накопители"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 17; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                FluentText { text: diskRepeater.count + " устройств"; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                            }
                            Repeater {
                                id: diskRepeater
                                objectName: "overviewDiskRepeater"
                                model: root.overview.disks || []
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    spacing: 12
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        FluentText { Layout.fillWidth: true; text: modelData.name; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                        FluentText { text: modelData.details; color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 12 }
                                    }
                                    Rectangle {
                                        implicitWidth: healthLabel.implicitWidth + 20
                                        implicitHeight: 28
                                        radius: 14
                                        color: Qt.alpha(root.healthColor(modelData.healthTone), 0.10)
                                        FluentText {
                                            id: healthLabel
                                            anchors.centerIn: parent
                                            text: modelData.healthText
                                            color: root.healthColor(modelData.healthTone)
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.row: root.wideLayout ? 1 : 2
                    Layout.column: 0
                    Layout.columnSpan: root.wideLayout ? 2 : 1
                    Layout.fillWidth: true
                    implicitHeight: 82
                    radius: 10
                    color: FluentTheme.surface
                    border.color: FluentTheme.stroke

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            FluentText { text: root.value("osCaption", "Windows"); color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 18; font.weight: Font.DemiBold }
                            FluentText { text: root.value("osSummary", "Сведения отсутствуют"); color: FluentTheme.textSecondary; font.family: FluentTheme.fontFamily; font.pixelSize: 13 }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
