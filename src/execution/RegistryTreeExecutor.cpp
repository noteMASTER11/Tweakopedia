#include "execution/RegistryTreeExecutor.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

RegistryExecutionResult failure(QString code, QString message)
{
    return {.success = false, .code = std::move(code), .message = std::move(message)};
}

QString errorText(const std::error_code& error)
{
    return error ? QString::fromLocal8Bit(error.message()) : QString{};
}

} // namespace

RegistryTreeExecutor::RegistryTreeExecutor(platform::IRegistryBackend& backend)
    : backend_(&backend)
{
}

QByteArray RegistryTreeExecutor::fingerprint(const domain::RegistryTreeSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

RegistryTreeCaptureResult RegistryTreeExecutor::capture(
    const domain::RegistryKeyLocation& location,
    const domain::RegistryTreeLimits& limits) const
{
    const auto result = backend_->readTree(location, limits);
    if (!result.success) {
        return {
            {.success = false,
             .code = result.code.isEmpty() ? u"registry.tree_read_failed"_s : result.code,
             .message = u"Не удалось прочитать ветвь реестра. "_s + errorText(result.error)},
            {},
        };
    }
    return {{.success = true}, result.snapshot};
}

RegistryExecutionResult RegistryTreeExecutor::compareBefore(
    const domain::RegistryTreeSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (fingerprint(snapshot) != expectedFingerprint) {
        return failure(
            u"state.changed"_s,
            u"Ветвь реестра изменилась после построения предварительного плана."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryTreeExecutor::apply(
    const domain::CreateRegistryKeyOperation& operation)
{
    const auto created = backend_->createKey(operation.location);
    if (!created.success) {
        return failure(u"registry.tree_create_failed"_s,
                       u"Не удалось создать ветвь реестра: "_s + errorText(created.error));
    }
    const auto verified = backend_->readTree(operation.location);
    if (!verified.success || !verified.snapshot.existed) {
        return failure(u"registry.tree_verify_failed"_s,
                       u"Созданная ветвь реестра не найдена при проверке."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryTreeExecutor::apply(
    const domain::DeleteRegistryTreeOperation& operation)
{
    const auto removed = backend_->deleteTree(operation.location);
    if (!removed.success) {
        return failure(u"registry.tree_delete_failed"_s,
                       u"Не удалось удалить ветвь реестра: "_s + errorText(removed.error));
    }
    const auto verified = backend_->readTree(operation.location);
    if (!verified.success || verified.snapshot.existed) {
        return failure(u"registry.tree_verify_failed"_s,
                       u"Ветвь реестра осталась после удаления."_s);
    }
    return {.success = true};
}

RegistryExecutionResult RegistryTreeExecutor::restore(
    const domain::RegistryTreeSnapshot& snapshot)
{
    const auto restored = backend_->restoreTree(snapshot);
    if (!restored.success) {
        return failure(u"registry.tree_restore_failed"_s,
                       u"Не удалось восстановить ветвь реестра: "_s + errorText(restored.error));
    }
    const auto verified = backend_->readTree(snapshot.location);
    if (!verified.success || fingerprint(verified.snapshot) != fingerprint(snapshot)) {
        return failure(u"registry.tree_restore_verify_failed"_s,
                       u"Восстановленная ветвь не совпала со снимком."_s);
    }
    return {.success = true};
}

} // namespace tweakopedia::execution
