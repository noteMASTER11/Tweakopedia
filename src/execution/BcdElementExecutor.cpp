#include "execution/BcdElementExecutor.h"

#include <QCryptographicHash>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

BcdElementExecutor::BcdElementExecutor(platform::IBcdBackend& backend) : backend_(&backend) {}

QByteArray BcdElementExecutor::fingerprint(const domain::BcdElementSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

BcdCaptureResult BcdElementExecutor::capture(const domain::BcdElementSpec& spec) const
{
    const auto read = backend_->read(spec);
    BcdCaptureResult result;
    if (!read.success && !read.missing) {
        result.code = u"bcd.read_failed"_s;
        result.message = read.error;
        return result;
    }
    result.success = true;
    result.snapshot = {.spec = spec, .existed = !read.missing, .value = read.value};
    return result;
}

BcdExecutionResult BcdElementExecutor::compareBefore(
    const domain::BcdElementSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (expectedFingerprint.isEmpty() || fingerprint(snapshot) != expectedFingerprint) {
        return {.code = u"bcd.before_changed"_s,
                .message = u"BCD-элемент изменился после построения плана."_s};
    }
    return {.success = true};
}

BcdExecutionResult BcdElementExecutor::apply(
    const domain::SetBcdElementOperation& operation) const
{
    if (operation.value && !domain::bcdValueMatchesKind(*operation.value, operation.spec.valueKind)) {
        return {.code = u"bcd.value_kind_mismatch"_s,
                .message = u"Тип значения не соответствует BCD-элементу."_s};
    }
    const auto changed = operation.value
        ? backend_->set(operation.spec, *operation.value)
        : backend_->remove(operation.spec);
    if (!changed.success) return {.code = u"bcd.write_failed"_s, .message = changed.error};
    const auto verified = backend_->read(operation.spec);
    const auto matches = operation.value
        ? verified.success && !verified.missing && verified.value == operation.value
        : verified.missing;
    if (!matches) return {.code = u"bcd.verify_failed"_s,
                          .message = verified.error.isEmpty()
                              ? u"Проверка BCD-элемента не пройдена."_s : verified.error};
    return {.success = true};
}

BcdExecutionResult BcdElementExecutor::restore(const domain::BcdElementSnapshot& snapshot) const
{
    return apply({.spec = snapshot.spec,
                  .value = snapshot.existed ? snapshot.value : std::nullopt});
}

} // namespace tweakopedia::execution
