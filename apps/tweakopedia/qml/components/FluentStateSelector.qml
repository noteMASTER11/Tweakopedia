import QtQuick
import QtQuick.Controls.Basic
import "../style"

ComboBox {
    id: root
    objectName: "stateComboBox"

    property var states: []
    property string selectedState
    signal stateSelected(string stateId)

    model: states
    textRole: "title"
    implicitWidth: 260
    implicitHeight: 36
    leftPadding: 12
    rightPadding: 36
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: "Выбор состояния"

    function indexOfState(stateId) {
        for (let index = 0; index < states.length; ++index) {
            if (states[index].id === stateId)
                return index
        }
        return -1
    }

    function syncSelection() {
        currentIndex = indexOfState(selectedState)
    }

    onSelectedStateChanged: syncSelection()
    onStatesChanged: syncSelection()
    onActivated: index => stateSelected(states[index].id)
    Component.onCompleted: syncSelection()

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: root.currentIndex >= 0
            ? root.states[root.currentIndex].title
            : "Выберите значение"
        color: root.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        font.family: FluentTheme.fontFamily
        font.pixelSize: 13
    }

    indicator: Canvas {
        x: root.width - width - 12
        y: (root.height - height) / 2
        width: 12
        height: 8
        contextType: "2d"

        onPaint: {
            context.reset()
            context.strokeStyle = root.enabled
                ? FluentTheme.textSecondary
                : FluentTheme.disabledText
            context.lineWidth = 1.5
            context.beginPath()
            context.moveTo(2, 2)
            context.lineTo(6, 6)
            context.lineTo(10, 2)
            context.stroke()
        }
    }

    background: Rectangle {
        radius: 5
        color: !root.enabled
            ? FluentTheme.disabledSurface
            : root.down || root.popup.visible
                ? FluentTheme.selected
                : root.hovered ? FluentTheme.hover : FluentTheme.surface
        border.width: root.activeFocus || root.popup.visible ? 2 : 1
        border.color: root.activeFocus || root.popup.visible
            ? FluentTheme.accent
            : FluentTheme.stroke
    }

    delegate: ItemDelegate {
        id: option
        required property var modelData
        required property int index

        objectName: "stateOption_" + modelData.id
        width: ListView.view ? ListView.view.width : root.popup.width
        height: 36
        leftPadding: 12
        rightPadding: 12
        highlighted: root.currentIndex === index
        hoverEnabled: true
        Accessible.name: modelData.title

        contentItem: Text {
            text: option.modelData.title
            color: option.enabled ? FluentTheme.textPrimary : FluentTheme.disabledText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            font.family: FluentTheme.fontFamily
            font.pixelSize: 13
            font.weight: option.highlighted ? Font.DemiBold : Font.Normal
        }

        background: Rectangle {
            radius: 4
            color: option.highlighted
                ? FluentTheme.selected
                : option.hovered ? FluentTheme.hover : "transparent"
        }

        onClicked: {
            root.currentIndex = index
            root.popup.close()
            root.activated(index)
        }
    }

    popup: Popup {
        y: root.height + 4
        width: Math.max(root.width, 320)
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4

        contentItem: ListView {
            implicitHeight: contentHeight
            clip: true
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: 6
            color: FluentTheme.surface
            border.width: 1
            border.color: FluentTheme.stroke
        }
    }
}
