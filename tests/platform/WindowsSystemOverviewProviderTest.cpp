#include "platform/WindowsSystemOverviewProvider.h"

#include <QtTest/QTest>

#include <dxgi1_2.h>

#include <algorithm>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

QHash<QString, quint64> dxgiMemory()
{
    IDXGIFactory1* rawFactory{};
    if (FAILED(CreateDXGIFactory1(
            __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&rawFactory)))) {
        return {};
    }
    QHash<QString, quint64> result;
    for (UINT index = 0; ; ++index) {
        IDXGIAdapter1* adapter{};
        if (rawFactory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 description{};
        if (SUCCEEDED(adapter->GetDesc1(&description))
            && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            result.insert(
                QString::fromWCharArray(description.Description).simplified().toLower(),
                static_cast<quint64>(description.DedicatedVideoMemory));
        }
        adapter->Release();
    }
    rawFactory->Release();
    return result;
}

} // namespace

class WindowsSystemOverviewProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsNativeComputerOverview()
    {
        const auto actual = platform::WindowsSystemOverviewProvider{}.collect();

        QVERIFY2(actual.error.isEmpty(), qPrintable(actual.error));
        QVERIFY(!actual.computerName.isEmpty());
        QVERIFY(!actual.os.caption.isEmpty());
        QVERIFY(actual.os.buildNumber >= 10240);
        QVERIFY(!actual.processor.name.isEmpty());
        QVERIFY(actual.processor.coreCount > 0);
        QVERIFY(actual.memory.totalBytes > 0);
        QVERIFY(!actual.graphics.isEmpty());
        QVERIFY(!actual.disks.isEmpty());
    }

    void reportsDedicatedVideoMemoryFromDxgi()
    {
        const auto expected = dxgiMemory();
        QVERIFY2(!expected.isEmpty(), "DXGI не вернул аппаратные видеоадаптеры");

        const auto actual = platform::WindowsSystemOverviewProvider{}.collect();
        for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
            const auto found = std::find_if(
                actual.graphics.cbegin(), actual.graphics.cend(),
                [&](const domain::GraphicsAdapterOverview& adapter) {
                    const auto name = adapter.name.simplified().toLower();
                    return name.contains(it.key()) || it.key().contains(name);
                });
            QVERIFY2(found != actual.graphics.cend(), qPrintable(it.key()));
            QCOMPARE(found->adapterRamBytes, it.value());
        }
    }
};

QTEST_APPLESS_MAIN(WindowsSystemOverviewProviderTest)

#include "WindowsSystemOverviewProviderTest.moc"
