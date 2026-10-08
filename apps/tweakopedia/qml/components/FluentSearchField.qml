import QtQuick
import QtQuick.Controls.Basic
import "../style"

TextField {
    id: root

    signal searchRequested(string query)

    placeholderText: "Поиск по названию и описанию"
    selectByMouse: true
    leftPadding: 36
    rightPadding: 12
    implicitHeight: 38
    color: FluentTheme.textPrimary
    placeholderTextColor: FluentTheme.textSecondary
    font.family: FluentTheme.fontFamily
    font.pixelSize: 14
    onTextEdited: searchRequested(text)

    Text {
        objectName: "searchFieldGlyph"
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "\uE721"
        color: FluentTheme.textSecondary
        font.family: "Segoe MDL2 Assets"
        font.pixelSize: 15
    }

    background: Rectangle {
        radius: 5
        color: FluentTheme.surface
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? FluentTheme.accent : FluentTheme.stroke
    }
}
