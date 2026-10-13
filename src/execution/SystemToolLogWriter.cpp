#include "execution/SystemToolLogWriter.h"

#include <QDateTime>
#include <QDir>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

bool SystemToolLogWriter::open(
    const QString& logsRoot,
    SystemToolKind tool,
    SystemToolOperation operation,
    const QStringList& metadata)
{
    close();
    error_.clear();
    const auto toolDirectory = tool == SystemToolKind::ComponentStore
        ? u"SystemIntegrity"_s : u"FileSystemCheck"_s;
    const auto directory = QDir(logsRoot).filePath(u"Tools/"_s + toolDirectory);
    if (!QDir{}.mkpath(directory)) {
        error_ = u"Не удалось создать каталог журнала инструмента."_s;
        return false;
    }
    const auto timestamp = QDateTime::currentDateTime().toString(u"yyyy-MM-dd_HH-mm-ss-zzz"_s);
    path_ = QDir(directory).filePath(
        timestamp + u"_"_s + SystemToolProtocol::operationName(operation) + u".log"_s);
    file_.setFileName(path_);
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Append)) {
        error_ = file_.errorString();
        path_.clear();
        return false;
    }
    QTextStream stream(&file_);
    stream << "Tweakopedia System Tool\n"
           << "Tool: " << SystemToolProtocol::toolName(tool) << '\n'
           << "Operation: " << SystemToolProtocol::operationName(operation) << '\n'
           << "Started: " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << '\n';
    for (const auto& line : metadata) stream << line << '\n';
    stream << '\n';
    stream.flush();
    file_.flush();
    return true;
}

void SystemToolLogWriter::appendCommand(const QString& program, const QStringList& arguments)
{
    if (!file_.isOpen()) return;
    QTextStream stream(&file_);
    stream << "\n=== " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << " ===\n> "
           << program << ' ' << arguments.join(u' ') << "\n";
    stream.flush();
    file_.flush();
}

void SystemToolLogWriter::appendOutput(const QByteArray& bytes)
{
    if (!file_.isOpen() || bytes.isEmpty()) return;
    (void)file_.write(bytes);
    file_.flush();
}

void SystemToolLogWriter::close()
{
    if (!file_.isOpen()) return;
    file_.flush();
    file_.close();
}

const QString& SystemToolLogWriter::path() const noexcept
{
    return path_;
}

const QString& SystemToolLogWriter::errorString() const noexcept
{
    return error_;
}

} // namespace tweakopedia::execution
