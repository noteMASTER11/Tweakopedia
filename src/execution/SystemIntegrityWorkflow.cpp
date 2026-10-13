#include "execution/SystemIntegrityWorkflow.h"

#include <QSysInfo>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

SystemIntegrityWorkflow::SystemIntegrityWorkflow(
    IDismProcessRunner& runner,
    QString logsRoot,
    QObject* parent)
    : QObject(parent), runner_(&runner), logsRoot_(std::move(logsRoot))
{
}

bool SystemIntegrityWorkflow::start(const SystemToolRequest& request)
{
    if (active_ || request.requestId.isNull()) return false;
    request_ = request;
    if (!log_.open(logsRoot_, request.operation,
                   {u"Windows: "_s + QSysInfo::prettyProductName(),
                    u"Architecture: "_s + QSysInfo::currentCpuArchitecture()})) {
        emit finished({.requestId = request.requestId,
                       .status = SystemToolResultStatus::Failed,
                       .code = u"tool.log_open_failed"_s,
                       .message = log_.errorString()});
        return false;
    }

    commands_.clear();
    if (request.operation == SystemToolOperation::Scan) {
        commands_.append({SystemToolStage::ScanHealth,
                          {u"/Online"_s, u"/Cleanup-Image"_s,
                           u"/ScanHealth"_s, u"/English"_s}});
        commands_.append({SystemToolStage::CheckHealth,
                          {u"/Online"_s, u"/Cleanup-Image"_s,
                           u"/CheckHealth"_s, u"/English"_s}});
    } else {
        commands_.append({SystemToolStage::RestoreHealth,
                          {u"/Online"_s, u"/Cleanup-Image"_s,
                           u"/RestoreHealth"_s, u"/English"_s}});
        commands_.append({SystemToolStage::VerifyRestore,
                          {u"/Online"_s, u"/Cleanup-Image"_s,
                           u"/CheckHealth"_s, u"/English"_s}});
    }
    active_ = true;
    commandIndex_ = 0;
    runNext();
    return active_;
}

bool SystemIntegrityWorkflow::active() const noexcept
{
    return active_;
}

void SystemIntegrityWorkflow::runNext()
{
    const auto& command = commands_.at(commandIndex_);
    parser_.reset(command.stage);
    lastProgress_ = -1;
    displayLine_.clear();
    pendingCarriageReturn_ = false;
    emit stageChanged({request_.requestId, command.stage});
    log_.appendCommand(u"DISM.exe"_s, command.arguments);
    emit outputReady(request_.requestId,
                     u"> DISM.exe "_s + command.arguments.join(u' ') + u"\n"_s);
    if (!runner_->start(
            u"DISM.exe"_s,
            command.arguments,
            [this](const QByteArray& bytes) { consumeOutput(bytes); },
            [this](DismProcessResult result) { commandFinished(std::move(result)); })) {
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .code = u"dism.runner_busy"_s,
                  .message = u"Не удалось запустить DISM."_s});
    }
}

void SystemIntegrityWorkflow::consumeOutput(const QByteArray& bytes)
{
    log_.appendOutput(bytes);
    parser_.consume(bytes);
    if (parser_.progress() > lastProgress_) {
        lastProgress_ = parser_.progress();
        emit progressChanged({request_.requestId, commands_.at(commandIndex_).stage,
                              lastProgress_});
    }

    const auto text = QString::fromLocal8Bit(bytes);
    for (const auto character : text) {
        if (pendingCarriageReturn_) {
            if (character == u'\n') {
                emitDisplayLine(displayLine_ + u'\n');
                displayLine_.clear();
                pendingCarriageReturn_ = false;
                continue;
            }
            displayLine_.clear();
            pendingCarriageReturn_ = false;
        }
        if (character == u'\r') {
            pendingCarriageReturn_ = true;
        } else if (character == u'\n') {
            emitDisplayLine(displayLine_ + u'\n');
            displayLine_.clear();
        } else {
            displayLine_.append(character);
        }
    }
}

void SystemIntegrityWorkflow::commandFinished(DismProcessResult result)
{
    flushDisplayLine();
    if (result.termination != DismProcessTermination::Normal || result.exitCode != 0) {
        const auto code = result.termination == DismProcessTermination::FailedToStart
            ? u"dism.start_failed"_s
            : result.termination == DismProcessTermination::Crashed
                ? u"dism.crashed"_s : u"dism.exit_code"_s;
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = code,
                  .message = result.error.isEmpty()
                      ? u"DISM завершился с ошибкой."_s : result.error});
        return;
    }
    if (++commandIndex_ < commands_.size()) {
        runNext();
        return;
    }
    if (parser_.health() == ComponentStoreHealth::Unknown) {
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = 0,
                  .code = u"dism.health_unknown"_s,
                  .message = u"DISM завершился, но состояние хранилища не распознано."_s});
        return;
    }
    complete({.requestId = request_.requestId,
              .status = SystemToolResultStatus::Completed,
              .exitCode = 0,
              .health = parser_.health()});
}

void SystemIntegrityWorkflow::emitDisplayLine(QString text)
{
    if (!text.isEmpty()) emit outputReady(request_.requestId, text);
}

void SystemIntegrityWorkflow::flushDisplayLine()
{
    if (pendingCarriageReturn_) {
        displayLine_.clear();
        pendingCarriageReturn_ = false;
    }
    if (!displayLine_.isEmpty()) {
        emitDisplayLine(displayLine_ + u'\n');
        displayLine_.clear();
    }
}

void SystemIntegrityWorkflow::complete(SystemToolResult result)
{
    if (!active_) return;
    active_ = false;
    log_.close();
    result.logPath = log_.path();
    emit finished(result);
}

} // namespace tweakopedia::execution
