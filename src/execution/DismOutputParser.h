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
    [[nodiscard]] ComponentStoreHealth health() const noexcept;

private:
    SystemToolStage stage_{SystemToolStage::ScanHealth};
    QByteArray recent_;
    int progress_{-1};
    ComponentStoreHealth health_{ComponentStoreHealth::Unknown};
};

} // namespace tweakopedia::execution
