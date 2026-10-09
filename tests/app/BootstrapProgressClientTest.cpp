#include "BootstrapProgressClient.h"

#include "startup/StartupIpc.h"

#include <QtTest/QTest>

using namespace tweakopedia;

namespace {

class FakeBootstrapWindowChannel final : public app::IBootstrapWindowChannel
{
public:
    bool valid{true};
    QRect bounds{100, 200, 1180, 760};
    mutable QVector<startup::StartupIpcMessage> sent;

    bool exists(std::uintptr_t handle) const override
    {
        return valid && handle == 4242U;
    }

    bool send(std::uintptr_t handle, const startup::StartupIpcMessage& message) const override
    {
        if (!exists(handle)) return false;
        sent.append(message);
        return true;
    }

    std::optional<QRect> geometry(std::uintptr_t handle) const override
    {
        return exists(handle) ? std::optional(bounds) : std::nullopt;
    }
};

}

class BootstrapProgressClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidOrMissingWindowArguments()
    {
        FakeBootstrapWindowChannel channel;
        QVERIFY(!app::BootstrapProgressClient::fromArgument({}, channel));
        QVERIFY(!app::BootstrapProgressClient::fromArgument(u"not-a-window", channel));
        QVERIFY(!app::BootstrapProgressClient::fromArgument(u"0", channel));
        QVERIFY(!app::BootstrapProgressClient::fromArgument(
            u"184467440737095516160", channel));

        channel.valid = false;
        QVERIFY(!app::BootstrapProgressClient::fromArgument(u"4242", channel));
    }

    void sendsVersionedMessagesForKnownStagesOnly()
    {
        FakeBootstrapWindowChannel channel;
        auto client = app::BootstrapProgressClient::fromArgument(u"4242", channel);
        QVERIFY(client);

        QVERIFY(client->report(startup::StartupStage::CatalogLoad));
        QCOMPARE(channel.sent.size(), 1);
        const auto decoded = startup::decodeStartupMessage(
            &channel.sent.first(), sizeof(startup::StartupIpcMessage));
        QVERIFY(decoded);
        QCOMPARE(*decoded, startup::StartupStage::CatalogLoad);

        QVERIFY(!client->report(static_cast<startup::StartupStage>(999U)));
        QCOMPARE(channel.sent.size(), 1);

        QVERIFY(client->ready());
        QCOMPARE(channel.sent.size(), 2);
        const auto ready = startup::decodeStartupMessage(
            &channel.sent.last(), sizeof(startup::StartupIpcMessage));
        QVERIFY(ready);
        QCOMPARE(*ready, startup::StartupStage::Ready);
    }

    void exposesGeometryOnlyWhileWindowExists()
    {
        FakeBootstrapWindowChannel channel;
        auto client = app::BootstrapProgressClient::fromArgument(u"4242", channel);
        QVERIFY(client);
        QCOMPARE(client->splashGeometry(), std::optional(QRect(100, 200, 1180, 760)));

        channel.valid = false;
        QVERIFY(!client->splashGeometry());
        QVERIFY(!client->report(startup::StartupStage::Models));
    }
};

QTEST_MAIN(BootstrapProgressClientTest)

#include "BootstrapProgressClientTest.moc"
