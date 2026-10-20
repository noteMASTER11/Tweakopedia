#include "UiTypography.h"

#include <QGuiApplication>
#include <QFontDatabase>
#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace tweakopedia::ui {

namespace {

void registerWindowsUiFonts()
{
#ifdef Q_OS_WIN
    static const bool registered = [] {
        const auto windowsRoot = qEnvironmentVariable("WINDIR", u"C:/Windows"_s);
        const QDir fontDirectory(QDir::cleanPath(windowsRoot + u"/Fonts"_s));
        const QStringList fontFiles{
            u"seguivar.ttf"_s,
            u"segoeui.ttf"_s,
            u"segoeuisl.ttf"_s,
            u"seguisb.ttf"_s,
            u"segoeuib.ttf"_s,
            u"segoeuii.ttf"_s,
            u"seguisbi.ttf"_s,
            u"segoeuiz.ttf"_s,
        };
        for (const auto& fileName : fontFiles) {
            const auto path = fontDirectory.filePath(fileName);
            if (QFileInfo(path).isFile()) QFontDatabase::addApplicationFont(path);
        }
        return true;
    }();
    Q_UNUSED(registered);
#endif
}

} // namespace

QString UiTypography::preferredFamily(
    const QStringList& availableFamilies,
    const QString& systemFamily)
{
    // The static Segoe UI family produces more predictable stem weights than
    // the variable face in Qt Quick's native renderer, especially at 100–150%.
    for (const auto& preferred : {u"Segoe UI"_s, u"Segoe UI Variable"_s}) {
        for (const auto& available : availableFamilies) {
            if (available.compare(preferred, Qt::CaseInsensitive) == 0) return available;
        }
    }
    return systemFamily;
}

QFont UiTypography::applicationFont()
{
    registerWindowsUiFonts();
    auto font = QGuiApplication::font();
    font.setFamily(preferredFamily(QFontDatabase::families(), font.family()));
    font.setWeight(QFont::Medium);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setKerning(true);

    auto strategy = static_cast<int>(font.styleStrategy());
    strategy &= ~static_cast<int>(QFont::NoAntialias);
    strategy &= ~static_cast<int>(QFont::NoSubpixelAntialias);
    strategy |= static_cast<int>(QFont::PreferAntialias);
    strategy |= static_cast<int>(QFont::PreferQuality);
    strategy |= static_cast<int>(QFont::PreferTypoLineMetrics);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(strategy));
    return font;
}

} // namespace tweakopedia::ui
