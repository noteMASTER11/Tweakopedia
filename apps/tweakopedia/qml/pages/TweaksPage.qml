import QtQuick
import QtQuick.Controls.Basic
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    property bool wideLayout: width >= 1180
    signal reviewRequested()

    function showExplanation(tweakId, trigger) {
        infoPane.show(controller.openExplanation(tweakId), trigger)
    }

    background: Rectangle { color: FluentTheme.canvas }

    Text {
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

    Text {
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
        onSearchRequested: query => root.controller.setTweakSearch(query)
    }

    CategoryRail {
        id: categoryRail
        objectName: "categoryRail"
        anchors.left: pageTitle.left
        anchors.top: searchField.bottom
        anchors.topMargin: 14
        width: root.wideLayout ? 220 : parent.width - 48
        height: root.wideLayout
            ? parent.height - y - 24
            : 44
        model: root.controller.categories
        wide: root.wideLayout
        onCategorySelected: categoryId => root.controller.setTweakCategory(categoryId)
    }

    ListView {
        id: tweakList
        objectName: "tweakList"
        anchors.left: parent.left
        anchors.leftMargin: root.wideLayout ? 260 : 24
        anchors.right: parent.right
        anchors.rightMargin: 24 + (infoPane.opened && infoPane.docked ? infoPane.width : 0)
        anchors.top: root.wideLayout ? searchField.bottom : categoryRail.bottom
        anchors.topMargin: 14
        anchors.bottom: queueBar.visible ? queueBar.top : parent.bottom
        anchors.bottomMargin: queueBar.visible ? 10 : 24
        clip: true
        spacing: 8
        model: root.controller.filteredTweaks

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
            pending: model.pending
            supported: model.supported
            supportDetails: model.supportDetails
            impact: model.impact
            restart: model.restart
            onTargetSelected: state => root.controller.selectTarget(model.id, state)
            onExplanationRequested: root.showExplanation(model.id, tweakRowDelegate)
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
