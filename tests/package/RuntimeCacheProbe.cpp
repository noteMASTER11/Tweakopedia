#include "package/PackageFooter.h"
#include "package/RuntimeCache.h"

#include <windows.h>

#include <filesystem>

using namespace tweakopedia;

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) return 2;
    const std::filesystem::path container(argv[1]);
    const auto info = package::inspectContainer(container);
    if (!info.ok()) return 3;
    package::RuntimeCache cache{std::filesystem::path(argv[2])};
    auto prepared = cache.prepare(container, *info.value);
    if (!prepared.ok()) return 4;
    Sleep(400);
    return 0;
}
