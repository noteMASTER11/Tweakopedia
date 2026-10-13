#include "execution/SystemToolProtocol.h"

#include <QJsonObject>
#include <QtTest/QTest>

using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

class SystemToolProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsKnownMessages()
    {
        const auto id = QUuid::createUuid();

        const SystemToolRequest request{
            id, SystemToolKind::ComponentStore, SystemToolOperation::Scan, {}};
        const auto decodedRequest = SystemToolProtocol::decodeRequest(
            SystemToolProtocol::encode(request));
        QVERIFY(decodedRequest.value.has_value());
        QCOMPARE(decodedRequest.value->requestId, id);
        QCOMPARE(decodedRequest.value->tool, SystemToolKind::ComponentStore);
        QCOMPARE(decodedRequest.value->operation, SystemToolOperation::Scan);
        QVERIFY(decodedRequest.value->target.isEmpty());

        const SystemToolRequest fileSystemRequest{
            id, SystemToolKind::FileSystem, SystemToolOperation::Check, u"C:"_s};
        const auto decodedFileSystemRequest = SystemToolProtocol::decodeRequest(
            SystemToolProtocol::encode(fileSystemRequest));
        QVERIFY(decodedFileSystemRequest.value.has_value());
        QCOMPARE(decodedFileSystemRequest.value->tool, SystemToolKind::FileSystem);
        QCOMPARE(decodedFileSystemRequest.value->operation, SystemToolOperation::Check);
        QCOMPARE(decodedFileSystemRequest.value->target, u"C:"_s);

        const SystemToolRequest networkRequest{
            id, SystemToolKind::NetworkDiagnostics, SystemToolOperation::Diagnose, {}};
        const auto decodedNetworkRequest = SystemToolProtocol::decodeRequest(
            SystemToolProtocol::encode(networkRequest));
        QVERIFY(decodedNetworkRequest.value.has_value());
        QCOMPARE(decodedNetworkRequest.value->tool, SystemToolKind::NetworkDiagnostics);
        QCOMPARE(decodedNetworkRequest.value->operation, SystemToolOperation::Diagnose);
        QVERIFY(decodedNetworkRequest.value->target.isEmpty());

        const SystemToolStageEvent stage{id, SystemToolStage::CheckHealth};
        const auto decodedStage = SystemToolProtocol::decodeStage(
            SystemToolProtocol::encode(stage));
        QVERIFY(decodedStage.value.has_value());
        QCOMPARE(decodedStage.value->stage, SystemToolStage::CheckHealth);

        const SystemToolStageEvent networkStage{id, SystemToolStage::CheckNetworkDns};
        const auto decodedNetworkStage = SystemToolProtocol::decodeStage(
            SystemToolProtocol::encode(networkStage));
        QVERIFY(decodedNetworkStage.value.has_value());
        QCOMPARE(decodedNetworkStage.value->stage, SystemToolStage::CheckNetworkDns);

        const SystemToolOutputEvent output{id, 42, u"hello\n"_s};
        const auto decodedOutput = SystemToolProtocol::decodeOutput(
            SystemToolProtocol::encode(output));
        QVERIFY(decodedOutput.value.has_value());
        QCOMPARE(decodedOutput.value->sequence, 42);
        QCOMPARE(decodedOutput.value->text, u"hello\n"_s);

        const SystemToolProgressEvent progress{id, SystemToolStage::ScanHealth, 87};
        const auto decodedProgress = SystemToolProtocol::decodeProgress(
            SystemToolProtocol::encode(progress));
        QVERIFY(decodedProgress.value.has_value());
        QCOMPARE(decodedProgress.value->percent, 87);

        const SystemToolResult result{
            .requestId = id,
            .status = SystemToolResultStatus::Completed,
            .exitCode = 0,
            .outcome = SystemToolOutcome::Healthy,
            .logPath = u"C:/logs/scan.log"_s,
            .details = {{u"ok_count"_s, 6}},
        };
        const auto decodedResult = SystemToolProtocol::decodeResult(
            SystemToolProtocol::encode(result));
        QVERIFY(decodedResult.value.has_value());
        QCOMPARE(decodedResult.value->status, SystemToolResultStatus::Completed);
        QCOMPARE(decodedResult.value->outcome, SystemToolOutcome::Healthy);
        QCOMPARE(decodedResult.value->logPath, result.logPath);
        QCOMPARE(decodedResult.value->details, result.details);
    }

    void rejectsMalformedMessages()
    {
        const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        auto request = SystemToolProtocol::encode(
            SystemToolRequest{QUuid::createUuid(), SystemToolKind::ComponentStore,
                              SystemToolOperation::Scan, {}});
        request.insert(u"operation"_s, u"arbitrary.command"_s);
        QVERIFY(!SystemToolProtocol::decodeRequest(request).value.has_value());
        request = SystemToolProtocol::encode(SystemToolRequest{
            QUuid::createUuid(), SystemToolKind::FileSystem,
            SystemToolOperation::Check, u"C:"_s});
        request.insert(u"target"_s, u"C: /F"_s);
        QVERIFY(!SystemToolProtocol::decodeRequest(request).value.has_value());
        request.insert(u"operation"_s, u"scan"_s);
        request.insert(u"request_id"_s, u"not-a-uuid"_s);
        QVERIFY(!SystemToolProtocol::decodeRequest(request).value.has_value());
        request = SystemToolProtocol::encode(SystemToolRequest{
            QUuid::createUuid(), SystemToolKind::NetworkDiagnostics,
            SystemToolOperation::Diagnose, {}});
        request.insert(u"target"_s, u"user supplied"_s);
        QVERIFY(!SystemToolProtocol::decodeRequest(request).value.has_value());

        QJsonObject stage{{u"type"_s, u"tool_stage"_s}, {u"request_id"_s, id},
                          {u"stage"_s, u"unknown"_s}};
        QVERIFY(!SystemToolProtocol::decodeStage(stage).value.has_value());

        QJsonObject output{{u"type"_s, u"tool_output"_s}, {u"request_id"_s, id},
                           {u"sequence"_s, -1}, {u"text"_s, u"x"_s}};
        QVERIFY(!SystemToolProtocol::decodeOutput(output).value.has_value());

        QJsonObject progress{{u"type"_s, u"tool_progress"_s}, {u"request_id"_s, id},
                             {u"stage"_s, u"scan_health"_s}, {u"percent"_s, 101}};
        QVERIFY(!SystemToolProtocol::decodeProgress(progress).value.has_value());

        QJsonObject result{{u"type"_s, u"tool_result"_s}, {u"request_id"_s, id},
                           {u"status"_s, u"completed"_s}, {u"exit_code"_s, 0},
                           {u"outcome"_s, u"invented"_s}, {u"log_path"_s, u"x"_s}};
        QVERIFY(!SystemToolProtocol::decodeResult(result).value.has_value());
    }
};

QTEST_GUILESS_MAIN(SystemToolProtocolTest)
#include "SystemToolProtocolTest.moc"
