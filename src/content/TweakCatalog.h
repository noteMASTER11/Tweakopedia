#pragma once

#include "domain/TweakDefinition.h"

#include <QVector>

namespace tweakopedia::content {

class TweakCatalog final
{
public:
    TweakCatalog() = default;
    explicit TweakCatalog(QVector<domain::TweakDefinition> tweaks);

    [[nodiscard]] qsizetype size() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] const QVector<domain::TweakDefinition>& tweaks() const noexcept;
    [[nodiscard]] const domain::TweakDefinition* find(const domain::TweakId& id) const noexcept;

private:
    QVector<domain::TweakDefinition> tweaks_;
};

} // namespace tweakopedia::content
