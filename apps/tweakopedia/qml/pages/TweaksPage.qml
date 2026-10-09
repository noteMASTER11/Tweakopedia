import QtQuick
import QtQuick.Controls.Basic
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    property bool wideLayout: width >= 1180
    readonly property string selectedCategory: controller.filteredTweaks.categoryId || ""
    readonly property bool appRemovalSelected: selectedCategory === "app-removal"
    readonly property bool appRemovalPromptVisible:
        appRemovalSelected && controller.appRemovalScanStatus !== "succeeded"
    signal reviewRequested()

    function showExplanation(tweakId, trigger) {
        infoPane.show(controller.openExplanation(tweakId), trigger)
    }

    function resetTweakListPosition() {
        Qt.callLater(function() {
            tweakList.positionViewAtBeginning()
            tweakList.currentIndex = -1
        })
    }

    function openTweak(tweakId) {
        const row = controller.revealTweak(tweakId)
        if (row < 0)
            return
        searchField.text = ""
        tweakList.positionViewAtIndex(row, ListView.Center)
        tweakList.currentIndex = row
        Qt.callLater(function() {
            const item = tweakList.itemAtIndex(row)
            if (!item)
                return
            item.forceActiveFocus()
            root.showExplanation(tweakId, item)
        })
    }

    background: Rectangle { color: FluentTheme.canvas }

    FluentText {
        id: pageTitle
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.top: parent.top
        anchors.topMargin: 20
        text: "Твики"
        color: FluentTheme.textPrimary
        font.family: FluentTheme.fontFamily
        font.pixelSize: 28
        font.weight: Font.DemiBold
    }

    FluentText {
        id: pageDescription
        anchors.left: pageTitle.left
        anchors.top: pageTitle.bottom
        anchors.topMargin: 6
        text: "Выбранные состояния сначала добавляются в очередь."
        color: FluentTheme.textSecondary
        font.family: FluentTheme.fontFamily
        font.pixelSize: 13
    }

    FluentSearchField {
        id: searchField
        objectName: "tweakSearchField"
        anchors.left: pageTitle.left
        anchors.right: parent.right
        anchors.rightMargin: 24 + (infoPane.opened && infoPane.docked ? infoPane.width : 0)
        anchors.top: pageDescription.bottom
        anchors.topMargin: 16
        visible: !root.appRemovalPromptVisible
        onSearchRequested: query => root.controller.setTweakSearch(query)
    }

    CategoryRail {
        id: categoryRail
        objectName: "categoryRail"
        anchors.left: pageTitle.left
        anchors.top: root.appRemovalPromptVisible ? pageDescription.bottom : searchField.bottom
        anchors.topMargin: 14
        width: root.wideLayout ? 220 : parent.width - 48
        height: root.wideLayout
            ? parent.height - y - 24
            : 44
        model: root.controller.categories
        wide: root.wideLayout
        currentCategory: root.selectedCategory
        onCategorySelected: categoryId => {
            root.controller.setTweakCategory(categoryId)
            root.resetTweakListPosition()
        }
    }

    TweakGroupStrip {
        id: tweakGroupStrip
        anchors.left: tweakList.left
        anchors.right: tweakList.right
        anchors.top: root.wideLayout ? categoryRail.top : categoryRail.bottom
        anchors.topMargin: root.wideLayout ? 0 : 8
        model: root.controller.tweakGroups
        currentGroup: root.controller.tweakGroups.selectedId || ""
        visible: root.selectedCategory !== ""
            && !root.appRemovalPromptVisible
            && hasGroups
        height: visible ? implicitHeight : 0
        onGroupSelected: groupId => {
            root.controller.setTweakSubcategory(groupId)
            root.resetTweakListPosition()
        }
        onStepRequested: delta => {
            root.controller.stepTweakSubcategory(delta)
            root.resetTweakListPosition()
        }
    }

    ListView {
        id: tweakList
        objectName: "tweakList"
        anchors.left: parent.left
        anchors.leftMargin: root.wideLayout ? 260 : 24
        anchors.right: parent.right
        anchors.rightMargin: 24 + (infoPane.opened && infoPane.docked ? infoPane.width : 0)
        anchors.top: root.wideLayout
            ? (tweakGroupStrip.visible ? tweakGroupStrip.bottom : categoryRail.top)
            : (tweakGroupStrip.visible ? tweakGroupStrip.bottom : categoryRail.bottom)
        anchors.topMargin: tweakGroupStrip.visible ? 8 : (root.wideLayout ? 0 : 14)
        anchors.bottom: queueBar.visible ? queueBar.top : parent.bottom
        anchors.bottomMargin: queueBar.visible ? 10 : 24
        clip: true
        spacing: 8
        model: root.controller.filteredTweaks
        visible: !root.appRemovalSelected
            || root.controller.appRemovalScanStatus === "succeeded"

        delegate: TweakRow {
            id: tweakRowDelegate
            objectName: "tweakRow_" + model.id
            width: ListView.view.width
            title: model.title
            summary: model.summary
            currentState: model.currentState
            currentStateTitle: model.currentStateTitle
            targetState: model.targetState
            targetStateTitle: model.targetStateTitle
            availableStates: model.availableStates
            binary: model.binary
            action: model.action
            pending: model.pending
            supported: model.supported
            supportDetails: model.supportDetails
            impact: model.impact
            restart: model.restart
            inputs: model.inputs
            onTargetSelected: state => root.controller.selectTarget(model.id, state)
            onParameterizedTargetSelected: (state, inputs) =>
                root.controller.selectParameterizedTarget(model.id, state, inputs)
            onExplanationRequested: root.showExplanation(model.id, tweakRowDelegate)
        }
    }

    Rectangle {
        id: appRemovalSearchPrompt
        objectName: "appRemovalSearchPrompt"
        anchors.left: tweakList.left
        anchors.right: tweakList.right
        anchors.top: tweakList.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        visible: root.appRemovalPromptVisible
        radius: 8
        color: FluentTheme.surface
        border.width: 1
        border.color: FluentTheme.stroke

        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 560)
            spacing: 16

            FluentText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "⌕"
                color: FluentTheme.accent
                font.family: FluentTheme.fontFamily
                font.pixelSize: 72
            }

            FluentText {
                width: parent.width
                text: "Нажмите на поиск, чтобы автоматически определить установленные приложения"
                color: FluentTheme.textPrimary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            FluentText {
                width: parent.width
                visible: root.controller.appRemovalScanStatus === "failed"
                text: root.controller.appRemovalScanError
                    || "Не удалось определить установленные приложения."
                color: FluentTheme.danger
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.family: FluentTheme.fontFamily
                font.pixelSize: 13
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                BusyIndicator {
                    id: appRemovalSearchSpinner
                    objectName: "appRemovalSearchSpinner"
                    width: 32
                    height: 32
                    visible: root.controller.appRemovalScanStatus === "running"
                    running: visible
                }

                Button {
                    id: appRemovalSearchButton
                    objectName: "appRemovalSearchButton"
                    text: root.controller.appRemovalScanStatus === "running"
                        ? "Поиск…" : "Поиск"
                    enabled: root.controller.appRemovalScanStatus !== "running"
                    leftPadding: 24
                    rightPadding: 24
                    topPadding: 9
                    bottomPadding: 9
                    onClicked: root.controller.scanInstalledApps()

                    contentItem: FluentText {
                        text: appRemovalSearchButton.text
                        color: appRemovalSearchButton.enabled ? "white" : FluentTheme.disabledText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    background: Rectangle {
                        radius: 5
                        color: appRemovalSearchButton.enabled
                            ? (appRemovalSearchButton.hovered
                                ? FluentTheme.accentHover : FluentTheme.accent)
                            : FluentTheme.disabledSurface
                    }
                }
            }
        }
    }

    QueueCommandBar {
        id: queueBar
        objectName: "queueCommandBar"
        anchors.left: tweakList.left
        anchors.right: tweakList.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        height: implicitHeight
        queueCount: root.controller.queue.count
        onReviewRequested: root.reviewRequested()
    }

    InfoPane {
        id: infoPane
        objectName: "infoPane"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 420
        docked: root.wideLayout
    }
}
