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
    visible: !startupManaged
    title: "Tweakopedia"
    color: FluentTheme.canvas

    function openQueue() {
        navigation.currentIndex = 3
    }

    function applyAppearance() {
        FluentTheme.configure(settingsController.effectiveThemeId,
                              settingsController.accentId,
                              settingsController.uiScale,
                              settingsController.reduceMotion)
    }

    Component.onCompleted: applyAppearance()

    Connections {
        target: settingsController
        function onAppearanceChanged() { window.applyAppearance() }
    }

    Item {
        id: contentRoot
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            spacing: 0

            FluentNavigation {
                id: navigation
                Layout.fillHeight: true
                availableWidth: contentRoot.width
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
                    wideLayout: contentRoot.width >= 1180
                    onReviewRequested: window.openQueue()
                }
                TweakopediaPage {
                    controller: appController
                    availableWindowWidth: contentRoot.width
                    onTweakRequested: function(tweakId) {
                        navigation.currentIndex = 1
                        tweaksPage.openTweak(tweakId)
                    }
                }
                QueuePage {
                    controller: appController
                    wideLayout: contentRoot.width >= 1180
                    onReturnToTweaksRequested: navigation.currentIndex = 1
                }
                HistoryPage { controller: appController }
                ToolsPage {
                    controller: appController
                    integrityController: systemIntegrityController
                    fileSystemController: fileSystemCheckController
                    networkController: networkDiagnosticsController
                    onArticleRequested: function(articleId) {
                        appController.encyclopedia.openArticle(articleId)
                        navigation.currentIndex = 2
                    }
                    onReviewRequested: window.openQueue()
                    onTweakRequested: function(tweakId) {
                        navigation.currentIndex = 1
                        tweaksPage.openTweak(tweakId)
                    }
                }
                SettingsPage { controller: settingsController }
                AboutPage {}
            }
        }
    }
}
