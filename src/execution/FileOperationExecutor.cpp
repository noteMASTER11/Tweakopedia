#include "execution/FileOperationExecutor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

FileOperationResult failure(QString code, QString message)
{
    return {.success = false, .code = std::move(code), .message = std::move(message)};
}

std::optional<QByteArray> hashFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return std::nullopt;
    return hash.result().toHex();
}

bool copyAtomically(const QString& sourcePath, const QString& destinationPath)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) return false;
    if (!QDir{}.mkpath(QFileInfo(destinationPath).absolutePath())) return false;
    QSaveFile destination(destinationPath);
    if (!destination.open(QIODevice::WriteOnly)) return false;
    while (!source.atEnd()) {
        const auto chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && source.error() != QFile::NoError) {
            destination.cancelWriting();
            return false;
        }
        if (destination.write(chunk) != chunk.size()) {
            destination.cancelWriting();
            return false;
        }
    }
    return destination.commit();
}

bool validDestination(const QString& path)
{
    return QDir::isAbsolutePath(path) && QDir::cleanPath(path) == path;
}

std::optional<QString> resolvedArtifact(
    const QString& transactionDirectory,
    const domain::InputArtifact& artifact)
{
    const auto relative = QDir::cleanPath(artifact.managedPath);
    if (relative.isEmpty() || relative == u"."_s || QDir::isAbsolutePath(relative)
        || relative == u".."_s || relative.startsWith(u"../"_s)
        || relative.startsWith(u"..\\"_s)
        || !(relative.startsWith(u"inputs/"_s) || relative.startsWith(u"inputs\\"_s))) {
        return std::nullopt;
    }
    const auto root = QDir::cleanPath(QFileInfo(transactionDirectory).absoluteFilePath());
    const auto candidate = QDir::cleanPath(QDir(root).filePath(relative));
    const auto prefix = root.endsWith(u'/') ? root : root + u'/';
    if (!candidate.startsWith(prefix, Qt::CaseInsensitive)) return std::nullopt;
    return candidate;
}

} // namespace

FileOperationExecutor::FileOperationExecutor(QString transactionDirectory)
    : transactionDirectory_(QDir::cleanPath(std::move(transactionDirectory)))
{
}

FileCaptureResult FileOperationExecutor::capture(const domain::FileOperation& operation) const
{
    FileCaptureResult result;
    if (!validDestination(operation.destination)) {
        result.code = u"file.destination_invalid"_s;
        result.message = u"Путь назначения файла недопустим."_s;
        return result;
    }
    result.snapshot.destination = operation.destination;
    const QFileInfo destination(operation.destination);
    if (!destination.exists()) {
        result.success = true;
        return result;
    }
    if (!destination.isFile() || !destination.isReadable()) {
        result.code = u"file.destination_unreadable"_s;
        result.message = u"Исходный файл назначения недоступен для чтения."_s;
        return result;
    }
    const auto digest = hashFile(operation.destination);
    if (!digest || destination.size() < 0) {
        result.code = u"file.snapshot_failed"_s;
        result.message = u"Не удалось получить снимок файла назначения."_s;
        return result;
    }
    const auto backupName = QString::fromLatin1(QCryptographicHash::hash(
        operation.destination.toUtf8(), QCryptographicHash::Sha256).toHex()) + u".bin"_s;
    const auto backupRelative = u"backups/"_s + backupName;
    const auto backupAbsolute = QDir(transactionDirectory_).filePath(backupRelative);
    if (!copyAtomically(operation.destination, backupAbsolute)) {
        result.code = u"file.snapshot_failed"_s;
        result.message = u"Не удалось сохранить резервную копию файла."_s;
        return result;
    }
    result.snapshot.existed = true;
    result.snapshot.backupRelativePath = backupRelative;
    result.snapshot.size = static_cast<quint64>(destination.size());
    result.snapshot.sha256 = *digest;
    result.success = true;
    return result;
}

