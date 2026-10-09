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
    for (const auto& preferred : {u"Segoe UI Variable"_s, u"Segoe UI"_s}) {
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
    font.setHintingPreference(QFont::PreferVerticalHinting);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        font.styleStrategy() | QFont::PreferTypoLineMetrics));
    return font;
}

} // namespace tweakopedia::ui
