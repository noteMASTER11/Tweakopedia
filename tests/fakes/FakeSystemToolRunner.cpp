#include "fakes/FakeSystemToolRunner.h"

namespace tweakopedia::tests {

bool FakeSystemToolRunner::start(execution::SystemToolKind tool,
                                 execution::SystemToolOperation operation,
                                 QString target)
{
    ++startCalls;
    lastTool = tool;
    lastOperation = operation;
    lastTarget = std::move(target);
    return startResult;
}

} // namespace tweakopedia::tests
