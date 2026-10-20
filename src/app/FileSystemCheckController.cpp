#include "app/FileSystemCheckController.h"

#include <QLocale>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString formatBytes(quint64 bytes)
{
    constexpr quint64 gib = 1024ULL * 1024 * 1024;
    if (bytes >= gib) return u"%1 ГБ"_s.arg(QLocale().toString(bytes / gib));
    constexpr quint64 mib = 1024ULL * 1024;
    return u"%1 МБ"_s.arg(QLocale().toString(bytes / mib));
}

QVariantMap volumeMap(const platform::LocalVolume& volume)
{
    const auto title = volume.label.isEmpty()
        ? volume.target
        : u"%1 — %2"_s.arg(volume.target, volume.label);
    auto details = u"%1 · %2 свободно из %3"_s.arg(
        volume.fileSystem,
        formatBytes(volume.freeBytes),
        formatBytes(volume.totalBytes));
    if (volume.systemVolume) details += u" · Системный том"_s;
    return {{u"target"_s, volume.target},
            {u"title"_s, title},
            {u"details"_s, details},
            {u"systemVolume"_s, volume.systemVolume}};
}

} // namespace

FileSystemCheckController::FileSystemCheckController(
    platform::ILocalVolumeProvider& volumeProvider,
    execution::ISystemToolRunner& runner,
    ISystemIntegrityDesktopServices& desktopServices,
    QObject* parent)
    : QObject(parent)
    , volumeProvider_(&volumeProvider)
    , runner_(&runner)
    , desktopServices_(&desktopServices)
{
    connect(runner_, &execution::ISystemToolRunner::launchCancelled, this,
            [this](const QString& message) {
        if (!busy_) return;
        busy_ = false;
        pendingNewSession_ = false;
        state_ = hasVolumes() ? u"idle"_s : u"no_volumes"_s;
        stageText_ = message;
        errorCode_ = u"launch.cancelled"_s;
        emit changed();
    });
    connect(runner_, &execution::ISystemToolRunner::stageChanged,
            this, &FileSystemCheckController::handleStage);
    connect(runner_, &execution::ISystemToolRunner::outputReceived,
            this, &FileSystemCheckController::handleOutput);
    connect(runner_, &execution::ISystemToolRunner::finished,
            this, &FileSystemCheckController::handleResult);
    connect(runner_, &execution::ISystemToolRunner::failed,
            this, &FileSystemCheckController::handleFailure);
    rebuildVolumeModel();
}

QVariantList FileSystemCheckController::volumes() const { return volumes_; }
int FileSystemCheckController::selectedVolumeIndex() const { return selectedVolumeIndex_; }
bool FileSystemCheckController::hasVolumes() const { return !volumeData_.isEmpty(); }
QString FileSystemCheckController::state() const { return state_; }
QString FileSystemCheckController::stageText() const { return stageText_; }
QString FileSystemCheckController::outputText() const { return output_.text(); }
QString FileSystemCheckController::resultTitle() const { return resultTitle_; }
QString FileSystemCheckController::resultDescription() const { return resultDescription_; }
QString FileSystemCheckController::errorCode() const { return errorCode_; }
int FileSystemCheckController::exitCode() const { return exitCode_; }
QString FileSystemCheckController::sessionLogPath() const { return sessionLogPath_; }
bool FileSystemCheckController::busy() const { return busy_; }
bool FileSystemCheckController::canCheck() const { return !busy_ && hasVolumes(); }

bool FileSystemCheckController::refreshVolumes()
{
    if (busy_) return false;
    const auto preferredTarget = selectedVolumeIndex_ >= 0
        && selectedVolumeIndex_ < volumeData_.size()
        ? volumeData_.at(selectedVolumeIndex_).target : QString{};
    rebuildVolumeModel(preferredTarget);
    emit changed();
    return true;
}

bool FileSystemCheckController::setSelectedVolumeIndex(int index)
{
    if (busy_ || index < 0 || index >= volumeData_.size()) return false;
    if (selectedVolumeIndex_ == index) return true;
    selectedVolumeIndex_ = index;
    emit changed();
    return true;
}

bool FileSystemCheckController::startCheck()
{
    if (!canCheck()) return false;
    busy_ = true;
    pendingNewSession_ = true;
    state_ = u"awaiting_elevation"_s;
    stageText_ = u"Ожидание подтверждения прав администратора"_s;
    emit changed();
    const auto target = volumeData_.at(selectedVolumeIndex_).target;
    if (runner_->start(execution::SystemToolKind::FileSystem,
                       execution::SystemToolOperation::Check,
                       target)) {
        return true;
    }
    if (busy_) {
        handleFailure(u"launch.failed"_s,
                      u"Не удалось запустить исполнительный процесс."_s);
    }
    return false;
}

