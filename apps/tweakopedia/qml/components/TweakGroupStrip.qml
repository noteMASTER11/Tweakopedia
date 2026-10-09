import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root
    objectName: "tweakGroupStrip"

    property var model
    property string currentGroup: ""
    readonly property int currentIndex: groupList.currentIndex
    signal groupSelected(string groupId)
    signal stepRequested(int delta)

    implicitHeight: 40

    function selectVisibleIndex(index) {
        if (!groupList || index < 0 || index >= groupList.count)
            return
        groupList.currentIndex = index
        Qt.callLater(function() {
            if (!groupList || index < 0 || index >= groupList.count)
                return
            groupList.forceLayout()
            groupList.positionViewAtIndex(index, ListView.Contain)
            const item = groupList.itemAtIndex(index)
            if (!item)
                return
            const maximum = Math.max(0, groupList.contentWidth - groupList.width)
            if (item.x < groupList.contentX)
                groupList.contentX = Math.max(0, item.x)
            else if (item.x + item.width > groupList.contentX + groupList.width)
                groupList.contentX = Math.min(maximum, item.x + item.width - groupList.width)
        })
    }

    function synchronizeCurrentIndex() {
        if (!groupTracker || !groupList)
            return
        for (let index = 0; index < groupTracker.count; ++index) {
            const item = groupTracker.objectAt(index)
            if (item && item.groupId === root.currentGroup) {
                selectVisibleIndex(index)
                return
            }
        }
        groupList.currentIndex = -1
    }

    onCurrentGroupChanged: synchronizeCurrentIndex()
    onModelChanged: Qt.callLater(synchronizeCurrentIndex)

    Instantiator {
        id: groupTracker
        model: root.model
        delegate: QtObject {
            readonly property int modelIndex: index
            readonly property string groupId: model.id !== undefined ? model.id : ""
        }
        onObjectAdded: function(index, object) {
            if (object.groupId === root.currentGroup)
                root.selectVisibleIndex(index)
        }
        onObjectRemoved: Qt.callLater(root.synchronizeCurrentIndex)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 8

        Button {
            id: previousButton
            objectName: "groupPreviousButton"
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            enabled: root.currentIndex > 0
            Accessible.name: "Предыдущая группа"
            onClicked: root.stepRequested(-1)

            contentItem: Text {
                text: "\uE76B"
                color: previousButton.enabled
                    ? FluentTheme.textPrimary : FluentTheme.disabledText
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: previousButton.width / 2
                color: previousButton.down ? FluentTheme.selected
                    : previousButton.hovered ? FluentTheme.hover : FluentTheme.surface
                border.width: 1
                border.color: FluentTheme.stroke
            }
        }

        ListView {
            id: groupList
            objectName: "groupHorizontalList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.model
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: -1
            highlightMoveDuration: 120

            delegate: Button {
                id: tab
                readonly property string groupId: model.id !== undefined ? model.id : ""
                readonly property string groupTitle: model.title !== undefined ? model.title : ""
                readonly property string fullTitleTooltip: groupTitle
                objectName: "groupTab_" + groupId
                width: Math.max(72, Math.min(220, label.implicitWidth + 28))
                height: 36
                enabled: model.enabled !== undefined ? model.enabled : true
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                text: groupTitle
                Accessible.name: groupTitle
                onClicked: root.groupSelected(groupId)

                contentItem: FluentText {
                    id: label
                    objectName: "groupTabLabel_" + tab.groupId
                    text: tab.text
                    color: tab.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    font.weight: tab.groupId === root.currentGroup ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: tab.groupId === root.currentGroup ? FluentTheme.selected
                        : tab.down ? FluentTheme.selected
                        : tab.hovered ? FluentTheme.hover : FluentTheme.surface
                    border.width: 1
                    border.color: tab.groupId === root.currentGroup
                        ? FluentTheme.accent : FluentTheme.stroke
                }

                ToolTip.visible: tab.hovered && label.truncated
                ToolTip.text: tab.fullTitleTooltip
                ToolTip.delay: 500
            }

            WheelHandler {
                parent: root
                target: null
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {
                    const rawDelta = event.pixelDelta.y !== 0
                        ? event.pixelDelta.y : event.angleDelta.y / 2
                    const maximum = Math.max(0, groupList.contentWidth - groupList.width)
                    groupList.contentX = Math.max(
                        0, Math.min(maximum, groupList.contentX - rawDelta))
                    event.accepted = true
                }
            }
        }

        Button {
            id: nextButton
            objectName: "groupNextButton"
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            enabled: root.currentIndex >= 0 && root.currentIndex < groupList.count - 1
            Accessible.name: "Следующая группа"
            onClicked: root.stepRequested(1)

            contentItem: Text {
                text: "\uE76C"
                color: nextButton.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: nextButton.width / 2
                color: nextButton.down ? FluentTheme.selected
                    : nextButton.hovered ? FluentTheme.hover : FluentTheme.surface
                border.width: 1
                border.color: FluentTheme.stroke
            }
        }
    }
}
