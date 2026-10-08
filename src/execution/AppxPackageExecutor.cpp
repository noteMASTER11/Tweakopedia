#include "execution/AppxPackageExecutor.h"

#include <QCryptographicHash>
#include <QJsonArray>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

QByteArray AppxPackageSnapshot::fingerprint() const
{
    QVector<QString> fullNames;
    fullNames.reserve(packages.size());
    for (const auto& package : packages) fullNames.append(package.fullName);
    std::sort(fullNames.begin(), fullNames.end(), [](const QString& left, const QString& right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& fullName : fullNames) {
        hash.addData(fullName.toUtf8());
        hash.addData("\n");
    }
    return hash.result().toHex();
}

QJsonObject AppxPackageSnapshot::toJson() const
{
    QJsonArray values;
    for (const auto& package : packages) {
        values.append(QJsonObject{
            {u"name"_s, package.name},
            {u"full_name"_s, package.fullName},
        });
    }
    return {
        {u"type"_s, u"appx.packages"_s},
        {u"package_name"_s, packageName},
        {u"packages"_s, values},
    };
}

AppxPackageExecutor::AppxPackageExecutor(platform::IAppxPackageBackend& backend)
    : backend_(&backend)
{
}

AppxExecutionResult AppxPackageExecutor::capture(QStringView packageName) const
{
    const auto query = backend_->installedForCurrentUser();
    if (!query.error.isEmpty()) {
        return {.code = u"appx.query_failed"_s, .message = query.error};
    }
    AppxPackageSnapshot snapshot{.packageName = packageName.toString()};
    for (const auto& package : query.packages) {
        if (package.name.compare(packageName, Qt::CaseInsensitive) == 0) {
            snapshot.packages.append(package);
        }
    }
    return {.success = true, .snapshot = std::move(snapshot)};
}

AppxExecutionResult AppxPackageExecutor::compareBefore(
    const AppxPackageSnapshot& snapshot,
    const QByteArray& expectedFingerprint) const
{
    if (snapshot.fingerprint() != expectedFingerprint) {
        return {
            .code = u"state.changed"_s,
            .message = u"Состав установленного AppX-пакета изменился после предпросмотра."_s,
            .snapshot = snapshot,
        };
    }
    return {.success = true, .snapshot = snapshot};
}

AppxExecutionResult AppxPackageExecutor::apply(const AppxPackageSnapshot& snapshot)
{
    for (const auto& package : snapshot.packages) {
        const auto removed = backend_->removeCurrentUser(package.fullName);
        if (!removed.success) {
            return {
                .code = u"appx.remove_failed"_s,
                .message = removed.error,
                .snapshot = snapshot,
            };
        }
    }
    const auto remaining = capture(snapshot.packageName);
    if (!remaining.success) return remaining;
    if (!remaining.snapshot.packages.isEmpty()) {
        return {
            .code = u"appx.verify_failed"_s,
            .message = u"После удаления пакет всё ещё обнаруживается в текущей учётной записи."_s,
            .snapshot = snapshot,
        };
    }
    return {.success = true, .snapshot = snapshot};
}

} // namespace tweakopedia::execution
