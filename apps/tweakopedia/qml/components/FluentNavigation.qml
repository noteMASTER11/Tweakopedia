import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property real availableWidth: FluentTheme.compactBreakpoint
    property int currentIndex: 0
    property int queueCount: 0
    property bool hideUnsupportedTweaks: false
    readonly property bool compact: availableWidth < FluentTheme.compactBreakpoint
    readonly property int effectiveWidth: compact
        ? FluentTheme.navigationCompactWidth
        : FluentTheme.navigationExpandedWidth
    signal destinationRequested(int index)
    signal hideUnsupportedTweaksRequested(bool hide)

    implicitWidth: effectiveWidth
    width: effectiveWidth
    Layout.minimumWidth: effectiveWidth
    Layout.preferredWidth: effectiveWidth
    Layout.maximumWidth: effectiveWidth
    color: FluentTheme.surfaceInset
    border.width: 1
    border.color: FluentTheme.stroke

    readonly property var primaryDestinations: [
        {title: "Обзор", glyph: "\uE80F"},
        {title: "Твики", glyph: "\uE8AB"},
        {title: "Твикопедия", glyph: "\uE736"},
        {title: "Очередь", glyph: "\uE8FD"},
        {title: "История", glyph: "\uE81C"}
    ]

    Image {
        id: brandLogo
        objectName: "navigationBrandLogo"
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 62
        visible: !root.compact
        source: "qrc:/images/tweakopedia-logo.png"
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignVCenter
        mipmap: true
        smooth: true
        Accessible.name: "Tweakopedia"
        Accessible.role: Accessible.Graphic
    }

    Image {
        id: brandIcon
        objectName: "navigationBrandIcon"
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: 44
        height: 44
        visible: root.compact
        source: "qrc:/images/tweakopedia-icon.png"
        fillMode: Image.PreserveAspectFit
        mipmap: true
        smooth: true
        Accessible.name: "Tweakopedia"
        Accessible.role: Accessible.Graphic
    }

    Column {
        anchors.top: root.compact ? brandIcon.bottom : brandLogo.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 2

        Repeater {
            model: root.primaryDestinations

            delegate: FluentNavigationItem {
                required property int index
                required property var modelData

                objectName: "navItem_" + index
                width: root.width
                title: modelData.title
                glyph: modelData.glyph
                compact: root.compact
                selected: root.currentIndex === index
                destinationIndex: index
                badgeCount: index === 3 ? root.queueCount : 0
                onClicked: {
                    root.currentIndex = index
                    root.destinationRequested(index)
                }
            }
        }
    }

    AbstractButton {
        id: unsupportedTweaksCheckbox
        objectName: "unsupportedTweaksCheckbox"
        visible: !root.compact
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: aboutItem.top
        anchors.bottomMargin: 8
        height: 58
        checked: root.hideUnsupportedTweaks
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.CheckBox
        Accessible.name: "Скрыть неподдерживаемые твики"
        Accessible.checked: root.hideUnsupportedTweaks
        onClicked: root.hideUnsupportedTweaksRequested(!root.hideUnsupportedTweaks)

        contentItem: Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Rectangle {
                width: 18
                height: 18
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: root.hideUnsupportedTweaks ? FluentTheme.accent : FluentTheme.surface
                border.width: 1
                border.color: root.hideUnsupportedTweaks
                    ? FluentTheme.accent
                    : FluentTheme.textSecondary

                Text {
                    anchors.centerIn: parent
                    visible: root.hideUnsupportedTweaks
                    text: "✓"
                    color: FluentTheme.surface
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Text {
                width: parent.width - 28
                anchors.verticalCenter: parent.verticalCenter
                text: "Скрыть неподдерживаемые твики"
                color: FluentTheme.textPrimary
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        background: Rectangle {
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            radius: 6
            color: unsupportedTweaksCheckbox.hovered ? FluentTheme.hover : "transparent"
        }
    }

    AbstractButton {
        id: unsupportedTweaksCompactButton
        objectName: "unsupportedTweaksCompactButton"
        visible: root.compact
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: aboutItem.top
        anchors.bottomMargin: 8
        width: 44
        height: 44
        checked: root.hideUnsupportedTweaks
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.CheckBox
        Accessible.name: root.hideUnsupportedTweaks
            ? "Показывать неподдерживаемые твики"
            : "Скрыть неподдерживаемые твики"
        Accessible.checked: root.hideUnsupportedTweaks
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: root.hideUnsupportedTweaksRequested(!root.hideUnsupportedTweaks)

        contentItem: Text {
            text: "\uE71C"
            color: root.hideUnsupportedTweaks ? FluentTheme.accent : FluentTheme.textPrimary
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 18
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 6
            color: unsupportedTweaksCompactButton.hovered || root.hideUnsupportedTweaks
                ? FluentTheme.selected
                : "transparent"
        }
    }

    FluentNavigationItem {
        id: aboutItem
        objectName: "navItem_5"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        title: "О программе"
        glyph: "\uE946"
        compact: root.compact
        selected: root.currentIndex === 5
        destinationIndex: 5
        onClicked: {
            root.currentIndex = 5
            root.destinationRequested(5)
        }
    }
}
