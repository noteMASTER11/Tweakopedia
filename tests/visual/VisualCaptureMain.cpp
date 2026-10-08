#include <QCommandLineParser>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "UiFontLoader.h"

using namespace Qt::StringLiterals;

namespace {

void loadWindowsSymbolFonts()
{
    const QStringList fontFiles{
        u"seguisym.ttf"_s,
        u"segmdl2.ttf"_s,
    };

    for (const auto& fileName : fontFiles) {
        const auto fontPath = u"C:/Windows/Fonts/"_s + fileName;
        if (!QFileInfo::exists(fontPath)) continue;
        (void)QFontDatabase::addApplicationFont(fontPath);
    }
}

}

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(u"Basic"_s);
    QGuiApplication application(argc, argv);
    if (tweakopedia::ui::loadBundledUiFont().isEmpty()) return 5;
    loadWindowsSymbolFonts();

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption inputOption(u"input"_s, u"Входной QML-файл."_s, u"path"_s);
    const QCommandLineOption outputOption(u"output"_s, u"Выходной PNG-файл."_s, u"path"_s);
    const QCommandLineOption widthOption(u"width"_s, u"Ширина окна."_s, u"pixels"_s, u"1280"_s);
    const QCommandLineOption heightOption(u"height"_s, u"Высота окна."_s, u"pixels"_s, u"800"_s);
    parser.addOptions({inputOption, outputOption, widthOption, heightOption});
    parser.process(application);

    bool widthValid{};
    bool heightValid{};
    const auto width = parser.value(widthOption).toInt(&widthValid);
    const auto height = parser.value(heightOption).toInt(&heightValid);
    const auto input = QFileInfo(parser.value(inputOption)).absoluteFilePath();
    const auto output = QFileInfo(parser.value(outputOption)).absoluteFilePath();
    if (input.isEmpty() || output.isEmpty() || !widthValid || !heightValid
        || width <= 0 || height <= 0) {
        return 2;
    }

    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(input));
    if (engine.rootObjects().isEmpty()) return 3;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (!window) return 3;
    window->resize(width, height);
    window->show();

    bool captured{};
    const auto capture = [&] {
        if (captured) return;
        captured = true;
        const auto image = window->grabWindow();
        application.exit(!image.isNull() && image.save(output, "PNG") ? 0 : 4);
    };
    QObject::connect(window, &QQuickWindow::frameSwapped, &application, [&] {
        QTimer::singleShot(0, &application, capture);
    });
    QTimer::singleShot(3000, &application, capture);
    window->requestUpdate();
    return application.exec();
}
