#pragma once

#include <QFile>
#include <QMutex>
#include <QString>
#include <QtLogging>

namespace tweakopedia::app {

class SessionLogger final
{
public:
    explicit SessionLogger(QString logsRoot);
    ~SessionLogger();

    SessionLogger(const SessionLogger&) = delete;
    SessionLogger& operator=(const SessionLogger&) = delete;

    [[nodiscard]] bool setEnabled(bool enabled);
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] const QString& currentFilePath() const noexcept;
    [[nodiscard]] const QString& errorString() const noexcept;

private:
    static void messageHandler(
        QtMsgType type,
        const QMessageLogContext& context,
        const QString& message);
    void writeMessage(
        QtMsgType type,
        const QMessageLogContext& context,
        const QString& message);

    QString logsRoot_;
    QString currentFilePath_;
    QString errorString_;
    QFile file_;
    QtMessageHandler previousHandler_{};
    bool enabled_{};

    static QMutex handlerMutex_;
    static SessionLogger* activeLogger_;
};

} // namespace tweakopedia::app
