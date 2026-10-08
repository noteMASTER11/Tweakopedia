import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root

    property string title: ""
    property string summary: ""
    property string currentState: ""
    property string currentStateTitle: ""
    property string targetState: ""
    property string targetStateTitle: ""
    property var availableStates: []
    property bool binary: false
    property bool action: false
    property bool pending: false
    property bool supported: true
    property string supportDetails: ""
    property string impact: "low"
    property string restart: "none"

    signal explanationRequested()
    signal targetSelected(string state)

    implicitHeight: Math.max(116, content.implicitHeight + 32)
    height: implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: "Открыть справку: " + title

    function openExplanation() {
        forceActiveFocus()
        explanationRequested()
    }

    Keys.onReturnPressed: event => {
        openExplanation()
        event.accepted = true
    }
    Keys.onEnterPressed: event => {
        openExplanation()
        event.accepted = true
    }
    Keys.onSpacePressed: event => {
        openExplanation()
        event.accepted = true
    }

    Rectangle {
        id: background
        objectName: "rowBackground"
        anchors.fill: parent
        radius: 8
        color: FluentTheme.surface
        border.width: 1
        border.color: FluentTheme.stroke
    }

    Rectangle {
        id: pendingGradient
        objectName: "pendingGradient"
        property real fadeEnd: 0.5
        visible: root.pending
        anchors.fill: parent
        radius: 8
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.0, 0.404, 0.753, 0.18) }
            GradientStop { position: pendingGradient.fadeEnd; color: "transparent" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Rectangle {
        objectName: "accentBorder"
        visible: root.pending || cardHover.hovered
        anchors.fill: parent
        radius: 8
        color: "transparent"
        border.width: 1
        border.color: FluentTheme.accent
    }

    MouseArea {
        objectName: "cardClickArea"
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.openExplanation()
    }

    HoverHandler {
        id: cardHover
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                objectName: "titleLabel"
                Layout.fillWidth: true
                text: root.title
                color: FluentTheme.textPrimary
                elide: Text.ElideRight
                font.family: FluentTheme.fontFamily
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: root.summary
                color: FluentTheme.textSecondary
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }

            StateLabel {
                objectName: "currentStateLabel"
                visible: !root.action
                prefix: root.pending ? "После применения:" : "Сейчас:"
                stateId: root.pending ? root.targetState : root.currentState
                stateTitle: root.binary
                    ? (root.pending ? root.targetStateTitle : root.currentStateTitle).toLocaleLowerCase()
                    : (root.pending ? root.targetStateTitle : root.currentStateTitle)
            }

            Text {
                objectName: "supportDetailsLabel"
                visible: text.length > 0
                    && (!root.supported
                        || (root.currentState !== "disabled" && root.currentState !== "enabled"))
                Layout.fillWidth: true
                text: root.supportDetails
                color: FluentTheme.disabledText
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 12
            }
        }

        FluentStateSelector {
            objectName: "stateSelector"
            visible: !root.binary && !root.action
            enabled: root.supported
            states: root.availableStates
            selectedState: root.pending ? root.targetState : root.currentState
            onStateSelected: state => root.targetSelected(state)
        }

        FluentToggle {
            objectName: "binaryToggle"
            visible: root.binary && !root.action
            checked: root.pending
                ? root.targetState === "enabled"
                : root.currentState === "enabled"
            enabled: root.supported
                && (root.currentState === "disabled" || root.currentState === "enabled")
            onToggledByUser: checked => root.targetSelected(checked ? "enabled" : "disabled")
        }

        Button {
            id: actionButton
            objectName: "actionQueueButton"
            visible: root.action
            enabled: root.supported && !root.pending
            text: root.pending ? "Добавлено" : "В очередь"
            implicitWidth: 124
            implicitHeight: 36
            hoverEnabled: true
            onClicked: root.targetSelected("remove")

            contentItem: Text {
                text: actionButton.text
                color: actionButton.enabled ? "white" : FluentTheme.disabledText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            background: Rectangle {
                radius: 5
                color: !actionButton.enabled
                    ? FluentTheme.disabledSurface
                    : actionButton.hovered ? FluentTheme.accentHover : FluentTheme.accent
            }
        }

    }
}
