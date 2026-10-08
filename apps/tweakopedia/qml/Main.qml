import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "components"
import "pages"
import "style"

ApplicationWindow {
    id: window

    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "Tweakopedia"
    color: FluentTheme.canvas

    function openQueue() {
        navigation.currentIndex = 3
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        FluentNavigation {
            id: navigation
            Layout.fillHeight: true
            availableWidth: window.width
            queueCount: appController.queue.count
            hideUnsupportedTweaks: appController.filteredTweaks.hideUnsupported
            onHideUnsupportedTweaksRequested: function(hide) {
                appController.setHideUnsupportedTweaks(hide)
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: navigation.currentIndex

            OverviewPage { controller: appController }
            TweaksPage {
                id: tweaksPage
                controller: appController
                wideLayout: window.width >= 1180
                onReviewRequested: window.openQueue()
            }
            TweakopediaPage {
                controller: appController
                availableWindowWidth: window.width
                onTweakRequested: function(tweakId) {
                    navigation.currentIndex = 1
                    tweaksPage.openTweak(tweakId)
                }
            }
            QueuePage {
                controller: appController
                wideLayout: window.width >= 1180
            }
            HistoryPage { controller: appController }
            PlaceholderPage {
                pageTitle: "Настройки"
                description: "Здесь появятся параметры интерфейса, журналов и хранения локальных данных."
            }
        }
    }
}
