#include "execution/SystemToolSession.h"

#include <QtConcurrent/QtConcurrentRun>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

SystemToolSession::SystemToolSession(
    IShellExecuteBackend& shellBackend,
    QString executorPath,
    int pollIntervalMs,
    int inactivityTimeoutMs,
    QObject* parent)
    : ISystemToolRunner(parent)
    , shellBackend_(&shellBackend)
    , launcher_(shellBackend)
    , executorPath_(std::move(executorPath))
    , inactivityTimeoutMs_(inactivityTimeoutMs)
{
    processTimer_.setInterval(pollIntervalMs);
    watchdogTimer_.setInterval(qMax(5, qMin(pollIntervalMs, inactivityTimeoutMs / 4)));
    connect(&launchWatcher_, &QFutureWatcher<ExecutorLaunchSession>::finished,
            this, &SystemToolSession::launchFinished);
    connect(&processTimer_, &QTimer::timeout, this, &SystemToolSession::pollProcess);
    connect(&watchdogTimer_, &QTimer::timeout, this, &SystemToolSession::checkWatchdog);
}

SystemToolSession::~SystemToolSession()
{
    processTimer_.stop();
    watchdogTimer_.stop();
    if (launchSession_.started) launcher_.close(launchSession_);
}

bool SystemToolSession::start(SystemToolOperation operation)
{
    if (active_ || launchWatcher_.isRunning()) return false;
    active_ = true;
    resultReceived_ = false;
    request_ = {QUuid::createUuid(), operation};
    server_ = std::make_unique<ExecutorServer>();
    const auto serverName = u"tweakopedia-tool-"_s
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!server_->listen(serverName, nonce)) {
        const auto message = server_->errorString();
        resetSession(false);
        emit failed(u"ipc.listen_failed"_s, message);
        return false;
    }

    connect(server_.get(), &ExecutorServer::authenticated, this, [this](qint64) {
        touchActivity();
        if (!server_->sendToolRequest(request_)) {
            fail(u"ipc.send_failed"_s, u"Не удалось отправить запрос Executor."_s);
        }
    });
    connect(server_.get(), &ExecutorServer::toolStageReceived, this,
            [this](const SystemToolStageEvent& event) {
        touchActivity();
        emit stageChanged(event);
    });
    connect(server_.get(), &ExecutorServer::toolOutputReceived, this,
            [this](const SystemToolOutputEvent& event) {
        touchActivity();
        emit outputReceived(event);
    });
    connect(server_.get(), &ExecutorServer::toolProgressReceived, this,
            [this](const SystemToolProgressEvent& event) {
        touchActivity();
        emit progressChanged(event);
    });
    connect(server_.get(), &ExecutorServer::toolResultReceived, this,
            [this](const SystemToolResult& result) {
        touchActivity();
        resultReceived_ = true;
        emit finished(result);
    });
    connect(server_.get(), &ExecutorServer::clientRejected, this,
            [this](const QString& code) {
        if (!resultReceived_) fail(code, u"Executor отклонил IPC-сеанс."_s);
    });
    connect(server_.get(), &ExecutorServer::clientDisconnected, this, [this] {
        if (!resultReceived_) fail(u"ipc.disconnected"_s,
                                   u"Соединение с Executor было разорвано."_s);
    });

    activity_.start();
    watchdogTimer_.start();
    auto* backend = shellBackend_;
    const auto executable = executorPath_;
    launchWatcher_.setFuture(QtConcurrent::run(
        [backend, executable, serverName, nonce] {
            ExecutorLauncher launcher(*backend);
            return launcher.start(executable, serverName, nonce);
        }));
    return true;
}

void SystemToolSession::launchFinished()
{
    if (!active_) return;
    launchSession_ = launchWatcher_.result();
    if (!launchSession_.started) {
        const auto cancelled = launchSession_.code == u"launch.cancelled";
        const auto message = launchSession_.message;
        resetSession(false);
        if (cancelled) emit launchCancelled(message);
        else emit failed(launchSession_.code.isEmpty() ? u"launch.failed"_s : launchSession_.code,
                         message);
        return;
    }
    server_->setExpectedProcessId(launchSession_.processId);
    touchActivity();
    processTimer_.start();
}

void SystemToolSession::pollProcess()
{
    if (!active_ || !launchSession_.started) return;
    const auto status = launcher_.poll(launchSession_);
    if (status == ExecutorPollStatus::Running) return;
    if (status == ExecutorPollStatus::Failed) {
        fail(u"launch.wait_failed"_s, u"Не удалось проверить состояние Executor."_s);
        return;
    }
    launcher_.close(launchSession_);
    launchSession_ = {};
    if (!resultReceived_) {
        fail(u"launch.exited_early"_s, u"Executor завершился без итогового результата."_s);
        return;
    }
    resetSession(false);
}

void SystemToolSession::checkWatchdog()
{
    if (active_ && activity_.isValid()
        && activity_.elapsed() >= inactivityTimeoutMs_) {
        fail(u"tool.timeout"_s, u"Executor перестал отвечать."_s, true);
    }
}

void SystemToolSession::touchActivity()
{
    if (activity_.isValid()) activity_.restart();
}

void SystemToolSession::fail(QString code, QString message, bool terminateProcess)
{
    if (!active_) return;
    if (terminateProcess && launchSession_.started) {
        (void)launcher_.terminate(launchSession_);
    }
    resetSession(true);
    emit failed(code, message);
}

void SystemToolSession::resetSession(bool closeHandle)
{
    processTimer_.stop();
    watchdogTimer_.stop();
    if (closeHandle && launchSession_.started) launcher_.close(launchSession_);
    launchSession_ = {};
    server_.reset();
    active_ = false;
    resultReceived_ = false;
}

} // namespace tweakopedia::execution
