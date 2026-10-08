import QtQuick
import "../style"

Row {
    id: root

    property string prefix
    property string stateId
    property string stateTitle

    spacing: 4

    Text {
        objectName: "statePrefixLabel"
        text: root.prefix
        color: FluentTheme.textSecondary
        font.family: FluentTheme.fontFamily
        font.pixelSize: 13
    }

    Text {
        objectName: "stateValueLabel"
        text: root.stateTitle
        color: root.stateId === "enabled"
            ? FluentTheme.stateOn
            : root.stateId === "disabled" ? FluentTheme.stateOff : FluentTheme.stateUnknown
        font.family: FluentTheme.fontFamily
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }
}
