#include "execution/RegistryDwordExecutor.h"

#include <QtEndian>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

RegistryExecutionResult failure(QString code, QString message)
{
    return {.success = false, .code = std::move(code), .message = std::move(message)};
}

QString errorText(const std::error_code& error)
{
    return QString::fromLocal8Bit(error.message());
}

} // namespace

RegistryDwordExecutor::RegistryDwordExecutor(platform::IRegistryBackend& backend)
    : backend_(&backend)
{
}

RegistryCaptureResult RegistryDwordExecutor::capture(const domain::RegistryLocation& location) const
{
    const auto value = backend_->read(location);
    if (value.presence == platform::RegistryPresence::Error) {
        return {
            {.success = false,
             .code = u"registry.read_failed"_s,
             .message = u"Не удалось прочитать исходное значение: "_s + errorText(value.error)},
            {},
        };
    }
    return {
        {.success = true},
        RegistrySnapshot::fromRead(location, value),
    };
}

RegistryExecutionResult RegistryDwordExecutor::compareBefore(
    const RegistrySnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (snapshot.fingerprint() != expectedFingerprint) {
        return failure(
            u"state.changed"_s,
            u"Исходное значение изменилось после построения предварительного плана."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryDwordExecutor::apply(
    const domain::SetRegistryDwordOperation& operation)
{
    const auto written = backend_->writeDword(operation.location, operation.value);
    if (!written.success) {
        return failure(
            u"registry.write_failed"_s,
            u"Не удалось записать DWORD: "_s + errorText(written.error));
    }

    const auto verified = backend_->read(operation.location);
    QByteArray expected(sizeof(operation.value), Qt::Uninitialized);
    qToLittleEndian(operation.value, expected.data());
    if (verified.presence != platform::RegistryPresence::Present
        || verified.type != platform::RegistryValueType::Dword
        || verified.rawValue != expected) {
        return failure(
            u"registry.verify_failed"_s,
            u"Контрольное чтение DWORD не совпало с целевым значением."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryDwordExecutor::restore(const RegistrySnapshot& snapshot)
{
    const auto restored = snapshot.presence == platform::RegistryPresence::Missing
        ? backend_->deleteValue(snapshot.location)
        : backend_->writeRaw(snapshot.location, snapshot.nativeType, snapshot.rawValue);
    if (!restored.success) {
        return failure(
            u"registry.restore_failed"_s,
            u"Не удалось восстановить исходное значение: "_s + errorText(restored.error));
    }

    const auto verified = backend_->read(snapshot.location);
    if (RegistrySnapshot::fingerprint(verified) != snapshot.fingerprint()) {
        return failure(
            u"registry.restore_verify_failed"_s,
            u"Контрольное чтение после возврата не совпало со снимком."_s);
    }
    return {.success = true};
}

} // namespace tweakopedia::execution
