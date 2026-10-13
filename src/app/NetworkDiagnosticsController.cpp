#include "app/NetworkDiagnosticsController.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

constexpr qsizetype kMaximumOutputBytes = 2 * 1024 * 1024;
const auto kTruncationMarker = QByteArray("[Начало вывода удалено из экранного буфера]\n");

bool isNetworkStage(execution::SystemToolStage stage)
{
    switch (stage) {
    case execution::SystemToolStage::CollectNetworkAdapters:
    case execution::SystemToolStage::CheckNetworkDns:
    case execution::SystemToolStage::CheckNetworkProxy:
    case execution::SystemToolStage::CheckNetworkHosts:
    case execution::SystemToolStage::CheckNetworkRoutes:
    case execution::SystemToolStage::CheckNetworkFirewall:
        return true;
    default:
        return false;
    }
}

QString stageTitle(execution::SystemToolStage stage)
{
    switch (stage) {
    case execution::SystemToolStage::CollectNetworkAdapters: return u"Сбор данных сетевых адаптеров"_s;
    case execution::SystemToolStage::CheckNetworkDns: return u"Проверка DNS"_s;
    case execution::SystemToolStage::CheckNetworkProxy: return u"Проверка прокси"_s;
    case execution::SystemToolStage::CheckNetworkHosts: return u"Проверка файла hosts"_s;
    case execution::SystemToolStage::CheckNetworkRoutes: return u"Проверка маршрутов"_s;
    case execution::SystemToolStage::CheckNetworkFirewall: return u"Проверка Windows Firewall"_s;
    default: return {};
    }
}

} // namespace

NetworkDiagnosticsController::NetworkDiagnosticsController(
    execution::ISystemToolRunner& runner,
    ISystemIntegrityDesktopServices& desktopServices,
    QObject* parent)
    : QObject(parent), runner_(&runner), desktopServices_(&desktopServices)
{
    connect(runner_, &execution::ISystemToolRunner::launchCancelled, this,
            [this](const QString& message) {
        if (!busy_) return;
        busy_ = false;
        pendingNewSession_ = false;
        state_ = u"idle"_s;
        stageText_ = message;
        errorCode_ = u"launch.cancelled"_s;
        emit changed();
    });
    connect(runner_, &execution::ISystemToolRunner::stageChanged,
            this, &NetworkDiagnosticsController::handleStage);
    connect(runner_, &execution::ISystemToolRunner::outputReceived,
            this, &NetworkDiagnosticsController::handleOutput);
    connect(runner_, &execution::ISystemToolRunner::finished,
            this, &NetworkDiagnosticsController::handleResult);
    connect(runner_, &execution::ISystemToolRunner::failed,
            this, &NetworkDiagnosticsController::handleFailure);
}

QString NetworkDiagnosticsController::state() const { return state_; }
QString NetworkDiagnosticsController::stageText() const { return stageText_; }
QString NetworkDiagnosticsController::outputText() const { return outputText_; }
QString NetworkDiagnosticsController::resultTitle() const { return resultTitle_; }
QString NetworkDiagnosticsController::resultDescription() const { return resultDescription_; }
QString NetworkDiagnosticsController::errorCode() const { return errorCode_; }
QString NetworkDiagnosticsController::sessionLogPath() const { return sessionLogPath_; }
QVariantList NetworkDiagnosticsController::checks() const { return checks_; }
int NetworkDiagnosticsController::okCount() const { return okCount_; }
int NetworkDiagnosticsController::attentionCount() const { return attentionCount_; }
int NetworkDiagnosticsController::unavailableCount() const { return unavailableCount_; }
bool NetworkDiagnosticsController::hasAttention() const { return attentionCount_ > 0; }
bool NetworkDiagnosticsController::busy() const { return busy_; }
bool NetworkDiagnosticsController::canStart() const { return !busy_; }

bool NetworkDiagnosticsController::startDiagnostics()
{
    if (!canStart()) return false;
    busy_ = true;
    pendingNewSession_ = true;
    state_ = u"awaiting_elevation"_s;
    stageText_ = u"Ожидание подтверждения прав администратора"_s;
    emit changed();
    if (runner_->start(execution::SystemToolKind::NetworkDiagnostics,
                       execution::SystemToolOperation::Diagnose)) {
        return true;
    }
    if (busy_) handleFailure(u"launch.failed"_s, u"Не удалось запустить исполнительный процесс."_s);
    return false;
}

bool NetworkDiagnosticsController::copyOutput()
{
    return desktopServices_->copyText(outputText_);
}

