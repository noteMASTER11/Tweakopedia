import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "pages"

ApplicationWindow {
    id: window
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "Tweakopedia"
    color: palette.window

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            Label {
                text: "Tweakopedia"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                Layout.rightMargin: 24
            }
            TabBar {
                id: tabs
                Layout.fillWidth: true
                TabButton { text: "Обзор" }
                TabButton { text: "Твики" }
                TabButton { text: "Справочник · позже"; enabled: false }
                TabButton { text: "Очередь" }
                TabButton { text: "История" }
                TabButton { text: "Диагностика · позже"; enabled: false }
                TabButton { text: "Настройки · позже"; enabled: false }
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex
        OverviewPage { controller: appController }
        TweaksPage { controller: appController }
        Item {}
        QueuePage { controller: appController }
        HistoryPage { controller: appController }
        Item {}
        Item {}
    }
}
