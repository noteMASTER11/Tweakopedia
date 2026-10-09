import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../style"

Item {
    id: root

    required property var articleModel
    required property var controller
    property bool showContents: width >= 1000
    property bool singleColumnSections: !showContents
    property bool stackRelated: false
    property string activeSectionId: ""
    readonly property var articleData: articleModel && articleModel.article
        ? articleModel.article : ({})
    readonly property var compatibilityData: articleData.compatibility || ({})
    readonly property var restartData: articleData.restart || ({})
    signal sectionRequested(string sectionId)
    signal tweakRequested(string id)

    objectName: "articleReader"

    function scrollToSection(sectionId) {
        root.sectionRequested(sectionId)
        root.activeSectionId = sectionId
        for (let index = 0; index < sectionRepeater.count; ++index) {
            const item = sectionRepeater.itemAt(index)
            if (item && item.section.id === sectionId) {
                const flickable = articleScroll.contentItem
                const position = item.mapToItem(flickable.contentItem, 0, 0)
                flickable.contentY = Math.max(0, Math.min(position.y,
                    flickable.contentHeight - flickable.height))
                return
            }
        }
    }

    function updateActiveSection() {
        const flickable = articleScroll.contentItem
        if (!flickable || sectionRepeater.count === 0)
            return
        const marker = flickable.contentY + 32
        const firstItem = sectionRepeater.itemAt(0)
        let active = firstItem ? firstItem.section.id : ""
        for (let index = 0; index < sectionRepeater.count; ++index) {
            const item = sectionRepeater.itemAt(index)
            if (!item)
                continue
            const position = item.mapToItem(flickable.contentItem, 0, 0)
            if (position.y <= marker)
                active = item.section.id
            else
                break
        }
        if (active.length > 0)
            root.activeSectionId = active
    }

    onArticleDataChanged: Qt.callLater(updateActiveSection)
    Component.onCompleted: Qt.callLater(updateActiveSection)

    RowLayout {
        anchors.fill: parent
        spacing: 14

        ScrollView {
            id: articleScroll
            objectName: "articleScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: articleScroll.availableWidth
                spacing: 14

                FluentText {
                    Layout.fillWidth: true
                    text: (root.articleData.breadcrumbs || []).join("  ›  ")
                    color: FluentTheme.accent
                    wrapMode: Text.WordWrap
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    FluentText {
                        id: articleTitle
                        objectName: "articleTitle"
                        Layout.fillWidth: true
                        text: root.articleData.title || ""
                        color: FluentTheme.textPrimary
                        wrapMode: Text.WordWrap
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                    }

                    Button {
                        id: openTweakButton
                        objectName: "articleOpenTweakButton"
                        Layout.alignment: Qt.AlignTop
                        leftPadding: 14
                        rightPadding: 14
                        topPadding: 9
                        bottomPadding: 9
                        Accessible.name: "Перейти к твику: " + (root.articleData.title || "")
                        onClicked: root.tweakRequested(root.articleData.id || "")

                        contentItem: RowLayout {
                            spacing: 7

                            FluentText {
                                text: "Перейти к твику"
                                color: "white"
                                font.family: FluentTheme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: "\uE72A"
                                color: "white"
                                font.family: "Segoe MDL2 Assets"
                                font.pixelSize: 11
                            }
                        }

                        background: Rectangle {
                            radius: 6
                            color: openTweakButton.down
                                ? FluentTheme.accentPressed
                                : (openTweakButton.hovered
                                    ? FluentTheme.accentHover : FluentTheme.accent)
                        }
                    }

                    Button {
                        id: contentsButton
                        objectName: "articleContentsButton"
                        visible: !root.showContents
                            && (root.articleData.sections || []).length > 0
                        Layout.alignment: Qt.AlignTop
                        implicitWidth: 38
                        implicitHeight: 38
                        Accessible.name: "Открыть содержание статьи"
                        onClicked: contentsPopup.open()

                        contentItem: Text {
                            text: "\uE8FD"
                            color: FluentTheme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 15
                        }

                        background: Rectangle {
                            radius: 6
                            color: contentsButton.down ? FluentTheme.selected
                                : (contentsButton.hovered
                                    ? FluentTheme.hover : FluentTheme.surface)
                            border.width: contentsButton.activeFocus ? 2 : 1
                            border.color: contentsButton.activeFocus
                                ? FluentTheme.accent : FluentTheme.stroke
                        }
                    }
                }

                FluentText {
                    Layout.fillWidth: true
                    text: root.articleData.summary || ""
                    color: FluentTheme.textSecondary
                    wrapMode: Text.WordWrap
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 16
                    lineHeight: 1.2
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: (root.articleData.purpose || "").length > 0
                    implicitHeight: purposeText.implicitHeight + 28
                    radius: 8
                    color: "#EAF3FF"

                    FluentText {
                        id: purposeText
                        anchors.fill: parent
                        anchors.margins: 14
                        text: root.articleData.purpose || ""
                        color: FluentTheme.textPrimary
                        wrapMode: Text.WordWrap
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 14
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 50
                        radius: 8
                        color: FluentTheme.surface
                        border.width: 1
                        border.color: FluentTheme.stroke
                        FluentText {
                            anchors.centerIn: parent
                            text: (root.compatibilityData.operatingSystems || []).join(" · ")
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 50
                        radius: 8
                        color: FluentTheme.surface
                        border.width: 1
                        border.color: FluentTheme.stroke
                        FluentText {
                            anchors.centerIn: parent
                            text: "Перезапуск: " + (root.restartData.title || "Не требуется")
                            color: FluentTheme.textSecondary
                            font.family: FluentTheme.fontFamily
                            font.pixelSize: 12
                        }
                    }
                }

                GridLayout {
                    id: sectionsGrid
                    objectName: "articleSectionsGrid"
                    Layout.fillWidth: true
                    columns: root.singleColumnSections ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Repeater {
                        id: sectionRepeater
                        model: root.articleData.sections || []

                        delegate: ArticleSection {
                            required property var modelData
                            section: modelData
                            technicalObjects: root.articleData.technicalObjects || []
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.columnSpan: modelData.technical ? parent.columns : 1
                        }
                    }
                }

                FluentText {
                    Layout.fillWidth: true
                    visible: relatedRepeater.count > 0
                    text: "Связанные статьи"
                    color: FluentTheme.textPrimary
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }

                Flow {
                    id: relatedFlow
                    objectName: "relatedArticlesFlow"
                    Layout.fillWidth: true
                    Layout.preferredHeight: childrenRect.height
                    spacing: 12

                    Repeater {
                        id: relatedRepeater
                        model: root.articleData.relatedArticles || []

                        delegate: RelatedArticleCard {
                            required property var modelData
                            article: modelData
                            width: root.stackRelated
                                ? parent.width
                                : Math.max(190, Math.min(260,
                                    (parent.width - parent.spacing * 2) / 3))
                            onArticleRequested: id => root.controller.openArticle(id)
                        }
                    }
                }

                Item { Layout.fillWidth: true; implicitHeight: 18 }
            }

            Connections {
                target: articleScroll.contentItem
                function onContentYChanged() { root.updateActiveSection() }
            }
        }

        ArticleContents {
            objectName: "articleContentsPanel"
            Layout.preferredWidth: 210
            Layout.alignment: Qt.AlignTop
            visible: root.showContents
            sections: root.articleData.sections || []
            activeSectionId: root.activeSectionId
            onSectionRequested: sectionId => root.scrollToSection(sectionId)
        }
    }

    Popup {
        id: contentsPopup
        objectName: "articleContentsPopup"
        parent: root
        x: Math.max(0, root.width - width - 8)
        y: contentsButton.mapToItem(root, 0, contentsButton.height).y + 8
        width: Math.min(260, root.width - 16)
        height: Math.min(contentItem.implicitHeight, root.height - y - 8)
        padding: 0
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ArticleContents {
            objectName: "articleContentsPopupPanel"
            width: contentsPopup.width
            sections: root.articleData.sections || []
            activeSectionId: root.activeSectionId
            onSectionRequested: function(sectionId) {
                root.scrollToSection(sectionId)
                contentsPopup.close()
            }
        }
    }
}
