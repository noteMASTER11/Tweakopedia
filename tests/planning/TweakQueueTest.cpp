#include "fakes/FakeRegistryBackend.h"
#include "planning/TweakQueue.h"

#include <QtEndian>
#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace tweakopedia::planning;
using namespace tweakopedia::platform;
using namespace tweakopedia::tests;
using namespace Qt::StringLiterals;

namespace {

RegistryLocation location()
{
    return {
        .hive = RegistryHive::LocalMachine,
        .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
        .valueName = u"LongPathsEnabled"_s,
        .view = RegistryView::Registry64,
    };
}

TweakDefinition definition()
{
    TweakDefinition tweak;
    tweak.id = *TweakId::parse(u"filesystem.win32-long-paths");
    tweak.states = {
        {.id = u"disabled"_s, .title = u"Выключено"_s},
        {.id = u"enabled"_s, .title = u"Включено"_s},
    };
    return tweak;
}

QByteArray dwordBytes(quint32 value)
{
    QByteArray bytes(sizeof(value), Qt::Uninitialized);
    qToLittleEndian(value, bytes.data());
    return bytes;
}

} // namespace

class TweakQueueTest final : public QObject
{
    Q_OBJECT

private slots:
    void replacesTargetStateWithoutDuplicatingItem()
    {
        TweakQueue queue;
        const auto tweak = definition();

        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);
        QVERIFY(queue.setTarget(tweak, u"disabled").accepted);

        QCOMPARE(queue.size(), 1);
        QCOMPARE(queue.items().first().targetState, u"disabled"_s);
    }

    void removesSelectedTweak()
    {
        TweakQueue queue;
        const auto tweak = definition();
        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);

        QVERIFY(queue.remove(tweak.id));
        QVERIFY(queue.isEmpty());
    }

    void rejectsUnknownTargetState()
    {
        TweakQueue queue;

        const auto result = queue.setTarget(definition(), u"custom");

        QVERIFY(!result.accepted);
        QCOMPARE(result.errorCode, u"state.unknown"_s);
        QVERIFY(queue.isEmpty());
    }

    void selectingTargetDoesNotWriteRegistry()
    {
        FakeRegistryBackend backend;
        backend.setReadResult(location(), RegistryReadResult::present(RegistryValueType::Dword, dwordBytes(0)));
        const auto before = backend.read(location());

        TweakQueue queue;
        QVERIFY(queue.setTarget(definition(), u"enabled").accepted);

        const auto after = backend.read(location());
        QCOMPARE(after.presence, before.presence);
        QCOMPARE(after.type, before.type);
        QCOMPARE(after.rawValue, before.rawValue);
    }

    void normalizesAndReplacesParameterizedTarget()
    {
        auto tweak = definition();
        tweak.inputs = {{
            .id = u"manufacturer"_s,
            .label = u"Производитель"_s,
            .type = TweakInputType::Text,
            .required = true,
            .maximumLength = 40,
        }};
        tweak.states[1].operations = {SetRegistryValueOperation{
            .location = location(), .value = RegistryValueSpec::string({}),
            .valueInput = u"manufacturer"_s,
        }};
        TweakQueue queue;

        QVERIFY(queue.setTarget(tweak, u"enabled", {{u"manufacturer"_s, u"  ACME  "_s}}).accepted);
        QVERIFY(queue.setTarget(tweak, u"enabled", {{u"manufacturer"_s, u"Contoso"_s}}).accepted);

        QCOMPARE(queue.size(), 1);
        QCOMPARE(std::get<QString>(queue.items().first().inputs.value(u"manufacturer"_s)),
                 u"Contoso"_s);
    }

    void rejectsMissingRequiredParameterizedValue()
    {
        auto tweak = definition();
        tweak.inputs = {{
            .id = u"manufacturer"_s, .label = u"Производитель"_s,
            .type = TweakInputType::Text, .required = true,
        }};
        tweak.states[1].operations = {SetRegistryValueOperation{
            .location = location(), .value = RegistryValueSpec::string({}),
            .valueInput = u"manufacturer"_s,
        }};
        TweakQueue queue;

        const auto result = queue.setTarget(tweak, u"enabled", {});

        QVERIFY(!result.accepted);
        QCOMPARE(result.errorCode, u"input.required"_s);
        QVERIFY(queue.isEmpty());
    }

    void doesNotRequireInputUnusedBySelectedState()
    {
        auto tweak = definition();
        tweak.inputs = {{
            .id = u"manufacturer"_s, .label = u"Производитель"_s,
            .type = TweakInputType::Text, .required = true,
        }};
        tweak.states[0].operations = {DeleteRegistryValueOperation{location()}};
        tweak.states[1].operations = {SetRegistryValueOperation{
            .location = location(),
            .value = RegistryValueSpec::string({}),
            .valueInput = u"manufacturer"_s,
        }};
        TweakQueue queue;

        const auto cleared = queue.setTarget(tweak, u"disabled", {});
        const auto configured = queue.setTarget(tweak, u"enabled", {});

        QVERIFY(cleared.accepted);
        QVERIFY(!configured.accepted);
        QCOMPARE(configured.errorCode, u"input.required"_s);
    }
};

QTEST_APPLESS_MAIN(TweakQueueTest)

#include "TweakQueueTest.moc"
