#pragma once

#include "execution/ISystemToolRunner.h"

namespace tweakopedia::tests {

class FakeSystemToolRunner final : public execution::ISystemToolRunner
{
    Q_OBJECT

public:
    using ISystemToolRunner::ISystemToolRunner;

    bool start(execution::SystemToolOperation operation) override;

    bool startResult{true};
    int startCalls{};
    execution::SystemToolOperation lastOperation{execution::SystemToolOperation::Scan};
};

} // namespace tweakopedia::tests
