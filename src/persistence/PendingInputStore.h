#pragma once

#include "domain/TweakInput.h"

#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace tweakopedia::persistence {

struct PendingInputResult {
    std::optional<domain::InputArtifact> artifact;
    QString code;
    QString message;
};

class PendingInputStore final
{
public:
    explicit PendingInputStore(
        QString root,
        std::function<void()> beforeSourceRecheck = {});

    [[nodiscard]] PendingInputResult importFile(
        QString inputId,
        const QString& sourcePath,
        const QStringList& allowedExtensions,
        quint64 maximumSize) const;
    [[nodiscard]] bool cleanupUnused(const QSet<QString>& liveStorageIds) const;
    [[nodiscard]] const QString& root() const noexcept;

private:
    QString root_;
    std::function<void()> beforeSourceRecheck_;
};

} // namespace tweakopedia::persistence
