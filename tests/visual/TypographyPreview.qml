import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/components"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia Typography"

    Rectangle {
        anchors.fill: parent
        anchors.margins: 32
        radius: 10
        color: FluentTheme.surface
        border.width: 1
        border.color: FluentTheme.stroke

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 16

            FluentText {
                text: Application.font.family
                color: FluentTheme.textPrimary
                font.pixelSize: 28
                font.weight: Font.Bold
            }
            FluentText {
                text: "Обычный текст: параметры Windows, Fluent UI и Tweakopedia 0.9.3"
                color: FluentTheme.textPrimary
                font.pixelSize: 17
                font.weight: Font.Normal
            }
            FluentText {
                text: "Полужирный текст: Кириллица · Latin · 0123456789"
                color: FluentTheme.textPrimary
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            FluentText {
                Layout.fillWidth: true
                text: "Проверка переноса длинной строки на обычных и HiDPI-дисплеях: одинаковые метрики строк без ручных поправок геометрии для отдельных масштабов."
                color: FluentTheme.textSecondary
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
            RowLayout {
                spacing: 20
                FluentText { text: "Regular"; font.pixelSize: 15; font.weight: Font.Normal }
                FluentText { text: "Medium"; font.pixelSize: 15; font.weight: Font.Medium }
                FluentText { text: "Semibold"; font.pixelSize: 15; font.weight: Font.DemiBold }
                FluentText { text: "Bold"; font.pixelSize: 15; font.weight: Font.Bold }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
