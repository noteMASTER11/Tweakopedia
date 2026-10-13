#pragma once

#include "execution/ISystemToolRunner.h"

namespace tweakopedia::tests {

class FakeSystemToolRunner final : public execution::ISystemToolRunner
{
    Q_OBJECT

public:
    using ISystemToolRunner::ISystemToolRunner;

    bool start(execution::SystemToolKind tool,
               execution::SystemToolOperation operation,
               QString target = {}) override;

    bool startResult{true};
    int startCalls{};
    execution::SystemToolKind lastTool{execution::SystemToolKind::ComponentStore};
    execution::SystemToolOperation lastOperation{execution::SystemToolOperation::Scan};
    QString lastTarget;
};

} // namespace tweakopedia::tests