QByteArray FileOperationExecutor::fingerprint(const domain::FileSnapshot& snapshot)
{
    const QJsonObject state{
        {u"destination"_s, snapshot.destination},
        {u"existed"_s, snapshot.existed},
        {u"size"_s, QString::number(snapshot.size)},
        {u"sha256"_s, QString::fromLatin1(snapshot.sha256)},
    };
    return QCryptographicHash::hash(
        QJsonDocument(state).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

FileOperationResult FileOperationExecutor::compareBefore(
    const domain::FileSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (expectedFingerprint.isEmpty() || fingerprint(snapshot) != expectedFingerprint) {
        return failure(u"file.before_changed"_s,
                       u"Файл назначения изменился после построения плана."_s);
    }
    return {.success = true};
}

FileOperationResult FileOperationExecutor::apply(const domain::FileOperation& operation) const
{
    if (!validDestination(operation.destination)) {
        return failure(u"file.destination_invalid"_s, u"Путь назначения файла недопустим."_s);
    }
    if (operation.kind == domain::FileOperationKind::Delete) {
        if (!QFileInfo::exists(operation.destination)) return {.success = true};
        if (!QFile::remove(operation.destination)) {
            return failure(u"file.delete_failed"_s, u"Не удалось удалить файл назначения."_s);
        }
        return {.success = !QFileInfo::exists(operation.destination)};
    }
    const auto source = resolvedArtifact(transactionDirectory_, operation.artifact);
    if (!source) {
        return failure(u"file.artifact_path_invalid"_s,
                       u"Ссылка на входной файл выходит за каталог транзакции."_s);
    }
    const QFileInfo sourceInfo(*source);
    const auto digest = hashFile(*source);
    if (!digest || sourceInfo.size() < 0
        || static_cast<quint64>(sourceInfo.size()) != operation.artifact.size
        || *digest != operation.artifact.sha256) {
        return failure(u"file.artifact_hash_mismatch"_s,
                       u"Входной файл транзакции не совпадает с планом."_s);
    }
    if (operation.kind == domain::FileOperationKind::Copy
        && QFileInfo::exists(operation.destination)) {
        return failure(u"file.destination_exists"_s,
                       u"Операция копирования не заменяет существующий файл."_s);
    }
    if (!copyAtomically(*source, operation.destination)) {
        return failure(u"file.copy_failed"_s, u"Не удалось записать файл назначения."_s);
    }
    const auto destinationHash = hashFile(operation.destination);
    if (!destinationHash || *destinationHash != operation.artifact.sha256) {
        return failure(u"file.verify_failed"_s, u"Проверка записанного файла не пройдена."_s);
    }
    return {.success = true};
}

FileOperationResult FileOperationExecutor::restore(const domain::FileSnapshot& snapshot) const
{
    if (!validDestination(snapshot.destination)) {
        return failure(u"file.destination_invalid"_s, u"Путь назначения файла недопустим."_s);
    }
    if (!snapshot.existed) {
        if (QFileInfo::exists(snapshot.destination) && !QFile::remove(snapshot.destination)) {
            return failure(u"file.restore_failed"_s, u"Не удалось удалить созданный файл."_s);
        }
        return {.success = !QFileInfo::exists(snapshot.destination)};
    }
    const domain::InputArtifact backup{
        .managedPath = snapshot.backupRelativePath,
        .size = snapshot.size,
        .sha256 = snapshot.sha256,
    };
    auto source = resolvedArtifact(transactionDirectory_, backup);
    if (!source && snapshot.backupRelativePath.startsWith(u"backups/"_s)) {
        const auto root = QDir::cleanPath(QFileInfo(transactionDirectory_).absoluteFilePath());
        const auto candidate = QDir::cleanPath(QDir(root).filePath(snapshot.backupRelativePath));
        const auto prefix = root.endsWith(u'/') ? root : root + u'/';
        if (candidate.startsWith(prefix, Qt::CaseInsensitive)) source = candidate;
    }
    const auto digest = source ? hashFile(*source) : std::nullopt;
    if (!source || !digest || *digest != snapshot.sha256) {
        return failure(u"file.backup_invalid"_s, u"Резервная копия файла повреждена."_s);
    }
    if (!copyAtomically(*source, snapshot.destination)) {
        return failure(u"file.restore_failed"_s, u"Не удалось восстановить файл."_s);
    }
    return {.success = hashFile(snapshot.destination) == std::optional<QByteArray>{snapshot.sha256}};
}

} // namespace tweakopedia::execution
