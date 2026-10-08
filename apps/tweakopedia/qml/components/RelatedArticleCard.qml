import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

AbstractButton {
    id: root

    required property var article
    signal articleRequested(string id)

    objectName: "relatedArticle_" + article.id
    implicitWidth: 210
    implicitHeight: 112
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: "Открыть статью: " + article.title
    onClicked: articleRequested(article.id)

    contentItem: ColumnLayout {
        spacing: 5

        Text {
            Layout.fillWidth: true
            text: root.article.title
            color: FluentTheme.textPrimary
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.family: FluentTheme.fontFamily
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: root.article.summary || root.article.path || ""
            color: FluentTheme.textSecondary
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.family: FluentTheme.fontFamily
            font.pixelSize: 12
        }

        Item { Layout.fillHeight: true }

        Text {
            text: "Открыть  ›"
            color: FluentTheme.accent
            font.family: FluentTheme.fontFamily
            font.pixelSize: 12
        }
    }

    background: Rectangle {
        radius: 8
        color: root.hovered ? "#F5F9FF" : FluentTheme.surface
        border.width: root.hovered || root.activeFocus ? 2 : 1
        border.color: root.hovered || root.activeFocus
            ? FluentTheme.accent : FluentTheme.stroke
    }
}
