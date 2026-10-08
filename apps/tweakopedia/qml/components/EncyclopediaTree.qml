import QtQuick
import QtQuick.Controls.Basic
import "../style"

FocusScope {
    id: root

    property var model
    signal articleRequested(string id)

    Rectangle {
        anchors.fill: parent
        color: FluentTheme.surface
        radius: 10
        border.width: 1
        border.color: FluentTheme.stroke
    }

    TreeView {
        id: tree
        objectName: "encyclopediaTree"
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        model: root.model
        selectionBehavior: TableView.SelectionDisabled
        boundsBehavior: Flickable.StopAtBounds

        delegate: TreeViewDelegate {
            id: treeDelegate

            objectName: "encyclopediaNode_" + (model.id || row)
            implicitHeight: model.nodeType === "article" ? 52 : 42
            leftMargin: 6
            rightMargin: 6
            indentation: 18
            focusPolicy: Qt.StrongFocus
            Accessible.role: model.nodeType === "article"
                ? Accessible.ListItem : Accessible.Button
            Accessible.name: model.title || ""
            Accessible.description: model.path || ""

            onClicked: {
                if (model.nodeType === "article")
                    root.articleRequested(model.id)
                else
                    tree.toggleExpanded(row)
            }
            Keys.onReturnPressed: event => {
                clicked()
                event.accepted = true
            }
            Component.onCompleted: {
                if (model.expanded && hasChildren && !expanded)
                    tree.expand(row)
            }

            indicator: Text {
                x: treeDelegate.leftMargin + treeDelegate.depth * treeDelegate.indentation
                anchors.verticalCenter: parent.verticalCenter
                visible: treeDelegate.hasChildren
                text: treeDelegate.expanded ? "⌄" : "›"
                color: FluentTheme.textSecondary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 16
            }

            contentItem: Column {
                leftPadding: treeDelegate.__contentIndent
                spacing: 2

                Text {
                    width: parent.width - parent.leftPadding
                    text: treeDelegate.model.title || ""
                    color: treeDelegate.model.selected
                        ? FluentTheme.accent : FluentTheme.textPrimary
                    elide: Text.ElideRight
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: treeDelegate.model.nodeType === "category" ? 14 : 13
                    font.weight: treeDelegate.model.nodeType === "article"
                        ? Font.Normal : Font.DemiBold
                }

                Text {
                    width: parent.width - parent.leftPadding
                    visible: treeDelegate.model.nodeType === "article"
                        && (treeDelegate.model.summary || "").length > 0
                    text: treeDelegate.model.summary || ""
                    color: FluentTheme.textSecondary
                    elide: Text.ElideRight
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 11
                }
            }

            background: Rectangle {
                radius: 6
                color: treeDelegate.model.selected
                    ? FluentTheme.selected
                    : (treeDelegate.hovered ? FluentTheme.hover : "transparent")
                border.width: treeDelegate.activeFocus ? 1 : 0
                border.color: FluentTheme.accent
            }
        }
    }
}
