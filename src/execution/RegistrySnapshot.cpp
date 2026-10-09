#include "execution/RegistrySnapshot.h"

#include <QCryptographicHash>

#include <limits>

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
        {u"snapshotType"_s, u"registry.value"_s},
        {u"type"_s, static_cast<int>(type)},
        {u"nativeType"_s, static_cast<qint64>(nativeType)},
        {u"rawBase64"_s, QString::fromLatin1(rawValue.toBase64())},
        {u"fingerprint"_s, QString::fromLatin1(fingerprint())},
    };
}

std::optional<RegistrySnapshot> RegistrySnapshot::fromJson(const QJsonObject& object)
{
    const auto hive = object.value(u"hive"_s).toInt(-1);
    const auto view = object.value(u"view"_s).toInt(-1);
    const auto presence = object.value(u"presence"_s).toInt(-1);
    const auto type = object.value(u"type"_s).toInt(-1);
    const auto nativeType = object.value(u"nativeType"_s).toInteger(-1);
    const auto key = object.value(u"key"_s).toString();
    const auto valueName = object.value(u"valueName"_s).toString();
    if (hive < 0 || hive > static_cast<int>(domain::RegistryHive::Users)
        || view < 0 || view > static_cast<int>(domain::RegistryView::Registry64)
        || presence < 0 || presence > static_cast<int>(platform::RegistryPresence::Error)
        || type < 0 || type > static_cast<int>(platform::RegistryValueType::Unknown)
        || nativeType < 0 || nativeType > std::numeric_limits<quint32>::max()
        || key.isEmpty() || valueName.isEmpty()) {
        return std::nullopt;
    }
    const auto raw = QByteArray::fromBase64(object.value(u"rawBase64"_s).toString().toLatin1());
    RegistrySnapshot snapshot{
        .location = {
            .hive = static_cast<domain::RegistryHive>(hive),
            .key = key,
            .valueName = valueName,
            .view = static_cast<domain::RegistryView>(view),
        },
        .presence = static_cast<platform::RegistryPresence>(presence),
        .type = static_cast<platform::RegistryValueType>(type),
        .nativeType = static_cast<quint32>(nativeType),
        .rawValue = raw,
    };
    if (object.value(u"fingerprint"_s).toString().toLatin1() != snapshot.fingerprint()) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace tweakopedia::execution