bool FileSystemCheckController::copyOutput()
{
    return desktopServices_->copyText(output_.text());
}

bool FileSystemCheckController::openSessionLog()
{
    if (sessionLogPath_.isEmpty() || !desktopServices_->fileExists(sessionLogPath_)) {
        errorCode_ = u"session_log.not_found"_s;
        resultDescription_ = u"Журнал текущей проверки не найден."_s;
        emit changed();
        return false;
    }
    if (!desktopServices_->openFile(sessionLogPath_)) {
        errorCode_ = u"session_log.open_failed"_s;
        resultDescription_ = u"Не удалось открыть журнал текущей проверки."_s;
        emit changed();
        return false;
    }
    return true;
}

void FileSystemCheckController::beginConfirmedSession()
{
    if (!pendingNewSession_) return;
    pendingNewSession_ = false;
    output_.clear();
    resultTitle_.clear();
    resultDescription_.clear();
    errorCode_.clear();
    exitCode_ = -1;
    sessionLogPath_.clear();
}

void FileSystemCheckController::handleStage(
    const execution::SystemToolStageEvent& event)
{
    if (!busy_ || event.stage != execution::SystemToolStage::CheckFileSystem) return;
    beginConfirmedSession();
    state_ = u"checking"_s;
    stageText_ = u"Проверка файловой системы"_s;
    emit changed();
}

void FileSystemCheckController::handleOutput(
    const execution::SystemToolOutputEvent& event)
{
    if (!busy_) return;
    output_.append(event.text);
    emit changed();
}

void FileSystemCheckController::handleResult(const execution::SystemToolResult& result)
{
    if (!busy_) return;
    busy_ = false;
    pendingNewSession_ = false;
    stageText_.clear();
    exitCode_ = result.exitCode;
    sessionLogPath_ = result.logPath;
    errorCode_ = result.code;

    if (result.status == execution::SystemToolResultStatus::Failed) {
        state_ = u"failed"_s;
        resultTitle_ = u"Проверка не завершена"_s;
        resultDescription_ = result.message.isEmpty()
            ? u"CHKDSK не вернул подтверждённый результат."_s : result.message;
        emit changed();
        return;
    }

    switch (result.outcome) {
    case execution::SystemToolOutcome::Healthy:
        state_ = u"healthy"_s;
        resultTitle_ = u"Файловая система исправна"_s;
        resultDescription_ = u"CHKDSK не обнаружил ошибок. Содержимое тома не изменялось."_s;
        break;
    case execution::SystemToolOutcome::IssuesFound:
        state_ = u"issues_found"_s;
        resultTitle_ = u"Обнаружены ошибки файловой системы"_s;
        resultDescription_ = u"Проверка выполнялась без исправлений. Подробности доступны в выводе и журнале сеанса."_s;
        break;
    case execution::SystemToolOutcome::Incomplete:
        state_ = u"incomplete"_s;
        resultTitle_ = u"Проверка не завершена"_s;
        resultDescription_ = u"CHKDSK не смог полностью проверить выбранный том. Содержимое тома не изменялось."_s;
        break;
    case execution::SystemToolOutcome::Repairable:
    case execution::SystemToolOutcome::NonRepairable:
    case execution::SystemToolOutcome::Unknown:
        state_ = u"failed"_s;
        resultTitle_ = u"Результат проверки не распознан"_s;
        resultDescription_ = u"CHKDSK завершился без подтверждённого состояния файловой системы."_s;
        if (errorCode_.isEmpty()) errorCode_ = u"chkdsk.result_unknown"_s;
        break;
    }
    emit changed();
}

void FileSystemCheckController::handleFailure(
    const QString& code, const QString& message)
{
    if (!busy_) return;
    busy_ = false;
    pendingNewSession_ = false;
    state_ = u"failed"_s;
    stageText_.clear();
    resultTitle_ = u"Проверка не выполнена"_s;
    resultDescription_ = message;
    errorCode_ = code;
    emit changed();
}

void FileSystemCheckController::rebuildVolumeModel(const QString& preferredTarget)
{
    volumeData_ = volumeProvider_->volumes();
    volumes_.clear();
    volumes_.reserve(volumeData_.size());
    selectedVolumeIndex_ = volumeData_.isEmpty() ? -1 : 0;
    for (qsizetype index = 0; index < volumeData_.size(); ++index) {
        volumes_.append(volumeMap(volumeData_.at(index)));
        if (!preferredTarget.isEmpty()
            && volumeData_.at(index).target.compare(preferredTarget,
                                                    Qt::CaseInsensitive) == 0) {
            selectedVolumeIndex_ = static_cast<int>(index);
        }
    }
    if (volumeData_.isEmpty()) state_ = u"no_volumes"_s;
    else if (state_ == u"no_volumes"_s) state_ = u"idle"_s;
}

} // namespace tweakopedia::app
