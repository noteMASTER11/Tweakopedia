#pragma once

#include "domain/TweakInput.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace tweakopedia::domain {

enum class FileOperationKind {
    Copy,
    Replace,
    Delete,
};

struct FileOperationDefinition {
    FileOperationKind kind{FileOperationKind::Copy};
    QString inputId;
    QString destination;

    friend bool operator==(const FileOperationDefinition&, const FileOperationDefinition&) = default;
};

struct FileOperation {
    FileOperationKind kind{FileOperationKind::Copy};
    InputArtifact artifact;
    QString destination;

    friend bool operator==(const FileOperation&, const FileOperation&) = default;
};

struct FileSnapshot {
    QString destination;
    bool existed{};
    QString backupRelativePath;
    quint64 size{};
    QByteArray sha256;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<FileSnapshot> fromJson(const QJsonObject& object);
};

} // namespace tweakopedia::domain