bool NetworkDiagnosticsController::openSessionLog()
{
    if (sessionLogPath_.isEmpty() || !desktopServices_->fileExists(sessionLogPath_)) {
        errorCode_ = u"session_log.not_found"_s;
        resultDescription_ = u"Журнал текущей диагностики не найден."_s;
        emit changed();
        return false;
    }
    if (!desktopServices_->openFile(sessionLogPath_)) {
        errorCode_ = u"session_log.open_failed"_s;
        resultDescription_ = u"Не удалось открыть журнал текущей диагностики."_s;
        emit changed();
        return false;
    }
    return true;
}

void NetworkDiagnosticsController::beginConfirmedSession()
{
    if (!pendingNewSession_) return;
    pendingNewSession_ = false;
    outputText_.clear();
    resultTitle_.clear();
    resultDescription_.clear();
    errorCode_.clear();
    sessionLogPath_.clear();
    checks_.clear();
    okCount_ = attentionCount_ = unavailableCount_ = 0;
}

void NetworkDiagnosticsController::handleStage(const execution::SystemToolStageEvent& event)
{
    if (!busy_ || !isNetworkStage(event.stage)) return;
    beginConfirmedSession();
    state_ = u"checking"_s;
    stageText_ = stageTitle(event.stage);
    emit changed();
}

void NetworkDiagnosticsController::handleOutput(const execution::SystemToolOutputEvent& event)
{
    if (!busy_) return;
    outputText_ += event.text;
    trimOutput();
    emit changed();
}

void NetworkDiagnosticsController::handleResult(const execution::SystemToolResult& result)
{
    if (!busy_) return;
    beginConfirmedSession();
    busy_ = false;
    pendingNewSession_ = false;
    stageText_.clear();
    sessionLogPath_ = result.logPath;
    errorCode_ = result.code;
    if (result.status == execution::SystemToolResultStatus::Failed) {
        state_ = u"failed"_s;
        resultTitle_ = u"Диагностика не завершена"_s;
        resultDescription_ = result.message.isEmpty()
            ? u"Исполнитель не вернул подтверждённый результат."_s : result.message;
        emit changed();
        return;
    }

    checks_ = result.details.value(u"checks"_s).toArray().toVariantList();
    okCount_ = qMax(0, result.details.value(u"ok_count"_s).toInt());
    attentionCount_ = qMax(0, result.details.value(u"attention_count"_s).toInt());
    unavailableCount_ = qMax(0, result.details.value(u"unavailable_count"_s).toInt());
    switch (result.outcome) {
    case execution::SystemToolOutcome::Healthy:
        state_ = u"healthy"_s;
        resultTitle_ = u"Проблемы не обнаружены"_s;
        resultDescription_ = u"Все доступные проверки завершены без замечаний."_s;
        break;
    case execution::SystemToolOutcome::IssuesFound:
        state_ = u"issues_found"_s;
        resultTitle_ = u"Обнаружены параметры, требующие внимания"_s;
        resultDescription_ = u"Просмотрите отмеченные блоки и подробный вывод перед изменением настроек."_s;
        break;
    case execution::SystemToolOutcome::Incomplete:
        state_ = u"incomplete"_s;
        resultTitle_ = u"Диагностика выполнена не полностью"_s;
        resultDescription_ = u"Часть данных получить не удалось. Остальные результаты сохранены."_s;
        break;
    default:
        state_ = u"failed"_s;
        resultTitle_ = u"Результат не распознан"_s;
        resultDescription_ = u"Исполнитель вернул неподдерживаемое состояние."_s;
        if (errorCode_.isEmpty()) errorCode_ = u"network.result_unknown"_s;
        break;
    }
    emit changed();
}

void NetworkDiagnosticsController::handleFailure(const QString& code, const QString& message)
{
    if (!busy_) return;
    busy_ = false;
    pendingNewSession_ = false;
    state_ = u"failed"_s;
    stageText_.clear();
    resultTitle_ = u"Диагностика не выполнена"_s;
    resultDescription_ = message;
    errorCode_ = code;
    emit changed();
}

void NetworkDiagnosticsController::trimOutput()
{
    auto bytes = outputText_.toUtf8();
    if (bytes.size() <= kMaximumOutputBytes) return;
    const auto tailBytes = kMaximumOutputBytes - kTruncationMarker.size();
    outputText_ = QString::fromUtf8(kTruncationMarker + bytes.right(tailBytes));
}

} // namespace tweakopedia::app
