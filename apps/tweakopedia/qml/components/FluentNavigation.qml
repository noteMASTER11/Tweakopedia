import QtQuick
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property real availableWidth: FluentTheme.compactBreakpoint
    property int currentIndex: 0
    property int queueCount: 0
    readonly property bool compact: availableWidth < FluentTheme.compactBreakpoint
    readonly property int effectiveWidth: compact
        ? FluentTheme.navigationCompactWidth
        : FluentTheme.navigationExpandedWidth
    signal destinationRequested(int index)

    implicitWidth: effectiveWidth
    width: effectiveWidth
    Layout.minimumWidth: effectiveWidth
    Layout.preferredWidth: effectiveWidth
    Layout.maximumWidth: effectiveWidth
    color: FluentTheme.surfaceInset
    border.width: 1
    border.color: FluentTheme.stroke

    readonly property var primaryDestinations: [
        {title: "Обзор", glyph: "⌂"},
        {title: "Твики", glyph: "≡"},
        {title: "Справочник", glyph: "i"},
        {title: "Очередь", glyph: "≣"},
        {title: "История", glyph: "↶"}
    ]

    Text {
        id: brand
        anchors.top: parent.top
        anchors.topMargin: 18
        anchors.left: parent.left
        anchors.right: parent.right
        height: 32
        visible: !root.compact
        text: "Tweakopedia"
        color: FluentTheme.textPrimary
        font.family: FluentTheme.fontFamily
        font.pixelSize: 18
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignLeft
        leftPadding: 16
    }

    Column {
        anchors.top: root.compact ? parent.top : brand.bottom
        anchors.topMargin: root.compact ? 12 : 16
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

    FluentNavigationItem {
        objectName: "navItem_5"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        title: "Настройки"
        glyph: "⚙"
        compact: root.compact
        selected: root.currentIndex === 5
        destinationIndex: 5
        onClicked: {
            root.currentIndex = 5
            root.destinationRequested(5)
        }
    }
}
