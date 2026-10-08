#pragma once

#include "domain/TweakDefinition.h"

#include <QSet>
#include <QStringList>
#include <QVector>

namespace tweakopedia::content {

struct AppRemovalMetadata {
    QString friendlyName;
    QString packageName;
    QString description;
    QString recommendation;
};

struct AppRemovalMetadataResult {
    QVector<AppRemovalMetadata> apps;
    QStringList errors;
};

struct AppRemovalCatalogResult {
    QVector<domain::TweakDefinition> tweaks;
    QStringList errors;
};

class AppRemovalCatalogLoader final
{
public:
    [[nodiscard]] AppRemovalMetadataResult readMetadata(const QString& path) const;
    [[nodiscard]] AppRemovalCatalogResult loadFile(
        const QString& path,
        const QSet<QString>& installedPackageNames) const;
};

} // namespace tweakopedia::content
