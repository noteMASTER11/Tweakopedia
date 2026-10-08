import QtQuick
import QtQuick.Controls.Basic
import "../style"

Item {
    id: root

    property var model
    property bool wide: true
    property string currentCategory: ""
    signal categorySelected(string categoryId)

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
            property string categoryId: model.id

            objectName: "categoryButton_" + categoryId
            width: root.wide ? ListView.view.width : implicitWidth
            height: 40
            leftPadding: 12
            rightPadding: 12
            text: model.title
            Accessible.name: text
            onClicked: {
                root.currentCategory = categoryId
                root.categorySelected(categoryId)
            }

            contentItem: Text {
                text: categoryButton.text
                color: FluentTheme.textPrimary
                elide: Text.ElideRight
                horizontalAlignment: root.wide ? Text.AlignLeft : Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                font.weight: categoryButton.categoryId === root.currentCategory
                    ? Font.DemiBold : Font.Normal
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
