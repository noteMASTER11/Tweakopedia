import QtQuick
import QtTest
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

TestCase {
    name: "RemainingPages"
    when: windowShown
    visible: true
    width: 1000
    height: 700

    ListModel { id: emptyTweaks }
    ListModel { id: emptyQueue }
    ListModel { id: queuedChanges }
    ListModel { id: emptyHistory }

    QtObject {
        id: fakeController
        property var tweaks: emptyTweaks
        property var queue: emptyQueue
        property var history: emptyHistory
        property bool previewReady: false
        property string previewSummary: ""
        property var previewOperations: []
        property string applyStatus: "idle"
        property int applyProgress: 0
        property string applyMessage: ""
        property bool rebootRequired: false
        property bool systemOverviewLoading: false
        property string systemOverviewError: ""
        property var systemOverview: ({
            greetingName: "Иван",
            computerName: "DESKTOP-TEST",
            manufacturer: "ASUSTeK COMPUTER INC.",
            model: "System Product Name",
            baseboard: "ROG STRIX B650E-F",
            biosSummary: "3202",
            biosMode: "UEFI",
            uptime: "3 ч 42 мин",
            logo: "windows11",
            osCaption: "Windows 11 Pro",
            osSummary: "10.0.26200 · сборка 26200 · x64",
            processor: { title: "AMD Ryzen 9 7950X", details: "16 ядер · 32 потока · 4,50 ГГц" },
            memory: { title: "64 ГБ", details: "48 ГБ доступно · 6000 MT/s · 2 модуля", usedPercent: 25 },
            graphics: [
                { name: "NVIDIA GeForce RTX 4090", details: "24 ГБ выделено · 16 ГБ разделяемой · драйвер 591.12", technical: "PCI 10DE:2D04" },
                { name: "AMD Radeon Graphics", details: "512 МБ выделено · 16 ГБ разделяемой", technical: "PCI 1002:164E" }
            ],
            disks: [
                { name: "Samsung SSD 990 PRO", details: "2 ТБ · SSD · NVMe", healthText: "Исправен", healthTone: "good" }
            ]
        })
        property int applyCalls: 0
        property int explanationCalls: 0
        property int restartCalls: 0
        property string lastPackageName: ""
        property string lastExplanationId: ""
        property string lastRemovedId: ""
        function buildPreview() { return false }
        function applyQueue(name) {
            ++applyCalls
            lastPackageName = name
            applyStatus = "running"
            return true
        }
        function removeFromQueue(id) { lastRemovedId = id; return true }
        function openExplanation(id) {
            ++explanationCalls
            lastExplanationId = id
            return {
                title: "Подробные сообщения входа",
                purpose: "Показывает подробные этапы входа.",
                mechanism: "Изменяет системную политику.",
                effect: "Windows выводит больше сведений.",
                tradeoffs: "Экран входа становится подробнее.",
                recommendation: "Использовать при диагностике.",
                technicalDetails: "VerboseStatus",
                registryObject: "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\VerboseStatus",
                rollback: "Возвращается исходное значение."
            }
        }
        function restartComputer() { ++restartCalls; return true }
        function rollback(id) { return false }
    }

    Component { id: overviewComponent; OverviewPage { width: 900; height: 620; controller: fakeController } }
    Component { id: queueComponent; QueuePage { width: 900; height: 620; controller: fakeController } }
    Component { id: historyComponent; HistoryPage { width: 900; height: 620; controller: fakeController } }

    function init() {
        queuedChanges.clear()
        fakeController.queue = emptyQueue
        fakeController.applyStatus = "idle"
        fakeController.applyProgress = 0
        fakeController.applyMessage = ""
        fakeController.rebootRequired = false
        fakeController.applyCalls = 0
        fakeController.explanationCalls = 0
        fakeController.restartCalls = 0
        fakeController.lastPackageName = ""
        fakeController.lastExplanationId = ""
        fakeController.lastRemovedId = ""
    }

    function addQueuedChange() {
        queuedChanges.append({
            id: "boot.verbose-logon-messages",
            title: "Подробные сообщения входа",
            currentState: "disabled",
            targetState: "enabled"
        })
        fakeController.queue = queuedChanges
    }

    function test_overviewShowsComputerPassportAndWindowsLogo() {
        const page = createTemporaryObject(overviewComponent, this)
        compare(findChild(page, "overviewBackground").color, FluentTheme.canvas)
        compare(findChild(page, "overviewGreeting").text, "Здравствуйте, Иван!")
        compare(findChild(page, "overviewComputerName").text, "DESKTOP-TEST")
        compare(findChild(page, "overviewWindowsLogo").source, "qrc:/images/windows-11.svg")
        compare(findChild(page, "overviewProcessorName").text, "AMD Ryzen 9 7950X")
        compare(findChild(page, "overviewMemoryTitle").text, "64 ГБ")
        compare(findChild(page, "overviewDiskRepeater").count, 1)
    }

    function test_queueEmptyStateHidesApplyCommand() {
        const page = createTemporaryObject(queueComponent, this)
        compare(findChild(page, "queueEmptyState").visible, true)
        compare(findChild(page, "applyQueueButton").visible, false)
        compare(findChild(page, "queuePlanPreview"), null)
        compare(findChild(page, "queueBackground").color, FluentTheme.canvas)
    }

    function test_queueUsesCancelAndApplyActions() {
        addQueuedChange()
        const page = createTemporaryObject(queueComponent, this)

        const cancel = findChild(page, "cancelQueueItemButton")
        compare(cancel.text, "Отменить")
        const cancelBackground = findChild(page, "cancelQueueItemBackground")
        compare(cancelBackground.color, "#c42b1c")
        compare(cancelBackground.radius, 6)
        mouseMove(cancel, cancel.width / 2, cancel.height / 2)
        tryCompare(cancel, "hovered", true)
        compare(cancelBackground.color, "#a4262c")
        cancel.clicked()
        compare(fakeController.lastRemovedId, "boot.verbose-logon-messages")

        const apply = findChild(page, "applyQueueButton")
        compare(apply.text, "Применить")
        compare(findChild(page, "applyQueueButtonBackground").color, FluentTheme.accent)
        compare(findChild(page, "applyQueueButtonBackground").radius, 6)
        apply.clicked()

        compare(fakeController.applyCalls, 1)
        verify(fakeController.lastPackageName.indexOf("Изменения Windows — ") === 0)
        compare(apply.enabled, false)
        compare(findChild(page, "applySpinner").visible, true)
        compare(findChild(page, "applySpinner").running, true)
    }

    function test_queueCardOpensExplanationWithoutCancellingChange() {
        addQueuedChange()
        const page = createTemporaryObject(queueComponent, this)

        const card = findChild(page, "queueItemCard")
        verify(card !== null)
        mouseClick(card, 24, card.height / 2)

        compare(fakeController.explanationCalls, 1)
        compare(fakeController.lastExplanationId, "boot.verbose-logon-messages")
        compare(fakeController.lastRemovedId, "")
        compare(findChild(page, "queueInfoPane").visible, true)
        compare(findChild(page, "infoTitle").text, "Подробные сообщения входа")
    }

    function test_failedApplyRestoresButtonAndShowsMessage() {
        addQueuedChange()
        fakeController.applyStatus = "failed"
        fakeController.applyMessage = "Не удалось применить изменения"
        const page = createTemporaryObject(queueComponent, this)

        compare(findChild(page, "applyQueueButton").enabled, true)
        compare(findChild(page, "applySpinner").visible, false)
        const error = findChild(page, "applyErrorMessage")
        compare(error.visible, true)
        compare(error.text, "Не удалось применить изменения")
    }

    function test_successScreenOffersRestartOnlyWhenRequired() {
        fakeController.applyStatus = "succeeded"
        const page = createTemporaryObject(queueComponent, this)

        compare(findChild(page, "applySuccessView").visible, true)
        compare(findChild(page, "applySuccessTitle").text, "Настройки применены")
        compare(findChild(page, "applySuccessCheck").color, FluentTheme.stateOn)
        compare(findChild(page, "rebootRequirementText").visible, false)
        compare(findChild(page, "restartComputerButton").visible, false)

        fakeController.rebootRequired = true
        const rebootText = findChild(page, "rebootRequirementText")
        const restart = findChild(page, "restartComputerButton")
        tryCompare(rebootText, "visible", true)
        compare(rebootText.text, "Некоторые из применённых настроек требуют перезагрузки ПК")
        tryCompare(restart, "visible", true)
        compare(restart.text, "Перезагрузить")
        restart.clicked()
        compare(fakeController.restartCalls, 1)
    }

    function test_historyEmptyState() {
        const page = createTemporaryObject(historyComponent, this)
        compare(findChild(page, "historyEmptyState").visible, true)
        compare(findChild(page, "historyBackground").color, FluentTheme.canvas)
        const description = findChild(page, "historyDescription")
        verify(description)
        verify(description.text.indexOf("раскройте пакет") >= 0)
        verify(description.text.indexOf("Вернуть исходное") >= 0)
    }

    function test_aboutPageDescribesBuildLibrariesAndCredits() {
        const component = Qt.createComponent(Qt.resolvedUrl(
            "../../apps/tweakopedia/qml/pages/AboutPage.qml"))
        compare(component.status, Component.Ready, component.errorString())
        const page = createTemporaryObject(component, this, {width: 900, height: 620})

        compare(findChild(page, "aboutBackground").color, FluentTheme.canvas)
        compare(findChild(page, "aboutTitle").text, "О программе")
        compare(findChild(page, "aboutProductName").text, "Tweakopedia")
        compare(findChild(page, "aboutVersion").text, "Версия 0.9 · x64 · Portable")
        verify(findChild(page, "aboutDescription").text.indexOf("офлайн-энциклопедия") >= 0)

        const libraries = findChild(page, "aboutLibraries")
        verify(libraries.text.indexOf("Qt 6.8.3") >= 0)
        verify(libraries.text.indexOf("yaml-cpp 0.8.0") >= 0)
        verify(libraries.text.indexOf("nlohmann/json 3.12.0") >= 0)
        verify(libraries.text.indexOf("miniz 3.1.2") >= 0)
        verify(libraries.text.indexOf("SQLite") >= 0)

        const credits = findChild(page, "aboutCredits")
        verify(credits.text.indexOf("Raphire") >= 0)
        verify(credits.text.indexOf("Plínio Larrubia / LeDragoX") >= 0)
        verify(credits.text.indexOf("thebookisclosed") >= 0)
        verify(credits.text.indexOf("Sergey Tkachenko") >= 0)
        verify(credits.text.indexOf("SanLex") < 0)
    }
}
