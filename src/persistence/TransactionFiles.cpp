#include "persistence/TransactionFiles.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tweakopedia::persistence {

TransactionFiles::TransactionFiles(QString transactionsRoot)
    : transactionsRoot_(QDir::cleanPath(std::move(transactionsRoot)))
{
}

bool TransactionFiles::create(const QUuid& id) const
{
    if (id.isNull() || !QDir{}.mkpath(directory(id))) {
        return false;
    }
    QFile log(QDir(directory(id)).filePath(u"operations.log"_s));
    return log.open(QIODevice::WriteOnly | QIODevice::Append);
}

QString TransactionFiles::directory(const QUuid& id) const
{
    return QDir(transactionsRoot_).filePath(id.toString(QUuid::WithoutBraces));
}

bool TransactionFiles::writePlan(const QUuid& id, const QJsonObject& json) const
{
    return write(id, u"plan.json", json);
}

bool TransactionFiles::writeBefore(const QUuid& id, const QJsonObject& json) const
{
    return write(id, u"before.json", json);
}

bool TransactionFiles::writeResult(const QUuid& id, const QJsonObject& json) const
{
    return write(id, u"result.json", json);
}

std::optional<QJsonObject> TransactionFiles::readPlan(const QUuid& id) const
{
    return read(id, u"plan.json");
}

std::optional<QJsonObject> TransactionFiles::readBefore(const QUuid& id) const
{
    return read(id, u"before.json");
}

std::optional<QJsonObject> TransactionFiles::readResult(const QUuid& id) const
{
    return read(id, u"result.json");
}

ArtifactMaterializeResult TransactionFiles::materializeInput(
    const QUuid& id,
    const domain::InputArtifact& artifact) const
{
    static const QRegularExpression storagePattern(u"^[A-Za-z0-9-]{1,64}$"_s);
    const QFileInfo sourceInfo(artifact.managedPath);
    if (id.isNull() || !QDir(directory(id)).exists() || !sourceInfo.isAbsolute()
        || !sourceInfo.isFile() || !sourceInfo.isReadable()
        || !storagePattern.match(artifact.storageId).hasMatch()) {
        return {.code = u"input.artifact_invalid"_s};
    }
    QFile source(artifact.managedPath);
    if (!source.open(QIODevice::ReadOnly)) return {.code = u"input.artifact_unreadable"_s};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    quint64 size{};
    const auto relative = u"inputs/"_s + artifact.storageId + u"/"_s + sourceInfo.fileName();
    const auto destination = QDir(directory(id)).filePath(relative);
    if (!QDir{}.mkpath(QFileInfo(destination).absolutePath())) {
        return {.code = u"input.materialize_failed"_s};
    }
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly)) return {.code = u"input.materialize_failed"_s};
    while (!source.atEnd()) {
        const auto chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && source.error() != QFile::NoError) {
            output.cancelWriting();
            return {.code = u"input.artifact_unreadable"_s};
        }
        size += static_cast<quint64>(chunk.size());
        hash.addData(chunk);
        if (output.write(chunk) != chunk.size()) {
            output.cancelWriting();
            return {.code = u"input.materialize_failed"_s};
        }
    }
    const auto digest = hash.result().toHex();
    if (size != artifact.size || digest != artifact.sha256) {
        output.cancelWriting();
        return {.code = u"input.artifact_changed"_s};
    }
    if (!output.commit()) return {.code = u"input.materialize_failed"_s};
    return {.artifact = domain::InputArtifact{
        .id = artifact.id,
        .storageId = artifact.storageId,
        .managedPath = relative,
        .size = size,
        .sha256 = digest,
    }};
}

bool TransactionFiles::write(const QUuid& id, QStringView fileName, const QJsonObject& json) const
{
    if (!QDir(directory(id)).exists()) {
        return false;
    }
    QSaveFile file(QDir(directory(id)).filePath(fileName.toString()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const auto bytes = QJsonDocument(json).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

std::optional<QJsonObject> TransactionFiles::read(const QUuid& id, QStringView fileName) const
{
    QFile file(QDir(directory(id)).filePath(fileName.toString()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

} // namespace tweakopedia::persistence
