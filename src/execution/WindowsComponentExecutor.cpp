#include "execution/WindowsComponentExecutor.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

QByteArray WindowsComponentExecutor::fingerprint(
    const domain::WindowsComponentSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

WindowsComponentCaptureResult WindowsComponentExecutor::capture(
    const domain::WindowsComponentTarget& target) const
{
    const auto result = backend_->query(target);
    if (result.isUnsupported) {
        WindowsComponentCaptureResult captured;
        captured.code = u"windows_component.unsupported"_s;
        captured.message = result.error;
        return captured;
    }
    if (!result.success) {
        WindowsComponentCaptureResult captured;
        captured.code = u"windows_component.read_failed"_s;
        captured.message = result.error;
        return captured;
    }
    WindowsComponentCaptureResult captured;
    captured.success = true;
    captured.snapshot = {target, result.state};
    return captured;
}

WindowsComponentExecutionResult WindowsComponentExecutor::compareBefore(
    const domain::WindowsComponentSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    return !expectedFingerprint.isEmpty() && fingerprint(snapshot) == expectedFingerprint
        ? WindowsComponentExecutionResult{.success = true}
        : WindowsComponentExecutionResult{
              .code = u"windows_component.before_changed"_s,
              .message = u"Состояние компонента изменилось после построения плана."_s};
}

WindowsComponentExecutionResult WindowsComponentExecutor::apply(
    const domain::SetWindowsComponentStateOperation& operation) const
{
    const auto mutation = backend_->setState(operation.target, operation.state);
    if (!mutation.success) {
        return {.code = u"windows_component.write_failed"_s, .message = mutation.error};
    }
    const auto verified = backend_->query(operation.target);
    if (!verified.success || verified.state != operation.state) {
        return {.code = u"windows_component.verify_failed"_s,
                .message = verified.error.isEmpty()
                    ? u"Проверка состояния компонента не пройдена."_s : verified.error};
    }
    return {.success = true, .restartRequired = mutation.restartRequired};
}

WindowsComponentExecutionResult WindowsComponentExecutor::restore(
    const domain::WindowsComponentSnapshot& snapshot) const
{
    return apply({snapshot.target, snapshot.state});
}

} // namespace tweakopedia::execution
