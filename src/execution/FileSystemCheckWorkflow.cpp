#include "execution/FileSystemCheckWorkflow.h"

#include <QDir>
#include <QRegularExpression>
#include <QSysInfo>

#include <windows.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

QString systemChkdskPath()
{
    wchar_t directory[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(directory, MAX_PATH + 1);
    if (length == 0 || length > MAX_PATH) return u"chkdsk.exe"_s;
    return QDir(QString::fromWCharArray(directory, static_cast<qsizetype>(length)))
        .filePath(u"chkdsk.exe"_s);
}

} // namespace

FileSystemCheckWorkflow::FileSystemCheckWorkflow(
    IDismProcessRunner& runner,
    QString logsRoot,
    QObject* parent)
    : QObject(parent), runner_(&runner), logsRoot_(std::move(logsRoot))
{
}

bool FileSystemCheckWorkflow::start(const SystemToolRequest& request)
{
    static const QRegularExpression driveTarget(u"^[A-Z]:$"_s);
    if (active_ || request.requestId.isNull()
        || request.tool != SystemToolKind::FileSystem
        || request.operation != SystemToolOperation::Check
        || !driveTarget.match(request.target).hasMatch()) {
        return false;
    }
    request_ = request;
    if (!log_.open(logsRoot_, request.tool, request.operation,
                   {u"Windows: "_s + QSysInfo::prettyProductName(),
                    u"Architecture: "_s + QSysInfo::currentCpuArchitecture(),
                    u"Volume: "_s + request.target})) {
        emit finished({.requestId = request.requestId,
                       .status = SystemToolResultStatus::Failed,
                       .code = u"tool.log_open_failed"_s,
                       .message = log_.errorString()});
        return false;
    }

    displayLine_.clear();
    outputDecoder_.resetState();
    pendingCarriageReturn_ = false;
    active_ = true;
    emit stageChanged({request.requestId, SystemToolStage::CheckFileSystem});
    const QStringList arguments{request.target};
    const auto executable = systemChkdskPath();
    log_.appendCommand(u"chkdsk.exe"_s, arguments);
    emit outputReady(request.requestId, u"> chkdsk.exe "_s + request.target + u"\n"_s);
    if (!runner_->start(
            executable,
            arguments,
            [this](const QByteArray& bytes) { consumeOutput(bytes); },
            [this](DismProcessResult result) { processFinished(std::move(result)); })) {
        complete({.requestId = request.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .code = u"chkdsk.runner_busy"_s,
                  .message = u"Не удалось запустить CHKDSK."_s});
    }
    return active_;
}

bool FileSystemCheckWorkflow::active() const noexcept
{
    return active_;
}

void FileSystemCheckWorkflow::consumeOutput(const QByteArray& bytes)
{
    log_.appendOutput(bytes);
    const QString text = outputDecoder_(bytes);
    for (const auto character : text) {
        if (pendingCarriageReturn_) {
            if (character == u'\n') {
                emitDisplayLine(displayLine_ + u'\n');
                displayLine_.clear();
                pendingCarriageReturn_ = false;
                continue;
            }
            if (!displayLine_.isEmpty()) emitDisplayLine(displayLine_ + u'\n');
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

void FileSystemCheckWorkflow::processFinished(DismProcessResult result)
{
    flushDisplayLine();
    if (result.termination != DismProcessTermination::Normal) {
        const auto code = result.termination == DismProcessTermination::FailedToStart
            ? u"chkdsk.start_failed"_s : u"chkdsk.crashed"_s;
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = code,
                  .message = result.error.isEmpty()
                      ? u"CHKDSK завершился аварийно."_s : result.error});
        return;
    }

    SystemToolOutcome outcome = SystemToolOutcome::Unknown;
    QString code;
    QString message;
    switch (result.exitCode) {
    case 0:
        outcome = SystemToolOutcome::Healthy;
        break;
    case 2:
        outcome = SystemToolOutcome::IssuesFound;
        code = u"chkdsk.issues_found"_s;
        break;
    case 1:
        outcome = SystemToolOutcome::Incomplete;
        code = u"chkdsk.unexpected_repair"_s;
        message = u"CHKDSK вернул код исправления в режиме только чтения."_s;
        break;
    case 3:
        outcome = SystemToolOutcome::Incomplete;
        code = u"chkdsk.incomplete"_s;
        break;
    default:
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = u"chkdsk.exit_code"_s,
                  .message = u"CHKDSK завершился с непредусмотренным кодом."_s});
        return;
    }
    complete({.requestId = request_.requestId,
              .status = SystemToolResultStatus::Completed,
              .exitCode = result.exitCode,
              .outcome = outcome,
              .code = code,
              .message = message});
}

void FileSystemCheckWorkflow::emitDisplayLine(QString text)
{
    if (!text.isEmpty()) emit outputReady(request_.requestId, text);
}

void FileSystemCheckWorkflow::flushDisplayLine()
{
    if (pendingCarriageReturn_) {
        if (!displayLine_.isEmpty()) emitDisplayLine(displayLine_ + u'\n');
        displayLine_.clear();
        pendingCarriageReturn_ = false;
    } else if (!displayLine_.isEmpty()) {
        emitDisplayLine(displayLine_ + u'\n');
        displayLine_.clear();
    }
}

void FileSystemCheckWorkflow::complete(SystemToolResult result)
{
    if (!active_) return;
    active_ = false;
    log_.close();
    result.logPath = log_.path();
    emit finished(result);
}

} // namespace tweakopedia::execution
