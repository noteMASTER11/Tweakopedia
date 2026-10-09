#include "fakes/FakeRegistryBackend.h"

#include <QtEndian>

namespace tweakopedia::tests {

void FakeRegistryBackend::setReadResult(
    const domain::RegistryLocation& location,
    platform::RegistryReadResult result)
{
    values_.insert(keyFor(location), std::move(result));
}

platform::RegistryReadResult FakeRegistryBackend::read(const domain::RegistryLocation& location) const
{
    return values_.value(keyFor(location), platform::RegistryReadResult::missing());
}

platform::RegistryWriteResult FakeRegistryBackend::writeDword(
    const domain::RegistryLocation& location,
    quint32 value)
{
    return writeValue(location, domain::RegistryValueSpec::dword(value));
}

platform::RegistryWriteResult FakeRegistryBackend::writeValue(
    const domain::RegistryLocation& location,
    const domain::RegistryValueSpec& value)
{
    auto type = platform::RegistryValueType::Unknown;
    if (value.nativeType == 4) type = platform::RegistryValueType::Dword;
    else if (value.nativeType == 11) type = platform::RegistryValueType::Qword;
    else if (value.nativeType == 1) type = platform::RegistryValueType::String;
    else if (value.nativeType == 2) type = platform::RegistryValueType::ExpandString;
    else if (value.nativeType == 7) type = platform::RegistryValueType::MultiString;
    else if (value.nativeType == 3) type = platform::RegistryValueType::Binary;
    values_.insert(keyFor(location), platform::RegistryReadResult::present(
        type,
        value.rawValue,
        value.nativeType));
    return platform::RegistryWriteResult::succeeded();
}

platform::RegistryWriteResult FakeRegistryBackend::writeRaw(
    const domain::RegistryLocation& location,
    quint32 nativeType,
    const QByteArray& rawValue)
{
    return writeValue(location, domain::RegistryValueSpec{
        .nativeType = nativeType,
        .rawValue = rawValue,
    });
}

platform::RegistryWriteResult FakeRegistryBackend::deleteValue(const domain::RegistryLocation& location)
{
    values_.insert(keyFor(location), platform::RegistryReadResult::missing());
    return platform::RegistryWriteResult::succeeded();
}

QString FakeRegistryBackend::keyFor(const domain::RegistryLocation& location)
{
    return QString::number(static_cast<int>(location.hive))
        + u'|'
        + QString::number(static_cast<int>(location.view))
        + u'|'
        + location.key
        + u'|'
        + location.valueName;
}

} // namespace tweakopedia::tests
