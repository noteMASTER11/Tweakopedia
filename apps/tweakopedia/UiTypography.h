#pragma once

#include <QFont>
#include <QStringList>

namespace tweakopedia::ui {

class UiTypography final
{
public:
    [[nodiscard]] static QString preferredFamily(
        const QStringList& availableFamilies,
        const QString& systemFamily);
    [[nodiscard]] static QFont applicationFont(const QString& fontId = QStringLiteral("segoe"));
};

} // namespace tweakopedia::ui
