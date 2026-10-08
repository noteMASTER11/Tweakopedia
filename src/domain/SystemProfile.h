#pragma once

#include <QString>

namespace tweakopedia::domain {

enum class WindowsFamily {
    Unknown,
    Windows10,
    Windows11,
};

enum class CpuArchitecture {
    Unknown,
    X64,
    Arm64,
    X86,
};

struct SystemProfile {
    WindowsFamily family{WindowsFamily::Unknown};
    quint32 build{};
    quint32 ubr{};
    QString edition;
    CpuArchitecture architecture{CpuArchitecture::Unknown};
};

} // namespace tweakopedia::domain
