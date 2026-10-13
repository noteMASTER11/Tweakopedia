#include "app/SystemIntegrityController.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

constexpr qsizetype kMaximumOutputBytes = 2 * 1024 * 1024;
const auto kTruncationMarker = QByteArray("[Начало вывода удалено из экранного буфера]\n");

QString stateForStage(execution::SystemToolStage stage)
{
    switch (stage) {
    case execution::SystemToolStage::ScanHealth: return u"scanning"_s;
    case execution::SystemToolStage::CheckHealth: return u"checking"_s;
    case execution::SystemToolStage::RestoreHealth: return u"repairing"_s;
    case execution::SystemToolStage::VerifyRestore: return u"verifying_repair"_s;
    }
    return u"failed"_s;
}

QString textForStage(execution::SystemToolStage stage)
{
    switch (stage) {
    case execution::SystemToolStage::ScanHealth:
        return u"Проверка хранилища компонентов"_s;
    case execution::SystemToolStage::CheckHealth:
        return u"Определение состояния хранилища"_s;
    case execution::SystemToolStage::RestoreHealth:
        return u"Восстановление хранилища компонентов"_s;
    case execution::SystemToolStage::VerifyRestore:
        return u"Контрольная проверка"_s;
    }
    return {};
}

} // namespace

SystemIntegrityController::SystemIntegrityController(
    execution::ISystemToolRunner& runner,
    ISystemIntegrityDesktopServices& desktopServices,
    QObject* parent)
    : QObject(parent)
    , runner_(&runner)
    , desktopServices_(&desktopServices)
{
    connect(runner_, &execution::ISystemToolRunner::launchCancelled, this,
            [this](const QString& message) {
        busy_ = false;
        pendingNewSession_ = false;
        state_ = u"idle"_s;
        stageText_ = message;
        progressKnown_ = false;
        errorCode_ = u"launch.cancelled"_s;
        emit changed();
    });
    connect(runner_, &execution::ISystemToolRunner::stageChanged,
            this, &SystemIntegrityController::handleStage);
    connect(runner_, &execution::ISystemToolRunner::outputReceived,
            this, &SystemIntegrityController::handleOutput);
    connect(runner_, &execution::ISystemToolRunner::progressChanged,
            this, &SystemIntegrityController::handleProgress);
    connect(runner_, &execution::ISystemToolRunner::finished,
            this, &SystemIntegrityController::handleResult);
    connect(runner_, &execution::ISystemToolRunner::failed,
            this, &SystemIntegrityController::handleFailure);
}

QString SystemIntegrityController::state() const { return state_; }
QString SystemIntegrityController::stageText() const { return stageText_; }
int SystemIntegrityController::progress() const { return progress_; }
bool SystemIntegrityController::progressKnown() const { return progressKnown_; }
QString SystemIntegrityController::outputText() const { return outputText_; }
QString SystemIntegrityController::resultTitle() const { return resultTitle_; }
QString SystemIntegrityController::resultDescription() const { return resultDescription_; }
QString SystemIntegrityController::errorCode() const { return errorCode_; }
int SystemIntegrityController::exitCode() const { return exitCode_; }
QString SystemIntegrityController::sessionLogPath() const { return sessionLogPath_; }
QString SystemIntegrityController::dismLogPath() const { return desktopServices_->dismLogPath(); }
bool SystemIntegrityController::busy() const { return busy_; }
bool SystemIntegrityController::canScan() const { return !busy_; }
bool SystemIntegrityController::canRestore() const
{
    return !busy_ && state_ == u"repairable";
}

bool SystemIntegrityController::startScan()
{
    return start(execution::SystemToolOperation::Scan);
}

bool SystemIntegrityController::startRestore()
{
    if (!canRestore()) return false;
    return start(execution::SystemToolOperation::Restore);
}

bool SystemIntegrityController::copyOutput()
{
    return desktopServices_->copyText(outputText_);
}

bool SystemIntegrityController::openDismLog()
{
    const auto path = desktopServices_->dismLogPath();
    if (!desktopServices_->fileExists(path)) {
        errorCode_ = u"dism_log.not_found"_s;
        resultDescription_ = u"Системный журнал DISM не найден: %1"_s.arg(path);
        emit changed();
        return false;
    }
    if (!desktopServices_->openFile(path)) {
        errorCode_ = u"dism_log.open_failed"_s;
        resultDescription_ = u"Не удалось открыть системный журнал DISM."_s;
        emit changed();
        return false;
    }
    return true;
}

