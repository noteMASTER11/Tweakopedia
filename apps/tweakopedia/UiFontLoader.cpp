#include "UiFontLoader.h"

#include <QFontDatabase>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace tweakopedia::ui {

QString loadBundledUiFont()
{
    const auto fontId = QFontDatabase::addApplicationFont(u":/fonts/SF-Pro.ttf"_s);
    if (fontId < 0) return {};

    const auto families = QFontDatabase::applicationFontFamilies(fontId);
    for (const auto& family : families) {
        if (family.startsWith(u"SF Pro"_s, Qt::CaseInsensitive)) return family;
    }
    return {};
}

}
