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
};

QTEST_APPLESS_MAIN(WindowsRegistryBackendTest)

#include "WindowsRegistryBackendTest.moc"
