#include "fakes/FakeSystemToolRunner.h"

namespace tweakopedia::tests {

bool FakeSystemToolRunner::start(execution::SystemToolOperation operation)
{
    ++startCalls;
    lastOperation = operation;
    return startResult;
}

} // namespace tweakopedia::tests
