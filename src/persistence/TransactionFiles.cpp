#include "persistence/TransactionFiles.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

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
