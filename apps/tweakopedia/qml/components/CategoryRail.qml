import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root

    property var model
    property bool wide: true
    property string currentCategory: ""
    signal categorySelected(string categoryId)

    function categoryGlyph(categoryId) {
        const glyphs = {
            "": "\uE8FD",
            "behavior": "\uE8AB",
            "boot": "\uE7E8",
            "accounts": "\uE77B",
            "desktop": "\uE7F4",
            "privacy": "\uE72E",
            "filesystem": "\uE8B7",
            "network": "\uE774",
            "apps": "\uECAA",
            "app-removal": "\uE74D",
            "devices": "\uE772",
            "experimental": "\uE943",
            "gaming": "\uE7FC",
            "power": "\uE945",
            "updates": "\uE895"
        }
        return glyphs[categoryId] || "\uE8A5"
    }

    implicitWidth: wide ? 220 : 400
    implicitHeight: wide ? 400 : 44

    ListView {
        id: categoryList
        anchors.fill: parent
        model: root.model
        orientation: root.wide ? ListView.Vertical : ListView.Horizontal
        spacing: 4
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        delegate: Button {
            id: categoryButton
            property string categoryId: model.id !== undefined ? model.id : model.categoryId
            property string categoryTitle: model.title !== undefined ? model.title : model.categoryTitle

            objectName: "categoryButton_" + categoryId
            width: root.wide ? ListView.view.width : implicitWidth
            height: 40
            leftPadding: 12
            rightPadding: 12
            text: categoryTitle
            Accessible.name: text
            onClicked: {
                root.currentCategory = categoryId
                root.categorySelected(categoryId)
            }

            contentItem: RowLayout {
                spacing: 10

                Text {
                    objectName: "categoryIcon_" + categoryButton.categoryId
                    Layout.preferredWidth: 20
                    text: root.categoryGlyph(categoryButton.categoryId)
                    color: categoryButton.categoryId === root.currentCategory
                        ? FluentTheme.accent : FluentTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 16
                }

                FluentText {
                    Layout.fillWidth: true
                    text: categoryButton.text
                    color: FluentTheme.textPrimary
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    font.weight: categoryButton.categoryId === root.currentCategory
                        ? Font.DemiBold : Font.Normal
                }
            }

            background: Rectangle {
                radius: 5
                color: categoryButton.categoryId === root.currentCategory
                    ? FluentTheme.selected
                    : categoryButton.hovered ? FluentTheme.hover : "transparent"
            }
        }
    }
}