bool SystemIntegrityController::start(execution::SystemToolOperation operation)
{
    if (busy_) return false;
    operation_ = operation;
    busy_ = true;
    pendingNewSession_ = true;
    state_ = u"awaiting_elevation"_s;
    stageText_ = u"Ожидание подтверждения прав администратора"_s;
    progressKnown_ = false;
    emit changed();
    if (runner_->start(operation)) return true;

    if (busy_) {
        handleFailure(u"launch.failed"_s, u"Не удалось запустить исполнительный процесс."_s);
    }
    return false;
}

void SystemIntegrityController::beginConfirmedSession()
{
    if (!pendingNewSession_) return;
    pendingNewSession_ = false;
    outputText_.clear();
    resultTitle_.clear();
    resultDescription_.clear();
    errorCode_.clear();
    exitCode_ = -1;
    sessionLogPath_.clear();
}

void SystemIntegrityController::handleStage(const execution::SystemToolStageEvent& event)
{
    beginConfirmedSession();
    busy_ = true;
    state_ = stateForStage(event.stage);
    stageText_ = textForStage(event.stage);
    progress_ = 0;
    progressKnown_ = false;
    emit changed();
}

void SystemIntegrityController::handleOutput(const execution::SystemToolOutputEvent& event)
{
    outputText_ += event.text;
    trimOutput();
    emit changed();
}

void SystemIntegrityController::handleProgress(const execution::SystemToolProgressEvent& event)
{
    progress_ = event.percent;
    progressKnown_ = true;
    emit changed();
}

void SystemIntegrityController::handleResult(const execution::SystemToolResult& result)
{
    busy_ = false;
    pendingNewSession_ = false;
    progressKnown_ = false;
    stageText_.clear();
    exitCode_ = result.exitCode;
    sessionLogPath_ = result.logPath;
    errorCode_ = result.code;

    if (result.status == execution::SystemToolResultStatus::Failed) {
        state_ = u"failed"_s;
        resultTitle_ = u"Проверка не завершена"_s;
        resultDescription_ = result.message.isEmpty()
            ? u"DISM не вернул подтверждённый результат."_s : result.message;
        emit changed();
        return;
    }

    switch (result.health) {
    case execution::ComponentStoreHealth::Healthy:
        state_ = u"healthy"_s;
        resultTitle_ = operation_ == execution::SystemToolOperation::Restore
            ? u"Хранилище компонентов восстановлено"_s
            : u"Хранилище компонентов исправно"_s;
        resultDescription_ = u"Повреждения хранилища компонентов не обнаружены."_s;
        break;
    case execution::ComponentStoreHealth::Repairable:
        state_ = u"repairable"_s;
        resultTitle_ = u"Обнаружены исправимые повреждения"_s;
        resultDescription_ = u"DISM может восстановить хранилище компонентов."_s;
        break;
    case execution::ComponentStoreHealth::NonRepairable:
        state_ = u"non_repairable"_s;
        resultTitle_ = u"Автоматическое восстановление недоступно"_s;
        resultDescription_ = u"DISM сообщил, что хранилище нельзя восстановить автоматически."_s;
        break;
    case execution::ComponentStoreHealth::Unknown:
        state_ = u"failed"_s;
        resultTitle_ = u"Результат проверки не распознан"_s;
        resultDescription_ = u"DISM завершился без подтверждённого состояния хранилища."_s;
        if (errorCode_.isEmpty()) errorCode_ = u"dism.result_unknown"_s;
        break;
    }
    emit changed();
}

void SystemIntegrityController::handleFailure(const QString& code, const QString& message)
{
    busy_ = false;
    pendingNewSession_ = false;
    progressKnown_ = false;
    state_ = u"failed"_s;
    stageText_.clear();
    resultTitle_ = u"Операция не выполнена"_s;
    resultDescription_ = message;
    errorCode_ = code;
    emit changed();
}

void SystemIntegrityController::trimOutput()
{
    auto bytes = outputText_.toUtf8();
    if (bytes.size() <= kMaximumOutputBytes) return;
    const auto tailBytes = kMaximumOutputBytes - kTruncationMarker.size();
    bytes = kTruncationMarker + bytes.right(tailBytes);
    outputText_ = QString::fromUtf8(bytes);
}

} // namespace tweakopedia::app
