import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"

TestCase {
    name: "NetworkDiagnosticsPage"
    when: windowShown
    visible: true
    width: 1000
    height: 760

    QtObject {
        id: fakeController
        property string state: "idle"
        property string stageText: ""
        property string outputText: ""
        property string resultTitle: ""
        property string resultDescription: ""
        property string errorCode: ""
        property string sessionLogPath: ""
        property var checks: []
        property int okCount: 0
        property int attentionCount: 0
        property int unavailableCount: 0
        property bool hasAttention: attentionCount > 0
        property bool busy: false
        property bool canStart: !busy
        property string actionState: "idle"
        property string actionMessage: ""
        property string activeRuleKey: ""
        property var removedRuleKeys: []
        property int startCalls: 0
        property int copyCalls: 0
        property int openCalls: 0
        property int removeCalls: 0
        property string lastRemovedRule: ""
        function startDiagnostics() { ++startCalls; return true }
        function copyOutput() { ++copyCalls; return true }
        function openSessionLog() { ++openCalls; return true }
        function removeFirewallRule(key) {
            ++removeCalls
            lastRemovedRule = key
            return true
        }
    }

    Component {
        id: pageComponent
        NetworkDiagnosticsPage {
            width: 960
            height: 720
            controller: fakeController
        }
    }

    function init() {
        fakeController.state = "idle"
        fakeController.stageText = ""
        fakeController.outputText = ""
        fakeController.resultTitle = ""
        fakeController.resultDescription = ""
        fakeController.errorCode = ""
        fakeController.sessionLogPath = ""
        fakeController.checks = []
        fakeController.okCount = 0
        fakeController.attentionCount = 0
        fakeController.unavailableCount = 0
        fakeController.busy = false
        fakeController.startCalls = 0
        fakeController.copyCalls = 0
        fakeController.openCalls = 0
        fakeController.removeCalls = 0
        fakeController.lastRemovedRule = ""
        fakeController.actionState = "idle"
        fakeController.actionMessage = ""
        fakeController.activeRuleKey = ""
        fakeController.removedRuleKeys = []
    }

    function test_idleStartsDiagnostics() {
        const page = createTemporaryObject(pageComponent, this)
        const primary = findChild(page, "networkDiagnosticsPrimaryButton")
        compare(primary.text, "Начать диагностику")
        verify(primary.enabled)
        mouseClick(primary)
        compare(fakeController.startCalls, 1)
    }

    function test_runningShowsCurrentStageAndTerminal() {
        fakeController.state = "checking"
        fakeController.stageText = "Проверка DNS"
        fakeController.outputText = "[DNS] Получение конфигурации…"
        fakeController.busy = true
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "networkDiagnosticsStageText").text, "Проверка DNS")
        verify(findChild(page, "networkDiagnosticsProgressBar").indeterminate)
        compare(findChild(page, "networkDiagnosticsTerminal").text,
                "[DNS] Получение конфигурации…")
        verify(!findChild(page, "networkDiagnosticsPrimaryButton").enabled)
    }

    function test_resultBuildsCardsCountersAndOpensFirstAttentionItem() {
        fakeController.state = "issues_found"
        fakeController.resultTitle = "Обнаружены параметры, требующие внимания"
        fakeController.resultDescription = "Просмотрите отмеченные блоки."
        fakeController.checks = [
            { id: "adapters", title: "Сетевые адаптеры", status: "ok",
              summary: "Подключение активно", details: ["Ethernet"] },
            { id: "dns", title: "DNS", status: "attention",
              summary: "Сервер не отвечает", details: ["1.1.1.1"] },
            { id: "hosts", title: "Файл hosts", status: "unavailable",
              summary: "Данные недоступны", details: [] }
        ]
        fakeController.okCount = 1
        fakeController.attentionCount = 1
        fakeController.unavailableCount = 1
        const page = createTemporaryObject(pageComponent, this)
        compare(findChild(page, "networkDiagnosticsOkCount").text, "1")
        compare(findChild(page, "networkDiagnosticsAttentionCount").text, "1")
        compare(findChild(page, "networkDiagnosticsUnavailableCount").text, "1")
        compare(findChild(page, "networkDiagnosticsCheckRepeater").count, 3)
        const scroll = findChild(page, "networkDiagnosticsPageScroll")
        scroll.contentItem.contentY = Math.max(0,
            scroll.contentItem.contentHeight - scroll.contentItem.height)
        wait(0)
        mouseClick(findChild(page, "networkDiagnosticsRecoveryButton"))
        const panel = findChild(page, "networkDiagnosticsDetailPane")
        tryCompare(panel, "visible", true)
        compare(findChild(page, "networkDiagnosticsDetailTitle").text, "DNS")
    }

    function test_secondaryActionsAndBackSignal() {
        fakeController.outputText = "output"
        fakeController.sessionLogPath = "D:/logs/network.log"
        const page = createTemporaryObject(pageComponent, this)
        const backSpy = signalSpy.createObject(page, {
            target: page,
            signalName: "backRequested"
        })
        mouseClick(findChild(page, "networkDiagnosticsCopyButton"))
        mouseClick(findChild(page, "networkDiagnosticsOpenLogButton"))
        mouseClick(findChild(page, "networkDiagnosticsBackButton"))
        compare(fakeController.copyCalls, 1)
        compare(fakeController.openCalls, 1)
        compare(backSpy.count, 1)
    }

    function test_narrowPageKeepsPrimaryActionInsideBounds() {
        const page = createTemporaryObject(pageComponent, this, { width: 620, height: 720 })
        const primary = findChild(page, "networkDiagnosticsPrimaryButton")
        const point = primary.mapToItem(page, 0, 0)
        verify(point.x >= 0)
        verify(point.x + primary.width <= page.width)
    }

    function test_resultCardOpensDetailsAndKeepsSelectionAfterClosing() {
        fakeController.checks = [
            { id: "dns", title: "DNS", status: "attention",
              summary: "Сервер не отвечает", details: ["Сервер: 1.1.1.1", "Тайм-аут"] }
        ]
        const page = createTemporaryObject(pageComponent, this)
        const card = findChild(page, "networkDiagnosticsCheckCard_dns")
        verify(card)
        mouseClick(card)
        const panel = findChild(page, "networkDiagnosticsDetailPane")
        tryCompare(panel, "visible", true)
        compare(findChild(page, "networkDiagnosticsDetailTitle").text, "DNS")
        verify(findChild(page, "networkDiagnosticsDetailList").count === 2)

        mouseClick(findChild(page, "networkDiagnosticsDetailCloseButton"))
        tryCompare(panel, "visible", false)
        mouseClick(card)
        tryCompare(panel, "visible", true)
        compare(findChild(page, "networkDiagnosticsDetailTitle").text, "DNS")
    }

    function test_resultsUseSingleListAndDockedDetailsOnWideWindow() {
        fakeController.checks = [
            { id: "adapters", title: "Сетевые адаптеры", status: "ok",
              summary: "Подключение активно", details: ["Ethernet"] },
            { id: "dns", title: "DNS", status: "attention",
              summary: "Сервер не отвечает", details: ["1.1.1.1"] }
        ]
        const page = createTemporaryObject(pageComponent, this, { width: 1280, height: 760 })
        const first = findChild(page, "networkDiagnosticsCheckCard_adapters")
        const second = findChild(page, "networkDiagnosticsCheckCard_dns")
        verify(first)
        verify(second)
        compare(Math.round(first.x), Math.round(second.x))
        verify(second.y > first.y)

        mouseClick(second)
        const panel = findChild(page, "networkDiagnosticsDetailPane")
        const scroll = findChild(page, "networkDiagnosticsPageScroll")
        tryCompare(panel, "visible", true)
        compare(panel.modal, false)
        compare(Math.round(panel.width), 420)
        tryCompare(scroll, "width", page.width - panel.width)
        compare(Math.round(findChild(page, "networkDiagnosticsContent").width),
                Math.round(scroll.width))
        compare(findChild(page, "networkDiagnosticsDetailTitle").text, "DNS")
    }

    function test_detailsOverlayTheNarrowWindow() {
        fakeController.checks = [
            { id: "dns", title: "DNS", status: "attention",
              summary: "Сервер не отвечает", details: ["1.1.1.1"] }
        ]
        const page = createTemporaryObject(pageComponent, this, { width: 720, height: 760 })
        const card = findChild(page, "networkDiagnosticsCheckCard_dns")
        const scroll = findChild(page, "networkDiagnosticsPageScroll")
        const position = card.mapToItem(scroll.contentItem, 0, 0)
        scroll.contentItem.contentY = Math.max(0, position.y - 16)
        wait(0)
        mouseClick(card)
        const panel = findChild(page, "networkDiagnosticsDetailPane")
        tryCompare(panel, "visible", true)
        compare(panel.modal, true)
        verify(panel.width <= page.width)
    }

    function test_firewallDetailsRequireConfirmationBeforeRemoval() {
        fakeController.checks = [
            { id: "firewall", title: "Windows Firewall", status: "attention",
              summary: "Найдено правило", details: ["Входящее разрешение"],
              items: [{ key: "{RULE-ID}", title: "Тестовое правило",
                        description: "Разрешает входящее подключение",
                        profile: "Private", program: "C:/Example/app.exe",
                        protocol: "TCP", local_port: "8080",
                        remote_address: "Any" }] }
        ]
        const page = createTemporaryObject(pageComponent, this)
        mouseClick(findChild(page, "networkDiagnosticsCheckCard_firewall"))
        const ruleRepeater = findChild(page, "firewallRuleRepeater")
        verify(ruleRepeater)
        tryCompare(ruleRepeater, "count", 1)
        const ruleCard = ruleRepeater.itemAt(0)
        verify(ruleCard)
        const removeButton = findChild(ruleCard, "firewallRuleRemoveButton")
        verify(removeButton)
        mouseClick(removeButton)
        const confirmation = findChild(page, "firewallRuleConfirmation")
        tryCompare(confirmation, "visible", true)
        compare(fakeController.removeCalls, 0)
        verify(findChild(page, "firewallRuleConfirmationText").text.indexOf("{RULE-ID}") >= 0)
        mouseClick(findChild(page, "firewallRuleConfirmButton"))
        compare(fakeController.removeCalls, 1)
        compare(fakeController.lastRemovedRule, "{RULE-ID}")
    }

    function test_firewallActionsAreLockedWhileAnotherActionRuns() {
        fakeController.checks = [
            { id: "firewall", title: "Windows Firewall", status: "attention",
              summary: "Найдены правила", details: [],
              items: [
                  { key: "rule-one", title: "Первое правило" },
                  { key: "rule-two", title: "Второе правило" }
              ] }
        ]
        fakeController.busy = true
        fakeController.actionState = "running"
        fakeController.activeRuleKey = "rule-one"
        const page = createTemporaryObject(pageComponent, this)
        mouseClick(findChild(page, "networkDiagnosticsCheckCard_firewall"))
        const repeater = findChild(page, "firewallRuleRepeater")
        tryCompare(repeater, "count", 2)
        for (let i = 0; i < 2; ++i) {
            const button = findChild(repeater.itemAt(i), "firewallRuleRemoveButton")
            verify(button)
            verify(!button.enabled)
        }
    }

    function test_largeFirewallResultOpensPanelBeforeRenderingEveryRule() {
        let rules = []
        for (let i = 0; i < 64; ++i) {
            rules.push({
                key: "rule-" + i,
                title: "Правило " + (i + 1),
                description: "Разрешающее входящее правило",
                profile: "Private",
                program: "C:/Program Files/Example/app-" + i + ".exe",
                protocol: i % 2 === 0 ? "TCP" : "UDP",
                local_port: "Any",
                remote_port: "Any",
                local_address: "Any",
                remote_address: "Any"
            })
        }
        fakeController.checks = [
            { id: "firewall", title: "Windows Firewall", status: "attention",
              summary: "Найдено 64 правила", details: ["Domain", "Private", "Public"],
              items: rules }
        ]

        const page = createTemporaryObject(pageComponent, this)
        mouseClick(findChild(page, "networkDiagnosticsCheckCard_firewall"))
        const panel = findChild(page, "networkDiagnosticsDetailPane")
        tryCompare(panel, "visible", true)
        compare(findChild(page, "networkDiagnosticsDetailTitle").text, "Windows Firewall")
        compare(findChild(page, "firewallRuleRepeater").count, 12)
        const moreButton = findChild(page, "firewallRulesShowMoreButton")
        verify(moreButton.visible)
        mouseClick(moreButton)
        compare(findChild(page, "firewallRuleRepeater").count, 24)
    }

    function test_resultGuideOpensFullArticle() {
        const page = createTemporaryObject(pageComponent, this)
        const articleSpy = signalSpy.createObject(page, {
            target: page,
            signalName: "articleRequested"
        })
        const button = findChild(page, "toolResultGuideArticleButton")
        verify(button)
        mouseClick(button)
        compare(articleSpy.count, 1)
        compare(articleSpy.signalArguments[0][0], "guide.network-diagnostics")
    }

    Component { id: signalSpy; SignalSpy {} }
}
