#pragma once

#include "domain/SystemOverview.h"

#include <QVariantMap>

namespace tweakopedia::app {

class SystemOverviewPresenter final
{
public:
    [[nodiscard]] static QVariantMap present(
        const domain::SystemOverviewSnapshot& snapshot);
};

} // namespace tweakopedia::app
