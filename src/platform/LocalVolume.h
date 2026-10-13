#pragma once

#include <QString>

namespace tweakopedia::platform {

struct LocalVolume {
    QString rootPath;
    QString target;
    QString label;
    QString fileSystem;
    quint64 totalBytes{};
    quint64 freeBytes{};
    bool systemVolume{};
};

} // namespace tweakopedia::platform

