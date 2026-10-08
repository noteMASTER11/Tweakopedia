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
    property bool pending: false
    property bool supported: true
    property string supportDetails: ""
    property string impact: "low"
    property string restart: "none"
    property alias explanationButton: helpButton

    signal explanationRequested()
    signal targetSelected(string state)

    implicitHeight: Math.max(116, content.implicitHeight + 32)
    height: implicitHeight

    Rectangle {
        id: background
        objectName: "rowBackground"
        anchors.fill: parent
        radius: 8
        color: root.pending ? FluentTheme.selected : FluentTheme.surface
        border.width: 1
        border.color: root.pending ? FluentTheme.accent : FluentTheme.stroke
    }

    Rectangle {
        objectName: "pendingStripe"
        visible: root.pending
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 2
        color: FluentTheme.accent
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

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

                FluentBadge {
                    objectName: "changedBadge"
                    visible: root.pending
                    text: "Изменено"
                }
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
                prefix: "Сейчас:"
                stateId: root.currentState
                stateTitle: root.currentStateTitle
            }

            StateLabel {
                objectName: "targetStateLabel"
                visible: root.pending
                prefix: "После применения:"
                stateId: root.targetState
                stateTitle: root.targetStateTitle
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
            visible: !root.binary
            enabled: root.supported
            states: root.availableStates
            selectedState: root.pending ? root.targetState : root.currentState
            onStateSelected: state => root.targetSelected(state)
        }

        FluentToggle {
            objectName: "binaryToggle"
            visible: root.binary
            checked: root.pending
                ? root.targetState === "enabled"
                : root.currentState === "enabled"
            enabled: root.supported
                && (root.currentState === "disabled" || root.currentState === "enabled")
            onToggledByUser: checked => root.targetSelected(checked ? "enabled" : "disabled")
        }

        InfoButton {
            id: helpButton
            objectName: "explanationButton"
            onClicked: root.explanationRequested()
        }
    }
}
