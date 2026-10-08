#pragma once

#include <QString>

namespace tweakopedia::content {

struct CatalogError {
    QString code;
    QString message;
    QString filePath;
    int line{};
    int column{};
};

} // namespace tweakopedia::content
