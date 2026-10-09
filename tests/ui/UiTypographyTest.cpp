#include "UiTypography.h"

#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

QString qmlBlockAt(const QString& source, qsizetype openingBrace)
{
    int depth{};
    bool quoted{};
    QChar quote;
    for (auto index = openingBrace; index < source.size(); ++index) {
        const auto character = source.at(index);
        if (quoted) {
            if (character == quote && (index == 0 || source.at(index - 1) != u'\\')) quoted = false;
            continue;
        }
        if (character == u'\'' || character == u'"') {
            quoted = true;
            quote = character;
        } else if (character == u'{') {
            ++depth;
        } else if (character == u'}' && --depth == 0) {
            return source.mid(openingBrace, index - openingBrace + 1);
        }
    }
    return source.mid(openingBrace);
}

} // namespace

class UiTypographyTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesFluentFamilyInDeclaredOrder()
    {
        QCOMPARE(ui::UiTypography::preferredFamily(
                     {u"Arial"_s, u"Segoe UI"_s, u"Segoe UI Variable"_s}, u"Arial"_s),
                 u"Segoe UI Variable"_s);
        QCOMPARE(ui::UiTypography::preferredFamily(
                     {u"Arial"_s, u"Segoe UI"_s}, u"Arial"_s),
                 u"Segoe UI"_s);
        QCOMPARE(ui::UiTypography::preferredFamily({u"Arial"_s}, u"Arial"_s), u"Arial"_s);
    }

    void configuresNativeFluentFontMetricsAndWeights()
    {
        const auto font = ui::UiTypography::applicationFont();
        QVERIFY(!font.family().isEmpty());
        QCOMPARE(font.hintingPreference(), QFont::PreferVerticalHinting);
        QVERIFY((font.styleStrategy() & QFont::PreferTypoLineMetrics) != 0);

        for (const auto weight : {QFont::Normal, QFont::Medium, QFont::DemiBold, QFont::Bold}) {
            auto weighted = font;
            weighted.setWeight(weight);
            QCOMPARE(weighted.family(), font.family());
            QCOMPARE(weighted.weight(), weight);
        }
    }

    void applicationFontCoversRussianInterfaceText()
    {
        const QFontMetrics metrics(ui::UiTypography::applicationFont());
        for (const auto character : QStringView(u"Твикопедия Настройки Применить")) {
            if (!character.isSpace()) QVERIFY2(metrics.inFont(character),
                qPrintable(u"В выбранном шрифте отсутствует символ: "_s + character));
        }
    }

    void ordinaryQmlTextUsesFluentText()
    {
        const QRegularExpression ordinaryText(uR"((^|\s)Text\s*\{)"_s);
        QDirIterator files(
            QStringLiteral(TWEAKOPEDIA_TEST_QML_ROOT), {u"*.qml"_s}, QDir::Files,
            QDirIterator::Subdirectories);
        QStringList violations;
        while (files.hasNext()) {
            const auto path = files.next();
            if (path.endsWith(u"/FluentText.qml"_s, Qt::CaseInsensitive)
                || path.endsWith(u"\\FluentText.qml"_s, Qt::CaseInsensitive)) {
                continue;
            }
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto source = QString::fromUtf8(file.readAll());
            auto offset = qsizetype{};
            while (true) {
                const auto match = ordinaryText.match(source, offset);
                if (!match.hasMatch()) break;
                const auto brace = source.indexOf(u'{', match.capturedStart());
                const auto block = qmlBlockAt(source, brace);
                if (!block.contains(uR"(font.family: "Segoe MDL2 Assets")"_s)
                    && !block.contains(uR"(font.family: "Consolas")"_s)) {
                    const auto line = source.left(match.capturedStart()).count(u'\n') + 1;
                    violations.append(path + u":"_s + QString::number(line));
                }
                offset = match.capturedEnd();
            }
        }
        QVERIFY2(violations.isEmpty(), qPrintable(
            u"Обычный Text вне FluentText: "_s + violations.join(u", "_s)));
    }
};

QTEST_MAIN(UiTypographyTest)
#include "UiTypographyTest.moc"
