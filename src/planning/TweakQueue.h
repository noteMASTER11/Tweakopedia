#pragma once

#include "domain/TweakDefinition.h"

#include <QString>
#include <QVector>

namespace tweakopedia::planning {

struct QueueItem {
    domain::TweakId tweakId;
    QString targetState;
};

struct QueueChangeResult {
    bool accepted{};
    QString errorCode;
};

class TweakQueue final
{
public:
    [[nodiscard]] QueueChangeResult setTarget(
        const domain::TweakDefinition& tweak,
        QStringView targetState);
    [[nodiscard]] bool remove(const domain::TweakId& tweakId);
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] qsizetype size() const noexcept;
    [[nodiscard]] const QVector<QueueItem>& items() const noexcept;

private:
    QVector<QueueItem> items_;
};

} // namespace tweakopedia::planning
