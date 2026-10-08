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
            readonly property string fullTitleTooltip: ToolTip.text

            objectName: "encyclopediaNode_" + (model.id || row)
            implicitWidth: tree.width
            implicitHeight: model.nodeType === "article" ? 52 : 42
            leftMargin: 6
            rightMargin: 6
            indentation: 18
            leftPadding: leftMargin + depth * indentation + 24
            rightPadding: rightMargin
            focusPolicy: Qt.StrongFocus
            Accessible.role: model.nodeType === "article"
                ? Accessible.ListItem : Accessible.Button
            Accessible.name: model.title || ""
            Accessible.description: model.nodeType === "article"
                ? (model.path || "")
                : (expanded ? "Развёрнуто" : "Свёрнуто")
            ToolTip.text: model.title || ""
            ToolTip.visible: hovered && titleText.truncated
            ToolTip.delay: 500

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
                objectName: "encyclopediaIndicator_" + (treeDelegate.model.id || treeDelegate.row)
                x: treeDelegate.leftMargin + treeDelegate.depth * treeDelegate.indentation
                width: 20
                anchors.verticalCenter: parent.verticalCenter
                visible: treeDelegate.hasChildren
                text: treeDelegate.expanded ? "\uE96E" : "\uE970"
                color: FluentTheme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 12
            }

            contentItem: Column {
                leftPadding: 0
                rightPadding: 0
                spacing: 2

                Text {
                    id: titleText
                    objectName: "encyclopediaNodeTitle_" + (treeDelegate.model.id || treeDelegate.row)
                    width: parent.width
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
                    width: parent.width
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
