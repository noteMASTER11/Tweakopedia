#include "domain/TweakInput.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace Qt::StringLiterals;

class TweakInputTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAllDeclaredTypes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bmpPath = directory.filePath(u"logo.BMP"_s);
        QFile file(bmpPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("BM"), 2);
        file.close();

        const QVector<TweakInputDefinition> definitions{
            {.id = u"name"_s, .label = u"Название"_s, .type = TweakInputType::Text,
             .required = true, .minimumLength = 2, .maximumLength = 20},
            {.id = u"count"_s, .label = u"Количество"_s, .type = TweakInputType::Integer,
             .minimum = 1, .maximum = 9},
            {.id = u"logo"_s, .label = u"Логотип"_s, .type = TweakInputType::File,
             .allowedExtensions = {u"bmp"_s}, .maximumFileSize = 10},
            {.id = u"mode"_s, .label = u"Режим"_s, .type = TweakInputType::Choice,
             .choices = {{u"auto"_s, u"Автоматически"_s}, {u"manual"_s, u"Вручную"_s}}},
            {.id = u"active"_s, .label = u"Активно"_s, .type = TweakInputType::Boolean},
        };
        const QVariantMap provided{
            {u"name"_s, u"  ACME  "_s}, {u"count"_s, 4}, {u"logo"_s, bmpPath},
            {u"mode"_s, u"manual"_s}, {u"active"_s, true},
        };

        const auto result = normalizeTweakInputs(definitions, provided);

        QVERIFY2(result.values.has_value(), qPrintable(result.issues.value(0).code));
        QCOMPARE(std::get<QString>(result.values->value(u"name"_s)), u"ACME"_s);
        QCOMPARE(std::get<qint64>(result.values->value(u"count"_s)), 4);
        QCOMPARE(std::get<QString>(result.values->value(u"logo"_s)), QDir::cleanPath(bmpPath));
        QCOMPARE(std::get<QString>(result.values->value(u"mode"_s)), u"manual"_s);
        QVERIFY(std::get<bool>(result.values->value(u"active"_s)));
    }

    void rejectsMissingInvalidAndUndeclaredValues_data()
    {
        QTest::addColumn<QVariantMap>("provided");
        QTest::addColumn<QString>("code");
        QTest::newRow("missing") << QVariantMap{} << u"input.required"_s;
        QTest::newRow("short") << QVariantMap{{u"name"_s, u"x"_s}} << u"input.text_too_short"_s;
        QTest::newRow("integer-type") << QVariantMap{{u"name"_s, u"ok"_s}, {u"count"_s, u"4"_s}}
                                      << u"input.type_invalid"_s;
        QTest::newRow("integer-range") << QVariantMap{{u"name"_s, u"ok"_s}, {u"count"_s, 10}}
                                       << u"input.integer_out_of_range"_s;
        QTest::newRow("choice") << QVariantMap{{u"name"_s, u"ok"_s}, {u"mode"_s, u"other"_s}}
                                << u"input.choice_unknown"_s;
        QTest::newRow("boolean") << QVariantMap{{u"name"_s, u"ok"_s}, {u"active"_s, 1}}
                                 << u"input.type_invalid"_s;
        QTest::newRow("undeclared") << QVariantMap{{u"name"_s, u"ok"_s}, {u"extra"_s, true}}
                                    << u"input.undeclared"_s;
    }

    void rejectsMissingInvalidAndUndeclaredValues()
    {
        QFETCH(QVariantMap, provided);
        QFETCH(QString, code);
        const QVector<TweakInputDefinition> definitions{
            {.id = u"name"_s, .label = u"Название"_s, .type = TweakInputType::Text,
             .required = true, .minimumLength = 2},
            {.id = u"count"_s, .label = u"Количество"_s, .type = TweakInputType::Integer,
             .minimum = 1, .maximum = 9},
            {.id = u"mode"_s, .label = u"Режим"_s, .type = TweakInputType::Choice,
             .choices = {{u"auto"_s, u"Автоматически"_s}}},
            {.id = u"active"_s, .label = u"Активно"_s, .type = TweakInputType::Boolean},
        };

        const auto result = normalizeTweakInputs(definitions, provided);

        QVERIFY(!result.values.has_value());
        QVERIFY(std::any_of(result.issues.cbegin(), result.issues.cend(),
                            [&](const auto& issue) { return issue.code == code; }));
    }

    void rejectsDisallowedFileExtension()
    {
        const QVector<TweakInputDefinition> definitions{{
            .id = u"logo"_s, .label = u"Логотип"_s, .type = TweakInputType::File,
            .required = true, .allowedExtensions = {u"bmp"_s},
        }};
        const auto result = normalizeTweakInputs(
            definitions, {{u"logo"_s, u"D:/Images/logo.png"_s}});
        QVERIFY(!result.values.has_value());
        QCOMPARE(result.issues.first().code, u"input.file_extension_invalid"_s);
    }
};

QTEST_APPLESS_MAIN(TweakInputTest)

#include "TweakInputTest.moc"
