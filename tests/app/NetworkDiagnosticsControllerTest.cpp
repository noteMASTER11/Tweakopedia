#include "app/ISystemIntegrityDesktopServices.h"
#include "app/NetworkDiagnosticsController.h"
#include "fakes/FakeSystemToolRunner.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia;

namespace {

class FakeDesktopServices final : public app::ISystemIntegrityDesktopServices
{
public:
    QString dismLogPath() const override { return {}; }
    bool fileExists(const QString& path) const override { return existing.contains(path); }
    bool copyText(const QString& text) override { copied = text; return copyResult; }
    bool openFile(const QString& path) override { opened = path; return openResult; }

    QSet<QString> existing;
    QString copied;
    QString opened;
    bool copyResult{true};
    bool openResult{true};
};

QJsonObject diagnosticDetails()
{
    return {
        {u"checks"_s, QJsonArray{
            QJsonObject{{u"id"_s, u"adapters"_s}, {u"title"_s, u"Адаптеры"_s},
                        {u"status"_s, u"ok"_s}, {u"summary"_s, u"Готово"_s},
                        {u"details"_s, QJsonArray{u"Ethernet"_s}}},
            QJsonObject{{u"id"_s, u"dns"_s}, {u"title"_s, u"DNS"_s},
                        {u"status"_s, u"attention"_s}, {u"summary"_s, u"Нужна проверка"_s},
                        {u"details"_s, QJsonArray{}}},
        }},
        {u"ok_count"_s, 1},
        {u"attention_count"_s, 1},
        {u"unavailable_count"_s, 0},
    };
}

} // namespace

class NetworkDiagnosticsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void startsNetworkDiagnosticsAndBuildsResultModel()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::NetworkDiagnosticsController controller(runner, desktop);

        QCOMPARE(controller.state(), u"idle"_s);
        QVERIFY(controller.canStart());
        QVERIFY(controller.startDiagnostics());
        QCOMPARE(runner.startCalls, 1);
        QCOMPARE(runner.lastTool, execution::SystemToolKind::NetworkDiagnostics);
        QCOMPARE(runner.lastOperation, execution::SystemToolOperation::Diagnose);
        QCOMPARE(controller.state(), u"awaiting_elevation"_s);

        const auto id = QUuid::createUuid();
        emit runner.stageChanged({id, execution::SystemToolStage::CollectNetworkAdapters});
        QCOMPARE(controller.state(), u"checking"_s);
        emit runner.outputReceived({id, 0, u"Вывод\n"_s});
        QCOMPARE(controller.outputText(), u"Вывод\n"_s);
        emit runner.finished({.requestId = id,
                              .status = execution::SystemToolResultStatus::Completed,
                              .exitCode = 0,
                              .outcome = execution::SystemToolOutcome::IssuesFound,
                              .logPath = u"C:/logs/network.log"_s,
                              .details = diagnosticDetails()});

        QCOMPARE(controller.state(), u"issues_found"_s);
        QCOMPARE(controller.checks().size(), 2);
        QCOMPARE(controller.okCount(), 1);
        QCOMPARE(controller.attentionCount(), 1);
        QCOMPARE(controller.unavailableCount(), 0);
        QVERIFY(controller.hasAttention());
        QVERIFY(controller.resultTitle().contains(u"требующие внимания"_s));
    }

    void mapsIncompleteAndFailureStates()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::NetworkDiagnosticsController controller(runner, desktop);
        QVERIFY(controller.startDiagnostics());
        const auto id = QUuid::createUuid();
        emit runner.stageChanged({id, execution::SystemToolStage::CheckNetworkDns});
        auto details = diagnosticDetails();
        details.insert(u"attention_count"_s, 0);
        details.insert(u"unavailable_count"_s, 1);
        emit runner.finished({.requestId = id,
                              .status = execution::SystemToolResultStatus::Completed,
                              .exitCode = 0,
                              .outcome = execution::SystemToolOutcome::Incomplete,
                              .details = details});
        QCOMPARE(controller.state(), u"incomplete"_s);

        QVERIFY(controller.startDiagnostics());
        emit runner.failed(u"ipc.timeout"_s, u"Нет ответа"_s);
        QCOMPARE(controller.state(), u"failed"_s);
        QCOMPARE(controller.errorCode(), u"ipc.timeout"_s);
    }

    void supportsClipboardLogAndCancellation()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::NetworkDiagnosticsController controller(runner, desktop);
        QVERIFY(controller.startDiagnostics());
        const auto id = QUuid::createUuid();
        emit runner.stageChanged({id, execution::SystemToolStage::CheckNetworkProxy});
        emit runner.outputReceived({id, 0, u"abc"_s});
        emit runner.finished({.requestId = id,
                              .status = execution::SystemToolResultStatus::Completed,
                              .exitCode = 0,
                              .outcome = execution::SystemToolOutcome::Healthy,
                              .logPath = u"C:/logs/network.log"_s,
                              .details = diagnosticDetails()});
        desktop.existing.insert(u"C:/logs/network.log"_s);
        QVERIFY(controller.copyOutput());
        QCOMPARE(desktop.copied, u"abc"_s);
        QVERIFY(controller.openSessionLog());
        QCOMPARE(desktop.opened, u"C:/logs/network.log"_s);

        QVERIFY(controller.startDiagnostics());
        emit runner.launchCancelled(u"Отменено"_s);
        QCOMPARE(controller.state(), u"idle"_s);
        QVERIFY(!controller.busy());
    }

    void capsVisibleOutputBuffer()
    {
        tests::FakeSystemToolRunner runner;
        FakeDesktopServices desktop;
        app::NetworkDiagnosticsController controller(runner, desktop);
        QVERIFY(controller.startDiagnostics());
        const auto id = QUuid::createUuid();
        emit runner.stageChanged({id, execution::SystemToolStage::CollectNetworkAdapters});
        emit runner.outputReceived({id, 0, QString(3 * 1024 * 1024, u'x')});
        QVERIFY(controller.outputText().toUtf8().size() <= 2 * 1024 * 1024);
        QVERIFY(controller.outputText().startsWith(u"[Начало вывода удалено"_s));
    }
};

QTEST_APPLESS_MAIN(NetworkDiagnosticsControllerTest)
#include "NetworkDiagnosticsControllerTest.moc"
