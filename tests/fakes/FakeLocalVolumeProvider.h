#pragma once

#include "platform/ILocalVolumeProvider.h"

namespace tweakopedia::tests {

class FakeLocalVolumeProvider final : public platform::ILocalVolumeProvider
{
public:
    QVector<platform::LocalVolume> volumes() const override { return values; }
    QVector<platform::LocalVolume> values;
};

} // namespace tweakopedia::tests

