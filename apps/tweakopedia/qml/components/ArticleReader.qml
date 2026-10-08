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
    readonly property var articleData: articleModel && articleModel.article
        ? articleModel.article : ({})
    readonly property var compatibilityData: articleData.compatibility || ({})
    readonly property var restartData: articleData.restart || ({})
    signal sectionRequested(string sectionId)

    objectName: "articleReader"

    function scrollToSection(sectionId) {
        root.sectionRequested(sectionId)
        for (let index = 0; index < sectionRepeater.count; ++index) {
            const item = sectionRepeater.itemAt(index)
            if (item && item.section.id === sectionId) {
                const flickable = articleScroll.contentItem
                flickable.contentY = Math.max(0, Math.min(item.y,
                    flickable.contentHeight - flickable.height))
                return
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 14

        ScrollView {
            id: articleScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: articleScroll.availableWidth
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: (root.articleData.breadcrumbs || []).join("  ›  ")
                    color: FluentTheme.accent
                    wrapMode: Text.WordWrap
                    font.family: FluentTheme.fontFamily
                    font.pixelSize: 12
                }

                Text {
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

                Text {
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

                    Text {
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
                        Text {
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
                        Text {
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

                Text {
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
                    spacing: 10

                    Repeater {
                        id: relatedRepeater
                        model: root.articleData.relatedArticles || []

                        delegate: RelatedArticleCard {
                            required property var modelData
                            article: modelData
                            width: root.stackRelated
                                ? parent.width
                                : Math.max(190, Math.min(260, (parent.width - 20) / 3))
                            onArticleRequested: id => root.controller.openArticle(id)
                        }
                    }
                }

                Item { Layout.fillWidth: true; implicitHeight: 18 }
            }
        }

        ArticleContents {
            objectName: "articleContentsPanel"
            Layout.preferredWidth: 210
            Layout.alignment: Qt.AlignTop
            visible: root.showContents
            sections: root.articleData.sections || []
            onSectionRequested: sectionId => root.scrollToSection(sectionId)
        }
    }

    Button {
        id: contentsButton
        objectName: "articleContentsButton"
        anchors.top: parent.top
        anchors.right: parent.right
        z: 2
        visible: !root.showContents && (root.articleData.sections || []).length > 0
        text: "Содержание"
        Accessible.name: "Открыть содержание статьи"
        onClicked: contentsPopup.open()
    }

    Popup {
        id: contentsPopup
        objectName: "articleContentsPopup"
        parent: root
        x: Math.max(0, root.width - width - 8)
        y: contentsButton.height + 8
        width: Math.min(260, root.width - 16)
        height: Math.min(contentItem.implicitHeight, root.height - y - 8)
        padding: 0
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ArticleContents {
            objectName: "articleContentsPopupPanel"
            width: contentsPopup.width
            sections: root.articleData.sections || []
            onSectionRequested: function(sectionId) {
                root.scrollToSection(sectionId)
                contentsPopup.close()
            }
        }
    }
}
