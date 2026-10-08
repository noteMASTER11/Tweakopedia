#include "content/TweakCatalog.h"

namespace tweakopedia::content {

TweakCatalog::TweakCatalog(QVector<domain::TweakDefinition> tweaks)
    : tweaks_(std::move(tweaks))
{
}

qsizetype TweakCatalog::size() const noexcept
{
    return tweaks_.size();
}

bool TweakCatalog::isEmpty() const noexcept
{
    return tweaks_.isEmpty();
}

const QVector<domain::TweakDefinition>& TweakCatalog::tweaks() const noexcept
{
    return tweaks_;
}

const domain::TweakDefinition* TweakCatalog::find(const domain::TweakId& id) const noexcept
{
    for (const auto& tweak : tweaks_) {
        if (tweak.id == id) {
            return &tweak;
        }
    }
    return nullptr;
}

} // namespace tweakopedia::content
