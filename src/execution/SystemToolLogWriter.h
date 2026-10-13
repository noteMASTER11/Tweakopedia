#pragma once

#include "execution/SystemToolProtocol.h"

#include <QFile>
#include <QStringList>

namespace tweakopedia::execution {

class SystemToolLogWriter final
{
public:
    [[nodiscard]] bool open(
        const QString& logsRoot,
        SystemToolKind tool,
        SystemToolOperation operation,
        const QStringList& metadata);
    void appendCommand(const QString& program, const QStringList& arguments);
    void appendOutput(const QByteArray& bytes);
    void close();

    [[nodiscard]] const QString& path() const noexcept;
    [[nodiscard]] const QString& errorString() const noexcept;

private:
    QFile file_;
    QString path_;
    QString error_;
};

} // namespace tweakopedia::execution
