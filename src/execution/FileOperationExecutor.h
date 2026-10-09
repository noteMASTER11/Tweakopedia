#pragma once

#include "domain/FileOperation.h"

#include <QByteArray>
#include <QString>

namespace tweakopedia::execution {

struct FileOperationResult {
    bool success{};
    QString code;
    QString message;
};

struct FileCaptureResult : FileOperationResult {
    domain::FileSnapshot snapshot;
};

class FileOperationExecutor final
{
public:
    explicit FileOperationExecutor(QString transactionDirectory);

    [[nodiscard]] FileCaptureResult capture(const domain::FileOperation& operation) const;
    [[nodiscard]] FileOperationResult compareBefore(
        const domain::FileSnapshot& snapshot, const QByteArray& expectedFingerprint) const;
    [[nodiscard]] FileOperationResult apply(const domain::FileOperation& operation) const;
    [[nodiscard]] FileOperationResult restore(const domain::FileSnapshot& snapshot) const;

    [[nodiscard]] static QByteArray fingerprint(const domain::FileSnapshot& snapshot);

private:
    QString transactionDirectory_;
};

} // namespace tweakopedia::execution
