#include "execution/ExecutorServer.h"

#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

#include <windows.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

qint64 peerProcessId(QLocalSocket* socket)
{
    ULONG processId{};
    const auto handle = reinterpret_cast<HANDLE>(socket->socketDescriptor());
    return GetNamedPipeClientProcessId(handle, &processId) ? static_cast<qint64>(processId) : 0;
}

} // namespace

ExecutorServer::ExecutorServer(QObject* parent)
    : QObject(parent), server_(new QLocalServer(this))
{
    connect(server_, &QLocalServer::newConnection, this, &ExecutorServer::acceptConnections);
}

ExecutorServer::~ExecutorServer() = default;

bool ExecutorServer::listen(const QString& serverName, const QString& nonce)
{
    nonce_ = nonce;
    if (serverName.isEmpty() || nonce.isEmpty()) {
        error_ = u"Имя IPC-сервера и nonce обязательны."_s;
        return false;
    }
    if (!server_->listen(serverName)) {
        error_ = server_->errorString();
        return false;
    }
    return true;
}

void ExecutorServer::setExpectedProcessId(qint64 processId)
{
    expectedProcessId_ = processId;
    if (!pendingHello_.isEmpty() && pendingHelloSocket_) {
        const auto message = pendingHello_;
        auto* socket = pendingHelloSocket_;
        pendingHello_ = {};
        pendingHelloSocket_ = nullptr;
        handleMessage(socket, message);
    }
}

QString ExecutorServer::errorString() const
{
    return error_;
}

bool ExecutorServer::sendPlan(
    const QByteArray& encodedPlan,
    const QString& dataRoot,
    const QString& transactionDirectory)
{
    if (!authenticated_ || !client_) return false;
    return send(client_, {
        {u"type"_s, u"plan"_s},
        {u"plan_base64"_s, QString::fromLatin1(encodedPlan.toBase64())},
        {u"data_root"_s, dataRoot},
        {u"transaction_directory"_s, transactionDirectory},
    });
}

bool ExecutorServer::sendRollback(
    const QUuid& transactionId,
    const QString& dataRoot,
    const QString& transactionDirectory)
{
    if (!authenticated_ || !client_ || transactionId.isNull()) return false;
    return send(client_, {
        {u"type"_s, u"rollback"_s},
        {u"transaction_id"_s, transactionId.toString(QUuid::WithoutBraces)},
        {u"data_root"_s, dataRoot},
        {u"transaction_directory"_s, transactionDirectory},
    });
}

void ExecutorServer::acceptConnections()
{
    while (server_->hasPendingConnections()) {
        auto* socket = server_->nextPendingConnection();
        if (claimed_) {
            reject(socket, u"session.claimed"_s);
            continue;
        }
        claimed_ = true;
        client_ = socket;
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readMessages(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (socket == client_) emit clientDisconnected();
            socket->deleteLater();
        });
    }
}

void ExecutorServer::readMessages(QLocalSocket* socket)
{
    input_.append(socket->readAll());
    qsizetype newline{};
    while ((newline = input_.indexOf('\n')) >= 0) {
        const auto line = input_.left(newline);
        input_.remove(0, newline + 1);
        const auto document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            reject(socket, u"message.invalid"_s);
            return;
        }
        handleMessage(socket, document.object());
    }
}

void ExecutorServer::handleMessage(QLocalSocket* socket, const QJsonObject& message)
{
    const auto type = message.value(u"type"_s).toString();
    if (!authenticated_) {
        const auto reportedPid = message.value(u"pid"_s).toInteger();
        const auto nativePid = peerProcessId(socket);
        if (type != u"hello" || message.value(u"nonce"_s).toString() != nonce_) {
            reject(socket, u"handshake.invalid"_s);
            return;
        }
        if (expectedProcessId_ <= 0) {
            pendingHello_ = message;
            pendingHelloSocket_ = socket;
            return;
        }
        if (reportedPid != expectedProcessId_
            || nativePid != expectedProcessId_) {
            reject(socket, u"handshake.pid_mismatch"_s);
            return;
        }
        authenticated_ = true;
        (void)send(socket, {{u"type"_s, u"hello_ack"_s}});
        emit authenticated(reportedPid);
        return;
    }

    if (type == u"progress") {
        emit progressReceived(message.value(u"percent"_s).toInt(), message.value(u"message"_s).toString());
    } else if (type == u"result" && message.value(u"result"_s).isObject()) {
        emit completed(message.value(u"result"_s).toObject());
        (void)send(socket, {{u"type"_s, u"result_ack"_s}});
    } else {
        reject(socket, u"message.unexpected"_s);
    }
}

void ExecutorServer::reject(QLocalSocket* socket, const QString& code)
{
    (void)send(socket, {{u"type"_s, u"error"_s}, {u"code"_s, code}});
    socket->flush();
    socket->disconnectFromServer();
    if (socket == client_ && !authenticated_) {
        client_ = nullptr;
        claimed_ = false;
        input_.clear();
        pendingHello_ = {};
        pendingHelloSocket_ = nullptr;
    }
    emit clientRejected(code);
}

bool ExecutorServer::send(QLocalSocket* socket, const QJsonObject& message)
{
    if (!socket || socket->state() != QLocalSocket::ConnectedState) return false;
    auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    if (socket->write(bytes) != bytes.size()) return false;
    (void)socket->flush();
    return true;
}

} // namespace tweakopedia::execution
