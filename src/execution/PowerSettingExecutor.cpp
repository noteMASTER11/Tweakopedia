#include "execution/PowerSettingExecutor.h"
#include <QCryptographicHash>
#include <QJsonDocument>
using namespace Qt::StringLiterals;
namespace tweakopedia::execution {
QByteArray PowerSettingExecutor::fingerprint(const domain::PowerSettingSnapshot& snapshot)
{ return QCryptographicHash::hash(QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
                                  QCryptographicHash::Sha256).toHex(); }
PowerSettingCaptureResult PowerSettingExecutor::capture(const domain::PowerSettingLocation& location) const
{
    const auto read = backend_->read(location); PowerSettingCaptureResult result;
    if (read.missing) { result.code = u"power.setting_missing"_s; result.message = u"Параметр питания отсутствует."_s; return result; }
    if (!read.success) { result.code = u"power.read_failed"_s; result.message = read.error; return result; }
    result.success = true; result.snapshot = {location, read.resolvedScheme, read.index}; return result;
}
PowerSettingExecutionResult PowerSettingExecutor::compareBefore(
    const domain::PowerSettingSnapshot& snapshot, const QByteArray& expected) const
{
    return !expected.isEmpty() && fingerprint(snapshot) == expected
        ? PowerSettingExecutionResult{.success = true}
        : PowerSettingExecutionResult{.code = u"power.before_changed"_s,
            .message = u"Параметр питания изменился после построения плана."_s};
}
PowerSettingExecutionResult PowerSettingExecutor::apply(const domain::SetPowerSettingOperation& operation) const
{
    const auto written = backend_->write(operation.location, operation.index);
    if (!written.success) return {.code = u"power.write_failed"_s, .message = written.error};
    const auto verified = backend_->read(operation.location);
    if (!verified.success || verified.missing || verified.index != operation.index)
        return {.code = u"power.verify_failed"_s, .message = verified.error.isEmpty()
            ? u"Проверка индекса питания не пройдена."_s : verified.error};
    return {.success = true};
}
PowerSettingExecutionResult PowerSettingExecutor::restore(const domain::PowerSettingSnapshot& snapshot) const
{
    auto location = snapshot.location; location.scheme = snapshot.resolvedScheme;
    return apply({location, snapshot.index});
}
}
