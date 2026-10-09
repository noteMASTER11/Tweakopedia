#include "app/SessionLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return u"DEBUG"_s;
    case QtInfoMsg: return u"INFO"_s;
    case QtWarningMsg: return u"WARNING"_s;
    case QtCriticalMsg: return u"CRITICAL"_s;
    case QtFatalMsg: return u"FATAL"_s;
    }
    return u"UNKNOWN"_s;
}

QString contextText(const QMessageLogContext& context)
{
    QStringList parts;
    if (context.category && *context.category) {
        parts.append(QString::fromUtf8(context.category));
    }
    if (context.file && *context.file) {
        auto source = QString::fromUtf8(context.file);
        if (context.line > 0) source += u":"_s + QString::number(context.line);
        parts.append(std::move(source));
    }
    if (context.function && *context.function) {
        parts.append(QString::fromUtf8(context.function));
    }
    return parts.join(u" | "_s);
}

} // namespace

QMutex SessionLogger::handlerMutex_;
SessionLogger* SessionLogger::activeLogger_{};

SessionLogger::SessionLogger(QString logsRoot)
    : logsRoot_(QDir::cleanPath(std::move(logsRoot)))
{
}

SessionLogger::~SessionLogger()
{
    (void)setEnabled(false);
}

bool SessionLogger::setEnabled(bool enabled)
{
    if (enabled == enabled_) return true;
    errorString_.clear();

    if (enabled) {
        if (!QDir{}.mkpath(logsRoot_)) {
            errorString_ = u"Не удалось создать каталог логов."_s;
            return false;
        }
        const auto timestamp = QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s);
        const auto fileName = u"Tweakopedia-"_s + timestamp + u"-"_s
            + QString::number(QCoreApplication::applicationPid()) + u".log"_s;
        currentFilePath_ = QDir(logsRoot_).filePath(fileName);
        file_.setFileName(currentFilePath_);
        if (!file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            errorString_ = file_.errorString();
            currentFilePath_.clear();
            return false;
        }

        QMutexLocker lock(&handlerMutex_);
        if (activeLogger_ && activeLogger_ != this) {
            errorString_ = u"Другой файловый логгер уже активен."_s;
            file_.close();
            currentFilePath_.clear();
            return false;
        }
        previousHandler_ = qInstallMessageHandler(&SessionLogger::messageHandler);
        activeLogger_ = this;
        enabled_ = true;
        writeMessage(QtInfoMsg, {}, u"Debug logging session started"_s);
        return true;
    }

    QMutexLocker lock(&handlerMutex_);
    writeMessage(QtInfoMsg, {}, u"Debug logging session stopped"_s);
    if (activeLogger_ == this) {
        qInstallMessageHandler(previousHandler_);
        activeLogger_ = nullptr;
    }
    previousHandler_ = nullptr;
    enabled_ = false;
    file_.flush();
    file_.close();
    return true;
}

bool SessionLogger::enabled() const noexcept
{
    return enabled_;
}

const QString& SessionLogger::currentFilePath() const noexcept
{
    return currentFilePath_;
}

const QString& SessionLogger::errorString() const noexcept
{
    return errorString_;
}

void SessionLogger::messageHandler(
    QtMsgType type,
    const QMessageLogContext& context,
    const QString& message)
{
    QtMessageHandler previous{};
    {
        QMutexLocker lock(&handlerMutex_);
        if (activeLogger_) {
            activeLogger_->writeMessage(type, context, message);
            previous = activeLogger_->previousHandler_;
        }
    }
    if (previous && previous != &SessionLogger::messageHandler) {
        previous(type, context, message);
    }
}

void SessionLogger::writeMessage(
    QtMsgType type,
    const QMessageLogContext& context,
    const QString& message)
{
    if (!file_.isOpen()) return;
    QTextStream stream(&file_);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << " [" << levelName(type) << ']';
    const auto details = contextText(context);
    if (!details.isEmpty()) stream << " [" << details << ']';
    stream << ' ' << message << '\n';
    stream.flush();
    file_.flush();
}

} // namespace tweakopedia::app
