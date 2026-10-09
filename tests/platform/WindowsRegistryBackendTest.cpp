#include "platform/WindowsRegistryBackend.h"

#include <QtEndian>
#include <QtTest/QTest>
#include <QUuid>

#include <windows.h>

using namespace tweakopedia::domain;
using namespace tweakopedia::platform;
using namespace Qt::StringLiterals;

class WindowsRegistryBackendTest final : public QObject
{
    Q_OBJECT

private:
    QString key_;

    RegistryLocation location() const
    {
        return {
            .hive = RegistryHive::CurrentUser,
            .key = key_,
            .valueName = u"Value"_s,
            .view = RegistryView::Registry64,
        };
    }

    RegistryKeyLocation keyLocation() const
    {
        return {
            .hive = RegistryHive::CurrentUser,
            .key = key_,
            .view = RegistryView::Registry64,
        };
    }

private slots:
    void init()
    {
        key_ = u"Software\\Tweakopedia\\Tests\\"_s
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    void cleanup()
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(key_.utf16()));
    }

    void distinguishesMissingDwordAndStringInRegistry64View()
    {
        WindowsRegistryBackend backend;

        const auto missing = backend.read(location());
        QCOMPARE(missing.presence, RegistryPresence::Missing);
        QCOMPARE(missing.type, RegistryValueType::None);

        const auto write = backend.writeDword(location(), 1);
        QVERIFY(write.success);

        const auto dword = backend.read(location());
        QCOMPARE(dword.presence, RegistryPresence::Present);
        QCOMPARE(dword.type, RegistryValueType::Dword);
        QCOMPARE(dword.rawValue.size(), qsizetype{4});
        QCOMPARE(qFromLittleEndian<quint32>(dword.rawValue.constData()), quint32{1});

        HKEY key{};
        const auto createStatus = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            reinterpret_cast<LPCWSTR>(key_.utf16()),
            0,
            nullptr,
            0,
            KEY_SET_VALUE | KEY_WOW64_64KEY,
            nullptr,
            &key,
            nullptr);
        QCOMPARE(createStatus, LSTATUS{ERROR_SUCCESS});
        const wchar_t text[] = L"one";
        const auto setStatus = RegSetValueExW(
            key,
            L"Value",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(text),
            sizeof(text));
        RegCloseKey(key);
        QCOMPARE(setStatus, LSTATUS{ERROR_SUCCESS});

        const auto string = backend.read(location());
        QCOMPARE(string.presence, RegistryPresence::Present);
        QCOMPARE(string.type, RegistryValueType::String);

        const auto remove = backend.deleteValue(location());
        QVERIFY(remove.success);
        QCOMPARE(backend.read(location()).presence, RegistryPresence::Missing);
    }

    void writesGenericNativeTypesWithoutChangingBytes()
    {
        WindowsRegistryBackend backend;
        const QVector<RegistryValueSpec> values{
            RegistryValueSpec::dword(0x01020304U),
            RegistryValueSpec::qword(0x0102030405060708ULL),
            RegistryValueSpec::string(u"OEM"),
            RegistryValueSpec::expandString(u"%TEMP%"),
            RegistryValueSpec::multiString({u"one"_s, u"two"_s}),
            RegistryValueSpec::binary(QByteArray::fromHex("deadbeef")),
        };

        for (qsizetype index = 0; index < values.size(); ++index) {
            auto target = location();
            target.valueName = u"Value%1"_s.arg(index);
            QVERIFY(backend.writeValue(target, values.at(index)).success);
            const auto actual = backend.read(target);
            QCOMPARE(actual.presence, RegistryPresence::Present);
            QCOMPARE(actual.nativeType, values.at(index).nativeType);
            QCOMPARE(actual.rawValue, values.at(index).rawValue);
        }
    }

    void mapsDefaultValueSentinelToUnnamedRegistryValue()
    {
        WindowsRegistryBackend backend;
        auto target = location();
        target.valueName = u"(default)"_s;

        QVERIFY(backend.writeValue(target, RegistryValueSpec::string(u"unnamed")).success);
        const auto actual = backend.read(target);

        QCOMPARE(actual.presence, RegistryPresence::Present);
        QCOMPARE(actual.rawValue, RegistryValueSpec::string(u"unnamed").rawValue);
        QVERIFY(backend.deleteValue(target).success);
        QCOMPARE(backend.read(target).presence, RegistryPresence::Missing);
    }

    void snapshotsDeletesAndRestoresNestedTree()
    {
        WindowsRegistryBackend backend;
        auto rootValue = location();
        rootValue.valueName = u"RootText"_s;
        auto childValue = rootValue;
        childValue.key += u"\\Child"_s;
        childValue.valueName = u"ChildData"_s;
        QVERIFY(backend.writeValue(rootValue, RegistryValueSpec::string(u"root")).success);
        QVERIFY(backend.writeValue(
            childValue, RegistryValueSpec::binary(QByteArray::fromHex("deadbeef"))).success);

        const auto captured = backend.readTree(keyLocation());
        QVERIFY2(captured.success, qPrintable(captured.code));
        QVERIFY(captured.snapshot.existed);
        QCOMPARE(captured.snapshot.nodes.size(), 2);
        const auto jsonRoundTrip = RegistryTreeSnapshot::fromJson(captured.snapshot.toJson());
        QVERIFY(jsonRoundTrip.has_value());
        QCOMPARE(*jsonRoundTrip, captured.snapshot);

        QVERIFY(backend.deleteTree(keyLocation()).success);
        const auto missing = backend.readTree(keyLocation());
        QVERIFY(missing.success);
        QVERIFY(!missing.snapshot.existed);

        QVERIFY(backend.restoreTree(captured.snapshot).success);
        const auto restored = backend.readTree(keyLocation());
        QVERIFY(restored.success);
        QCOMPARE(restored.snapshot, captured.snapshot);
    }

    void stopsTreeSnapshotBeforeNodeLimit()
    {
        WindowsRegistryBackend backend;
        QVERIFY(backend.createKey(keyLocation()).success);
        auto child = keyLocation();
        child.key += u"\\Child"_s;
        QVERIFY(backend.createKey(child).success);

        const auto captured = backend.readTree(
            keyLocation(), RegistryTreeLimits{.maxNodes = 1, .maxBytes = 1024});

        QVERIFY(!captured.success);
        QCOMPARE(captured.code, u"registry.tree_limit"_s);
    }
};

QTEST_APPLESS_MAIN(WindowsRegistryBackendTest)

#include "WindowsRegistryBackendTest.moc"
