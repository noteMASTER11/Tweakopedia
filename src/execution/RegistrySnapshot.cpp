#include "execution/RegistrySnapshot.h"

#include <QCryptographicHash>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

RegistrySnapshot RegistrySnapshot::fromRead(
    domain::RegistryLocation location,
    const platform::RegistryReadResult& value)
{
    return {
        .location = std::move(location),
        .presence = value.presence,
        .type = value.type,
        .nativeType = value.nativeType,
        .rawValue = value.rawValue,
    };
}

QByteArray RegistrySnapshot::fingerprint(const platform::RegistryReadResult& value)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(static_cast<int>(value.presence)));
    hash.addData("|");
    hash.addData(QByteArray::number(static_cast<int>(value.type)));
    hash.addData("|");
    hash.addData(QByteArray::number(value.nativeType));
    hash.addData("|");
    hash.addData(value.rawValue);
    return hash.result().toHex();
}

QByteArray RegistrySnapshot::fingerprint() const
{
    return fingerprint({
        .presence = presence,
        .type = type,
        .nativeType = nativeType,
        .rawValue = rawValue,
    });
}

QJsonObject RegistrySnapshot::toJson() const
{
    return {
        {u"hive"_s, static_cast<int>(location.hive)},
        {u"key"_s, location.key},
        {u"valueName"_s, location.valueName},
        {u"view"_s, static_cast<int>(location.view)},
        {u"presence"_s, static_cast<int>(presence)},
        {u"type"_s, static_cast<int>(type)},
        {u"nativeType"_s, static_cast<qint64>(nativeType)},
        {u"rawBase64"_s, QString::fromLatin1(rawValue.toBase64())},
        {u"fingerprint"_s, QString::fromLatin1(fingerprint())},
    };
}

} // namespace tweakopedia::execution
