#include "persistence/PendingInputStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace tweakopedia::persistence {
namespace {

QString normalizedExtension(QString extension)
{
    extension = extension.trimmed().toLower();
    while (extension.startsWith(u'.')) extension.remove(0, 1);
    return extension;
}

std::optional<QByteArray> fileHash(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return std::nullopt;
    return hash.result().toHex();
}

PendingInputResult failure(QString code, QString message)
{
    return {.code = std::move(code), .message = std::move(message)};
}

} // namespace

PendingInputStore::PendingInputStore(
    QString root,
    std::function<void()> beforeSourceRecheck)
    : root_(QDir::cleanPath(std::move(root)))
    , beforeSourceRecheck_(std::move(beforeSourceRecheck))
{
}

PendingInputResult PendingInputStore::importFile(
    QString inputId,
    const QString& sourcePath,
    const QStringList& allowedExtensions,
    quint64 maximumSize) const
{
    inputId = inputId.trimmed();
    const QFileInfo sourceInfo(sourcePath);
    const auto canonicalSource = sourceInfo.canonicalFilePath();
    if (inputId.isEmpty() || canonicalSource.isEmpty() || !sourceInfo.isFile()
        || !sourceInfo.isReadable()) {
        return failure(u"input.file_unreadable"_s, u"Выбранный файл недоступен для чтения."_s);
    }
    QStringList normalizedExtensions;
    for (const auto& extension : allowedExtensions) {
        normalizedExtensions.append(normalizedExtension(extension));
    }
    if (!normalizedExtensions.isEmpty()
        && !normalizedExtensions.contains(
            normalizedExtension(sourceInfo.suffix()), Qt::CaseInsensitive)) {
        return failure(u"input.file_extension_invalid"_s,
                       u"Расширение выбранного файла не поддерживается."_s);
    }
    if (sourceInfo.size() < 0 || (maximumSize > 0
        && static_cast<quint64>(sourceInfo.size()) > maximumSize)) {
        return failure(u"input.file_too_large"_s, u"Выбранный файл превышает допустимый размер."_s);
    }

    const auto storageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto artifactDirectory = QDir(root_).filePath(storageId);
    if (!QDir{}.mkpath(artifactDirectory)) {
        return failure(u"input.store_create_failed"_s, u"Не удалось создать каталог входного файла."_s);
    }
    const auto destination = QDir(artifactDirectory).filePath(sourceInfo.fileName());
    QFile source(canonicalSource);
    QSaveFile output(destination);
    if (!source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        QDir(artifactDirectory).removeRecursively();
        return failure(u"input.file_copy_failed"_s, u"Не удалось открыть файл для копирования."_s);
    }
    QCryptographicHash copiedHash(QCryptographicHash::Sha256);
    quint64 copiedSize{};
    while (!source.atEnd()) {
        const auto chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && source.error() != QFile::NoError) {
            output.cancelWriting();
            QDir(artifactDirectory).removeRecursively();
            return failure(u"input.file_copy_failed"_s, u"Ошибка чтения выбранного файла."_s);
        }
        copiedSize += static_cast<quint64>(chunk.size());
        if ((maximumSize > 0 && copiedSize > maximumSize)
            || output.write(chunk) != chunk.size()) {
            output.cancelWriting();
            QDir(artifactDirectory).removeRecursively();
            return failure(maximumSize > 0 && copiedSize > maximumSize
                               ? u"input.file_too_large"_s : u"input.file_copy_failed"_s,
                           u"Не удалось скопировать выбранный файл."_s);
        }
        copiedHash.addData(chunk);
    }
    source.close();
    if (!output.commit()) {
        QDir(artifactDirectory).removeRecursively();
        return failure(u"input.file_copy_failed"_s, u"Не удалось завершить копирование файла."_s);
    }

    if (beforeSourceRecheck_) beforeSourceRecheck_();
    const auto sourceHashAfter = fileHash(canonicalSource);
    const QFileInfo sourceAfter(canonicalSource);
    if (!sourceHashAfter || sourceAfter.size() < 0
        || static_cast<quint64>(sourceAfter.size()) != copiedSize
        || *sourceHashAfter != copiedHash.result().toHex()) {
        QDir(artifactDirectory).removeRecursively();
        return failure(u"input.source_changed"_s,
                       u"Исходный файл изменился во время копирования."_s);
    }

    return {.artifact = domain::InputArtifact{
        .id = std::move(inputId),
        .storageId = storageId,
        .managedPath = QDir::cleanPath(destination),
        .size = copiedSize,
        .sha256 = copiedHash.result().toHex(),
    }};
}

bool PendingInputStore::cleanupUnused(const QSet<QString>& liveStorageIds) const
{
    QDir root(root_);
    if (!root.exists()) return true;
    const auto directories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& directory : directories) {
        if (liveStorageIds.contains(directory.fileName())) continue;
        if (!QDir(directory.absoluteFilePath()).removeRecursively()) return false;
    }
    return true;
}

const QString& PendingInputStore::root() const noexcept
{
    return root_;
}

} // namespace tweakopedia::persistence
