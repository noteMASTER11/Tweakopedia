#pragma once

#include "execution/SystemToolProtocol.h"

#include <QByteArray>
#include <QByteArrayView>

namespace tweakopedia::execution {

class DismOutputParser final
{
public:
    void reset(SystemToolStage stage);
    void consume(QByteArrayView chunk);

    [[nodiscard]] int progress() const noexcept;
    [[nodiscard]] SystemToolOutcome health() const noexcept;

private:
    SystemToolStage stage_{SystemToolStage::ScanHealth};
    QByteArray recent_;
    int progress_{-1};
    SystemToolOutcome health_{SystemToolOutcome::Unknown};
};

} // namespace tweakopedia::execution
