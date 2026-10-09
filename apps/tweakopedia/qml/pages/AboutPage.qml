import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    readonly property bool wideLayout: width >= 860
    padding: 0

    background: Rectangle {
        objectName: "aboutBackground"
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

            FluentText {
                objectName: "aboutTitle"
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                text: "О программе"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: root.wideLayout ? 204 : 330
                radius: 12
                color: "#EEF6FF"
                border.color: "#C8DCF4"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    columns: root.wideLayout ? 2 : 1
                    columnSpacing: 24
                    rowSpacing: 16

                    Rectangle {
                        Layout.preferredWidth: root.wideLayout ? 156 : 118
                        Layout.preferredHeight: root.wideLayout ? 156 : 118
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        radius: 24
                        color: FluentTheme.surface
                        border.color: "#D6E5F6"

                        Image {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            height: parent.height - 24
                            source: "qrc:/images/tweakopedia-icon.png"
                            fillMode: Image.PreserveAspectFit
                            mipmap: true
                            smooth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 8

                        FluentText {
                            objectName: "aboutProductName"
                            Layout.fillWidth: true
                            text: "Tweakopedia"
                            color: FluentTheme.textPrimary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                            horizontalAlignment: root.wideLayout ? Text.AlignLeft : Text.AlignHCenter
                        }

                        Rectangle {
                            Layout.preferredWidth: versionLabel.implicitWidth + 24
                            Layout.preferredHeight: 30
                            Layout.alignment: root.wideLayout ? Qt.AlignLeft : Qt.AlignHCenter
                            radius: 15
                            color: FluentTheme.selected

                            FluentText {
                                id: versionLabel
                                objectName: "aboutVersion"
                                anchors.centerIn: parent
                                text: "Версия 0.9.3 · x64 · Portable"
                                color: FluentTheme.accent
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }

                        FluentText {
                            objectName: "aboutDescription"
                            Layout.fillWidth: true
                            text: "Tweakopedia — офлайн-энциклопедия и твикер для Windows 10 и 11. Программа объясняет назначение системных параметров, собирает выбранные изменения в очередь, показывает итоговый план, сохраняет фактические исходные значения и позволяет вернуть их из истории."
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 14
                            lineHeight: 1.25
                            wrapMode: Text.WordWrap
                            horizontalAlignment: root.wideLayout ? Text.AlignLeft : Text.AlignHCenter
                        }
                    }
                }
            }

            FluentText {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 4
                text: "Сборка"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                columns: root.wideLayout ? 3 : 1
                columnSpacing: 12
                rowSpacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 132
                    radius: 10
                    color: FluentTheme.surface
                    border.color: FluentTheme.stroke

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 7
                        RowLayout {
                            spacing: 9
                            Text { text: "\uE943"; color: FluentTheme.accent; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20 }
                            FluentText { text: "Интерфейс и ядро"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: "C++20 · Qt 6.8.3\nQt Quick и QML"
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            lineHeight: 1.25
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 132
                    radius: 10
                    color: FluentTheme.surface
                    border.color: FluentTheme.stroke

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 7
                        RowLayout {
                            spacing: 9
                            Text { text: "\uE7F8"; color: FluentTheme.accent; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20 }
                            FluentText { text: "Формат выпуска"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: "Один EXE-контейнер\nЛокальный кэш Qt-runtime"
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            lineHeight: 1.25
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 132
                    radius: 10
                    color: FluentTheme.surface
                    border.color: FluentTheme.stroke

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 7
                        RowLayout {
                            spacing: 9
                            Text { text: "\uE950"; color: FluentTheme.accent; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20 }
                            FluentText { text: "Инструменты"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                        }
                        FluentText {
                            Layout.fillWidth: true
                            text: "MinGW 13.1.0 · CMake 3.30.5\nNinja 1.12.1"
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 13
                            lineHeight: 1.25
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: librariesContent.implicitHeight + 36
                radius: 10
                color: FluentTheme.surface
                border.color: FluentTheme.stroke

                ColumnLayout {
                    id: librariesContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 18
                    spacing: 8

                    RowLayout {
                        spacing: 9
                        Text { text: "\uE74C"; color: FluentTheme.accent; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20 }
                        FluentText { text: "Библиотеки и компоненты"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 18; font.weight: Font.DemiBold }
                    }

                    FluentText {
                        objectName: "aboutLibraries"
                        Layout.fillWidth: true
                        text: "Qt 6.8.3: Core, GUI, QML, Quick, Quick Controls 2, Concurrent, Network и SQL\nyaml-cpp 0.8.0 · nlohmann/json 3.12.0 · miniz 3.1.2 · SQLite через Qt SQL/QSQLITE\nSF Pro — встроенный шрифт интерфейса\nWin32 API: Registry/Advapi32, DXGI, DWM, COM/WMI, Secur32, CNG/Bcrypt, Shell32 и NTDLL Feature Store; AppX и PowerShell"
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        lineHeight: 1.35
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: creditsContent.implicitHeight + 36
                radius: 10
                color: FluentTheme.surface
                border.color: FluentTheme.stroke

                ColumnLayout {
                    id: creditsContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 18
                    spacing: 8

                    RowLayout {
                        spacing: 9
                        Text { text: "\uE734"; color: FluentTheme.accent; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20 }
                        FluentText { text: "Благодарности"; color: FluentTheme.textPrimary; font.family: FluentTheme.fontFamily; font.pixelSize: 18; font.weight: Font.DemiBold }
                    }

                    FluentText {
                        objectName: "aboutCredits"
                        Layout.fillWidth: true
                        text: "Спасибо авторам проектов, чьи исследования, документация и скрипты помогли сформировать каталог Tweakopedia:\n\nRaphire — Win11Debloat\nPlínio Larrubia / LeDragoX — Win-Debloat-Tools\nthebookisclosed — ViVe и ViVeTool\nSergey Tkachenko — Winaero Tweaker\n\nТакже спасибо W4RH4WK, Chris Titus Tech, Sycnex, kalaspuffar и matthewjberger — авторам работ, на которые опирался Win-Debloat-Tools."
                        color: FluentTheme.textSecondary
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                        lineHeight: 1.3
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                implicitHeight: independenceText.implicitHeight + 28
                radius: 10
                color: "#EEF6FF"
                border.color: "#C8DCF4"

                FluentText {
                    id: independenceText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    text: "Tweakopedia разработана самостоятельно. Перечисленные проекты использовались как источники идей, документации и проверяемых сведений о Windows; их упоминание не означает аффилированность."
                    color: FluentTheme.textSecondary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }
}
