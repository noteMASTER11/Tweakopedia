import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    property int selectedToolIndex: -1
    property var integrityController
    property var fileSystemController
    property var networkController
    property var controller
    readonly property bool wideLayout: width >= 820
    signal articleRequested(string id)
    signal reviewRequested()
    signal tweakRequested(string id)
    padding: 0

    function toolAccent(index) {
        if (index === 1) return FluentTheme.positive
        if (index === 2) return FluentTheme.accentColor("violet", FluentTheme.dark)
        if (index === 3) return FluentTheme.danger
        return FluentTheme.accent
    }

    function toolTint(index) {
        if (index === 1) return FluentTheme.positiveSurface
        if (index === 2) return FluentTheme.accentTint("violet", FluentTheme.dark, FluentTheme.midnight)
        if (index === 3) return FluentTheme.dangerSurface
        return FluentTheme.infoSurface
    }

    background: Rectangle {
        objectName: "toolsBackground"
        color: FluentTheme.canvas
    }

    ListModel {
        id: toolsModel

        ListElement {
            title: "Проверка целостности системы"
            description: "Проверяет хранилище компонентов Windows средствами DISM."
            badge: "DISM"
            glyph: "\uE950"
            accent: "#0067C0"
            tint: "#EAF3FF"
        }
        ListElement {
            title: "Проверка файловой системы"
            description: "Проверяет выбранные накопители средствами CHKDSK без исправления ошибок."
            badge: "CHKDSK · Только проверка"
            glyph: "\uEDA2"
            accent: "#107C10"
            tint: "#E8F5EA"
        }
        ListElement {
            title: "Диагностика сети"
            description: "Анализирует адаптеры, DNS, прокси, маршруты и правила брандмауэра."
            badge: "Без изменений"
            glyph: "\uE968"
            accent: "#5C2D91"
            tint: "#F1ECFB"
        }
        ListElement {
            title: "Блокировка телеметрии"
            description: "Собирает совместимый пакет ограничений из проверенных параметров Windows."
            badge: "Через общую очередь"
            glyph: "\uEA18"
            accent: "#C42B1C"
            tint: "#FDEBED"
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.selectedToolIndex < 0 ? 0
            : root.selectedToolIndex === 0 ? 1
            : root.selectedToolIndex === 1 ? 2
            : root.selectedToolIndex === 2 ? 3 : 4

        Item {
            id: overview
            objectName: "toolsOverview"

            ScrollView {
                id: overviewScroll
                anchors.fill: parent
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: overviewScroll.availableWidth
                    spacing: 8

                    Item { Layout.preferredHeight: 16 }

                    FluentText {
                        objectName: "toolsTitle"
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        text: "Инструменты"
                        color: FluentTheme.textPrimary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }

                    FluentText {
                        objectName: "toolsSubtitle"
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        text: "Проверка состояния Windows и встроенные средства обслуживания."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                    }

                    Item { Layout.preferredHeight: 12 }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 28
                        Layout.rightMargin: 28
                        columns: root.wideLayout ? 2 : 1
                        columnSpacing: 14
                        rowSpacing: 14

                        Repeater {
                            id: cardRepeater
                            objectName: "toolCardRepeater"
                            model: toolsModel

                            delegate: AbstractButton {
                                id: card

                                required property int index
                                required property string title
                                required property string description
                                required property string badge
                                required property string glyph
                                required property string accent
                                required property string tint

                                objectName: "toolCard_" + index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 164
                                focusPolicy: Qt.StrongFocus
                                Accessible.role: Accessible.Button
                                Accessible.name: title
                                onClicked: root.selectedToolIndex = index

                                contentItem: RowLayout {
                                    spacing: 18

                                    Rectangle {
                                        Layout.preferredWidth: 72
                                        Layout.preferredHeight: 72
                                        Layout.leftMargin: 22
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 36
                                        color: root.toolTint(index)

                                        Text {
                                            anchors.centerIn: parent
                                            text: card.glyph
                                            color: root.toolAccent(index)
                                            font.family: "Segoe MDL2 Assets"
                                            font.pixelSize: 30
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 7

                                        FluentText {
                                            Layout.fillWidth: true
                                            text: card.title
                                            color: FluentTheme.textPrimary
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 17
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.WordWrap
                                        }

                                        FluentText {
                                            Layout.fillWidth: true
                                            text: card.description
                                            color: FluentTheme.textSecondary
                                            font.family: FluentTheme.fontFamily
                                            font.pixelSize: 13
                                            lineHeight: 1.2
                                            wrapMode: Text.WordWrap
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: badgeText.implicitWidth + 22
                                            Layout.preferredHeight: 28
                                            radius: 6
                                            color: FluentTheme.surfaceInset

                                            FluentText {
                                                id: badgeText
                                                anchors.centerIn: parent
                                                text: card.badge
                                                color: FluentTheme.textSecondary
                                                font.family: FluentTheme.fontFamily
                                                font.pixelSize: 12
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.preferredWidth: 22
                                        Layout.rightMargin: 20
                                        Layout.alignment: Qt.AlignVCenter
                                        text: "\uE76C"
                                        color: card.hovered || card.activeFocus
                                            ? FluentTheme.accent : FluentTheme.textSecondary
                                        font.family: "Segoe MDL2 Assets"
                                        font.pixelSize: 17
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                background: Rectangle {
                                    radius: 10
                                    color: card.pressed
                                        ? FluentTheme.selected
                                        : card.hovered ? FluentTheme.hover : FluentTheme.surface
                                    border.width: card.hovered || card.activeFocus ? 2 : 1
                                    border.color: card.hovered || card.activeFocus
                                        ? FluentTheme.accent : FluentTheme.stroke
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 24 }
                }
            }
        }

        SystemIntegrityPage {
            controller: root.integrityController
            onBackRequested: root.selectedToolIndex = -1
            onArticleRequested: id => root.articleRequested(id)
        }

        FileSystemCheckPage {
            controller: root.fileSystemController
            onBackRequested: root.selectedToolIndex = -1
            onArticleRequested: id => root.articleRequested(id)
        }

        NetworkDiagnosticsPage {
            controller: root.networkController
            onBackRequested: root.selectedToolIndex = -1
            onArticleRequested: id => root.articleRequested(id)
        }

        TelemetryToolPage {
            controller: root.controller
            onBackRequested: root.selectedToolIndex = -1
            onReviewRequested: root.reviewRequested()
            onTweakRequested: id => root.tweakRequested(id)
        }
    }
}
