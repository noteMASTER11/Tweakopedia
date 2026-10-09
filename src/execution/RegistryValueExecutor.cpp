#include "execution/RegistryValueExecutor.h"

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

RegistryValueExecutor::RegistryValueExecutor(platform::IRegistryBackend& backend)
    : backend_(&backend)
{
}

RegistryCaptureResult RegistryValueExecutor::capture(
    const domain::RegistryLocation& location) const
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
    return {{.success = true}, RegistrySnapshot::fromRead(location, value)};
}

RegistryExecutionResult RegistryValueExecutor::compareBefore(
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

RegistryExecutionResult RegistryValueExecutor::apply(
    const domain::SetRegistryValueOperation& operation)
{
    const auto written = backend_->writeValue(operation.location, operation.value);
    if (!written.success) {
        return failure(
            u"registry.write_failed"_s,
            u"Не удалось записать значение реестра: "_s + errorText(written.error));
    }
    const auto verified = backend_->read(operation.location);
    if (verified.presence != platform::RegistryPresence::Present
        || verified.nativeType != operation.value.nativeType
        || verified.rawValue != operation.value.rawValue) {
        return failure(
            u"registry.verify_failed"_s,
            u"Контрольное чтение не совпало с целевым значением."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryValueExecutor::apply(
    const domain::DeleteRegistryValueOperation& operation)
{
    const auto removed = backend_->deleteValue(operation.location);
    if (!removed.success) {
        return failure(
            u"registry.delete_failed"_s,
            u"Не удалось удалить значение реестра: "_s + errorText(removed.error));
    }
    if (backend_->read(operation.location).presence != platform::RegistryPresence::Missing) {
        return failure(
            u"registry.verify_failed"_s,
            u"Значение реестра осталось после удаления."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryValueExecutor::restore(const RegistrySnapshot& snapshot)
{
    const auto restored = snapshot.presence == platform::RegistryPresence::Missing
        ? backend_->deleteValue(snapshot.location)
        : backend_->writeRaw(snapshot.location, snapshot.nativeType, snapshot.rawValue);
    if (!restored.success) {
        return failure(
            u"registry.restore_failed"_s,
            u"Не удалось восстановить исходное значение: "_s + errorText(restored.error));
    }
    if (RegistrySnapshot::fingerprint(backend_->read(snapshot.location)) != snapshot.fingerprint()) {
        return failure(
            u"registry.restore_verify_failed"_s,
            u"Контрольное чтение после возврата не совпало со снимком."_s);
    }
    return {.success = true};
}

} // namespace tweakopedia::execution
