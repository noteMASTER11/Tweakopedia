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
    QByteArray bytes(sizeof(value), Qt::Uninitialized);
    qToLittleEndian(value, bytes.data());
    values_.insert(keyFor(location), platform::RegistryReadResult::present(
        platform::RegistryValueType::Dword,
        bytes));
    return platform::RegistryWriteResult::succeeded();
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
