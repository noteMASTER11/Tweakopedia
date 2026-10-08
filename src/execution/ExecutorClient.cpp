#include "execution/ExecutorClient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalSocket>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

ExecutorClient::ExecutorClient(QObject* parent)
    : QObject(parent), socket_(new QLocalSocket(this))
{
    connect(socket_, &QLocalSocket::connected, this, [this] {
        (void)send({
            {u"type"_s, u"hello"_s},
            {u"nonce"_s, nonce_},
            {u"pid"_s, processId_},
        });
    });
    connect(socket_, &QLocalSocket::readyRead, this, &ExecutorClient::readMessages);
    connect(socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (socket_->state() == QLocalSocket::UnconnectedState) emit failed(u"ipc.connection_failed"_s);
    });
}

ExecutorClient::~ExecutorClient() = default;

void ExecutorClient::connectToServer(const QString& serverName, const QString& nonce, qint64 processId)
{
    nonce_ = nonce;
    processId_ = processId > 0 ? processId : QCoreApplication::applicationPid();
    socket_->connectToServer(serverName, QIODevice::ReadWrite);
}

bool ExecutorClient::sendProgress(int percent, const QString& message)
{
    return send({{u"type"_s, u"progress"_s}, {u"percent"_s, percent}, {u"message"_s, message}});
}

bool ExecutorClient::sendResult(const QJsonObject& result)
{
    return send({{u"type"_s, u"result"_s}, {u"result"_s, result}});
}

void ExecutorClient::readMessages()
{
    input_.append(socket_->readAll());
    qsizetype newline{};
    while ((newline = input_.indexOf('\n')) >= 0) {
        const auto line = input_.left(newline);
        input_.remove(0, newline + 1);
        const auto document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            emit failed(u"message.invalid"_s);
            continue;
        }
        handleMessage(document.object());
    }
}

void ExecutorClient::handleMessage(const QJsonObject& message)
{
    const auto type = message.value(u"type"_s).toString();
    if (type == u"hello_ack") {
        emit authenticated();
    } else if (type == u"plan") {
        emit planReceived(
            QByteArray::fromBase64(message.value(u"plan_base64"_s).toString().toLatin1()),
            message.value(u"data_root"_s).toString(),
            message.value(u"transaction_directory"_s).toString());
    } else if (type == u"error") {
        emit failed(message.value(u"code"_s).toString());
    } else {
        emit failed(u"message.unexpected"_s);
    }
}

bool ExecutorClient::send(const QJsonObject& message)
{
    if (socket_->state() != QLocalSocket::ConnectedState) return false;
    auto bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return socket_->write(bytes) == bytes.size() && socket_->flush();
}

} // namespace tweakopedia::execution
