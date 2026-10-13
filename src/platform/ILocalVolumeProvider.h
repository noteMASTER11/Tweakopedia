#pragma once

#include "platform/LocalVolume.h"

#include <QVector>

namespace tweakopedia::platform {

class ILocalVolumeProvider
{
public:
    virtual ~ILocalVolumeProvider() = default;
    [[nodiscard]] virtual QVector<LocalVolume> volumes() const = 0;
};

} // namespace tweakopedia::platform

