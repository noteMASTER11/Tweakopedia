import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../style"

Page {
    id: root

    required property var controller
    property real availableWindowWidth: width
    readonly property var encyclopedia: controller ? controller.encyclopedia : null
    property string lastRequestedSection: ""
    readonly property bool compactLayout: availableWindowWidth < FluentTheme.compactBreakpoint
    readonly property bool treeVisible: !compactLayout
    readonly property bool showArticleContents: availableWindowWidth >= 1360
    signal tweakRequested(string id)

    background: Rectangle { color: FluentTheme.canvas }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 18
        anchors.bottomMargin: 18
        spacing: 12

        Text {
            id: pageTitle
            objectName: "tweakopediaTitle"
            Layout.fillWidth: true
            text: "Твикопедия"
            color: FluentTheme.textPrimary
            font.family: FluentTheme.fontFamily
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                id: backButton
                objectName: "tweakopediaBackButton"
                enabled: root.encyclopedia && root.encyclopedia.canGoBack
                implicitWidth: 38
                implicitHeight: 38
                Accessible.name: "Назад по истории статей"
                onClicked: root.encyclopedia.goBack()

                contentItem: Text {
                    objectName: "tweakopediaBackGlyph"
                    text: "\uE72B"
                    color: backButton.enabled
                        ? FluentTheme.textPrimary : FluentTheme.disabledText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                }

                background: Rectangle {
                    radius: 6
                    color: backButton.down ? FluentTheme.selected
                        : (backButton.hovered ? FluentTheme.hover : FluentTheme.surface)
                    border.width: backButton.activeFocus ? 2 : 1
                    border.color: backButton.activeFocus
                        ? FluentTheme.accent : FluentTheme.stroke
                }
            }

            Button {
                id: forwardButton
                objectName: "tweakopediaForwardButton"
                enabled: root.encyclopedia && root.encyclopedia.canGoForward
                implicitWidth: 38
                implicitHeight: 38
                Accessible.name: "Вперёд по истории статей"
                onClicked: root.encyclopedia.goForward()

                contentItem: Text {
                    objectName: "tweakopediaForwardGlyph"
                    text: "\uE72A"
                    color: forwardButton.enabled
                        ? FluentTheme.textPrimary : FluentTheme.disabledText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 14
                }

                background: Rectangle {
                    radius: 6
                    color: forwardButton.down ? FluentTheme.selected
                        : (forwardButton.hovered ? FluentTheme.hover : FluentTheme.surface)
                    border.width: forwardButton.activeFocus ? 2 : 1
                    border.color: forwardButton.activeFocus
                        ? FluentTheme.accent : FluentTheme.stroke
                }
            }

            Button {
                id: treeButton
                objectName: "tweakopediaTreeButton"
                visible: root.compactLayout
                implicitWidth: 38
                implicitHeight: 38
                Accessible.name: "Открыть дерево статей"
                onClicked: treeDrawer.open()

                contentItem: Text {
                    text: "\uE700"
                    color: FluentTheme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 15
                }

                background: Rectangle {
                    radius: 6
                    color: treeButton.down ? FluentTheme.selected
                        : (treeButton.hovered ? FluentTheme.hover : FluentTheme.surface)
                    border.width: treeButton.activeFocus ? 2 : 1
                    border.color: treeButton.activeFocus
                        ? FluentTheme.accent : FluentTheme.stroke
                }
            }

            FluentSearchField {
                id: searchField
                objectName: "tweakopediaSearchField"
                Layout.fillWidth: true
                text: root.encyclopedia ? root.encyclopedia.query : ""
                placeholderText: "Поиск по статьям, параметрам и компонентам Windows"
                Accessible.name: "Поиск в Твикопедии"
                onSearchRequested: query => root.encyclopedia.setQuery(query)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            EncyclopediaTree {
                id: encyclopediaTree
                objectName: "tweakopediaTreePanel"
                Layout.preferredWidth: root.showArticleContents ? 300 : 270
                Layout.fillHeight: true
                visible: root.treeVisible
                model: root.encyclopedia ? root.encyclopedia.tree : null
                onArticleRequested: id => root.encyclopedia.openArticle(id)
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                BusyIndicator {
                    anchors.centerIn: parent
                    visible: root.encyclopedia && root.encyclopedia.loading
                    running: visible
                }

                Column {
                    id: initialState
                    objectName: "tweakopediaInitialState"
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 520)
                    spacing: 10
                    visible: root.encyclopedia
                        && !root.encyclopedia.loading
                        && root.encyclopedia.tree.articleCount > 0
                        && !root.encyclopedia.article.hasArticle

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "▤"
                        color: FluentTheme.accent
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 56
                    }
                    Text {
                        width: parent.width
                        text: "Выберите статью или воспользуйтесь поиском"
                        color: FluentTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: "Материалы собраны из локального каталога Tweakopedia и доступны без подключения к интернету."
                        color: FluentTheme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                    }
                }

                Column {
                    id: noResults
                    objectName: "tweakopediaNoResults"
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 520)
                    spacing: 12
                    visible: root.encyclopedia
                        && !root.encyclopedia.loading
                        && root.encyclopedia.tree.articleCount === 0
                        && root.encyclopedia.query.length > 0

                    Text {
                        width: parent.width
                        text: "Статьи не найдены"
                        color: FluentTheme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: "Попробуйте изменить запрос или очистить поиск."
                        color: FluentTheme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        font.family: FluentTheme.fontFamily
                        font.pixelSize: 13
                    }
                    Button {
                        id: clearSearchButton
                        objectName: "tweakopediaClearSearchButton"
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Очистить поиск"
                        Accessible.name: text
                        onClicked: root.encyclopedia.clearSearch()
                    }
                }

                ArticleReader {
                    anchors.fill: parent
                    visible: root.encyclopedia && root.encyclopedia.article.hasArticle
                    articleModel: root.encyclopedia.article
                    controller: root.encyclopedia
                    showContents: root.showArticleContents
                    singleColumnSections: !root.showArticleContents
                    stackRelated: root.compactLayout
                    onSectionRequested: sectionId => root.lastRequestedSection = sectionId
                    onTweakRequested: id => root.tweakRequested(id)
                }
            }
        }
    }

    Popup {
        id: treeDrawer
        objectName: "tweakopediaTreeDrawer"
        parent: root
        x: 24
        y: 112
        width: Math.min(340, root.width - 48)
        height: Math.max(220, root.height - y - 18)
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        enter: Transition {
            NumberAnimation { property: "x"; from: -360; duration: 160; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "x"; to: -360; duration: 120; easing.type: Easing.InCubic }
        }

        background: Rectangle {
            color: FluentTheme.surface
            radius: 10
            border.width: 1
            border.color: FluentTheme.stroke
        }

        contentItem: EncyclopediaTree {
            model: root.encyclopedia ? root.encyclopedia.tree : null
            onArticleRequested: function(articleId) {
                root.encyclopedia.openArticle(articleId)
                treeDrawer.close()
            }
        }
    }
}
