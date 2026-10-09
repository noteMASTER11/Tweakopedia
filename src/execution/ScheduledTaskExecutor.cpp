#include "execution/ScheduledTaskExecutor.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

ScheduledTaskExecutor::ScheduledTaskExecutor(platform::IScheduledTaskBackend& backend)
    : backend_(&backend)
{
}

QByteArray ScheduledTaskExecutor::fingerprint(const domain::ScheduledTaskSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

ScheduledTaskCaptureResult ScheduledTaskExecutor::capture(
    const domain::ScheduledTaskLocation& location) const
{
    const auto read = backend_->read(location);
    ScheduledTaskCaptureResult result;
    if (read.missing) {
        result.code = u"scheduled_task.missing"_s;
        result.message = u"Задача планировщика отсутствует."_s;
        return result;
    }
    if (!read.success) {
        result.code = u"scheduled_task.read_failed"_s;
        result.message = read.error;
        return result;
    }
    result.success = true;
    result.snapshot = {.location = location, .enabled = read.enabled};
    return result;
}

ScheduledTaskExecutionResult ScheduledTaskExecutor::compareBefore(
    const domain::ScheduledTaskSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (expectedFingerprint.isEmpty() || fingerprint(snapshot) != expectedFingerprint) {
        return {.code = u"scheduled_task.before_changed"_s,
                .message = u"Состояние задачи изменилось после построения плана."_s};
    }
    return {.success = true};
}

ScheduledTaskExecutionResult ScheduledTaskExecutor::apply(
    const domain::SetScheduledTaskEnabledOperation& operation) const
{
    const auto written = backend_->setEnabled(operation.location, operation.enabled);
    if (!written.success) return {.code = u"scheduled_task.write_failed"_s,
                                  .message = written.error};
    const auto verified = backend_->read(operation.location);
    if (!verified.success || verified.missing || verified.enabled != operation.enabled) {
        return {.code = u"scheduled_task.verify_failed"_s,
                .message = verified.error.isEmpty()
                    ? u"Проверка состояния задачи не пройдена."_s : verified.error};
    }
    return {.success = true};
}

ScheduledTaskExecutionResult ScheduledTaskExecutor::restore(
    const domain::ScheduledTaskSnapshot& snapshot) const
{
    return apply({.location = snapshot.location, .enabled = snapshot.enabled});
}

} // namespace tweakopedia::execution
