#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::content;
using namespace tweakopedia::domain;
using namespace Qt::StringLiterals;

namespace {

QString fixturePath(QStringView relative)
{
    return QDir(QString::fromUtf8(TWEAKOPEDIA_TEST_FIXTURES)).filePath(relative.toString());
}

void copyFixture(const QString& relative, const QString& targetDirectory, const QString& targetName = {})
{
    const auto source = fixturePath(relative);
    const auto destination = QDir(targetDirectory).filePath(
        targetName.isEmpty() ? QFileInfo(source).fileName() : targetName);
    QVERIFY2(QFile::copy(source, destination), qPrintable(u"Не удалось скопировать fixture: "_s + source));
}

bool hasErrorCode(const CatalogLoadResult& result, QStringView code)
{
    return std::any_of(result.errors.cbegin(), result.errors.cend(), [code](const CatalogError& error) {
        return error.code == code;
    });
}

QString formatErrors(const CatalogLoadResult& result)
{
    QStringList lines;
    for (const auto& error : result.errors) {
        lines.append(u"%1:%2:%3 [%4] %5"_s
                         .arg(error.filePath)
                         .arg(error.line)
                         .arg(error.column)
                         .arg(error.code, error.message));
    }
    return lines.join(u'\n');
}

CatalogLoadResult loadSingleFixture(const QString& relative)
{
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return {};
    }
    copyFixture(relative, directory.path());
    return TweakCatalogLoader{}.loadDirectory(directory.path());
}

} // namespace

class TweakCatalogLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsValidRegistryDwordTweak()
    {
        const auto result = loadSingleFixture(u"valid/win32-long-paths.yaml"_s);

        QVERIFY2(result.errors.isEmpty(), qPrintable(formatErrors(result)));
        QVERIFY(result.catalog.has_value());
        QCOMPARE(result.catalog->size(), 1);

        const auto id = *TweakId::parse(u"filesystem.win32-long-paths");
        const auto* tweak = result.catalog->find(id);
        QVERIFY(tweak != nullptr);
        QCOMPARE(tweak->title, u"Поддержка длинных путей Win32"_s);
        QCOMPARE(tweak->detection->location.key, u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s);
        QCOMPARE(tweak->detection->location.view, RegistryView::Registry64);
        QCOMPARE(tweak->detection->missingState, u"disabled"_s);
    }

    void rejectsMissingRequiredField()
    {
        const auto result = loadSingleFixture(u"invalid/missing-title.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"field.required"));
    }

    void rejectsUnknownField()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-field.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"field.unknown"));
    }

    void rejectsInvalidDwordValue()
    {
        const auto result = loadSingleFixture(u"invalid/bad-dword.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"value.invalid_dword"));
    }

    void rejectsUnknownStateReference()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-state.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"state.unknown"));
    }

    void rejectsDuplicateIdsAcrossFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        copyFixture(u"invalid/duplicate-id-a.yaml"_s, directory.path());
        copyFixture(u"invalid/duplicate-id-b.yaml"_s, directory.path());

        const auto result = TweakCatalogLoader{}.loadDirectory(directory.path());

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"id.duplicate"));
    }

    void rejectsShellOperation()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-operation.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"operation.unknown"));
    }

    void reportsFileAndYamlPosition()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-field.yaml"_s);

        QVERIFY(!result.errors.isEmpty());
        QVERIFY(!result.errors.first().filePath.isEmpty());
        QVERIFY(result.errors.first().line > 0);
        QVERIFY(result.errors.first().column > 0);
    }

    void loadsRealCatalog()
    {
        const auto tweaksDirectory = QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"tweaks"_s);
        const auto result = TweakCatalogLoader{}.loadDirectory(tweaksDirectory);

        QVERIFY2(result.errors.isEmpty(), qPrintable(formatErrors(result)));
        QVERIFY(result.catalog.has_value());
        QCOMPARE(result.catalog->size(), 142);

        const QStringList expectedIds{
            u"apps.windows-ink-workspace"_s,
            u"apps.developer-mode"_s,
            u"apps.onedrive-sync"_s,
            u"apps.defender-archive-scanning"_s,
            u"apps.defender-behavior-monitoring"_s,
            u"apps.defender-cloud-protection"_s,
            u"apps.defender-download-scanning"_s,
            u"apps.defender-email-scanning"_s,
            u"apps.defender-network-scanning"_s,
            u"apps.defender-pua-protection"_s,
            u"apps.defender-realtime-monitoring"_s,
            u"apps.defender-removable-scanning"_s,
            u"apps.defender-sample-submission"_s,
            u"apps.defender-script-scanning"_s,
            u"apps.store-auto-updates"_s,
            u"accounts.lsa-protection"_s,
            u"accounts.microsoft-accounts"_s,
            u"accounts.uac"_s,
            u"accounts.uac-admin-prompt"_s,
            u"accounts.uac-built-in-administrator"_s,
            u"accounts.uac-installer-detection"_s,
            u"accounts.uac-secure-desktop"_s,
            u"accounts.uac-signed-executables-only"_s,
            u"accounts.uac-standard-user-prompt"_s,
            u"accounts.uac-uiaccess-desktop-toggle"_s,
            u"accounts.uac-uiaccess-secure-paths"_s,
            u"accounts.uac-virtualization"_s,
            u"behavior.autoplay"_s,
            u"behavior.automatic-maintenance"_s,
            u"behavior.bsod-details"_s,
            u"behavior.chkdsk-timeout"_s,
            u"behavior.crash-on-ctrl-scroll-lock"_s,
            u"behavior.disable-aero-shake"_s,
            u"behavior.download-zone-information"_s,
            u"behavior.clipboard-history"_s,
            u"behavior.cross-device-clipboard"_s,
            u"behavior.new-app-notification"_s,
            u"behavior.store-app-lookup"_s,
            u"behavior.spelling-autocorrect"_s,
            u"behavior.spelling-highlighting"_s,
            u"behavior.text-predictions"_s,
            u"behavior.text-prediction-spacing"_s,
            u"boot.hide-last-user-name"_s,
            u"boot.last-logon-info"_s,
            u"boot.lock-screen"_s,
            u"boot.login-network-icon"_s,
            u"boot.login-power-button"_s,
            u"boot.lock-screen-camera"_s,
            u"boot.password-reveal-button"_s,
            u"boot.require-ctrl-alt-delete"_s,
            u"boot.verbose-logon-messages"_s,
            u"desktop.account-notifications"_s,
            u"desktop.drive-letter-position"_s,
            u"desktop.last-active-taskbar-window"_s,
            u"desktop.legacy-balloon-notifications"_s,
            u"desktop.live-tile-notifications"_s,
            u"desktop.multi-file-context-menu-limit"_s,
            u"desktop.notification-center"_s,
            u"desktop.cloud-optimized-content"_s,
            u"desktop.cloud-search"_s,
            u"desktop.consumer-account-content"_s,
            u"desktop.consumer-experiences"_s,
            u"desktop.cortana"_s,
            u"desktop.most-used-apps"_s,
            u"desktop.program-launch-tracking"_s,
            u"desktop.recent-documents-history"_s,
            u"desktop.recently-added-apps"_s,
            u"desktop.removable-drive-indexing"_s,
            u"desktop.search-box-suggestions"_s,
            u"desktop.search-history"_s,
            u"desktop.search-location"_s,
            u"desktop.taskbar-animations"_s,
            u"desktop.taskbar-clock-seconds"_s,
            u"desktop.taskbar-flash-count"_s,
            u"desktop.taskbar-thumbnail-delay"_s,
            u"desktop.third-party-suggestions"_s,
            u"desktop.unsupported-hardware-notifications"_s,
            u"desktop.web-search"_s,
            u"desktop.welcome-experience"_s,
            u"desktop.version-watermark"_s,
            u"desktop.wallpaper-jpeg-quality"_s,
            u"desktop.windows-search"_s,
            u"desktop.windows-spotlight"_s,
            u"desktop.windows-tips"_s,
            u"devices.biometrics"_s,
            u"devices.camera-activity-notification"_s,
            u"filesystem.clear-pagefile-at-shutdown"_s,
            u"filesystem.pagefile-encryption"_s,
            u"filesystem.removable-disk-write-access"_s,
            u"filesystem.win32-long-paths"_s,
            u"network.administrative-shares"_s,
            u"network.elevated-mapped-drives"_s,
            u"network.dhcp-media-sense"_s,
            u"network.domain-name-devolution"_s,
            u"network.netbios-name-resolution"_s,
            u"network.outgoing-ntlm"_s,
            u"network.ip-routing"_s,
            u"network.remote-assistance"_s,
            u"network.sharing-wizard"_s,
            u"network.windows-connect-now"_s,
            u"network.winrm-basic-auth"_s,
            u"network.winrm-remote-shell"_s,
            u"gaming.game-recording"_s,
            u"power.hibernation"_s,
            u"privacy.activity-feed"_s,
            u"privacy.advertising-id-block"_s,
            u"privacy.feedback-prompts"_s,
            u"privacy.diagnostic-data-level"_s,
            u"privacy.first-logon-privacy-screen"_s,
            u"privacy.online-speech-recognition"_s,
            u"privacy.experimentation"_s,
            u"privacy.publish-user-activities"_s,
            u"privacy.tailored-experiences"_s,
            u"privacy.upload-user-activities"_s,
            u"privacy.app-access-account-info"_s,
            u"privacy.app-access-background-spatial-perception"_s,
            u"privacy.app-access-calendar"_s,
            u"privacy.app-access-call-history"_s,
            u"privacy.app-access-contacts"_s,
            u"privacy.app-access-diagnostic-info"_s,
            u"privacy.app-access-email"_s,
            u"privacy.app-access-gaze-input"_s,
            u"privacy.app-access-generative-ai"_s,
            u"privacy.app-access-graphics-capture-programmatic"_s,
            u"privacy.app-access-graphics-capture-without-border"_s,
            u"privacy.app-access-location"_s,
            u"privacy.app-access-messaging"_s,
            u"privacy.app-access-motion"_s,
            u"privacy.app-access-notifications"_s,
            u"privacy.app-access-phone"_s,
            u"privacy.app-access-radios"_s,
            u"privacy.app-access-system-ai-models"_s,
            u"privacy.app-access-tasks"_s,
            u"privacy.app-access-trusted-devices"_s,
            u"privacy.app-access-voice-activation"_s,
            u"privacy.app-access-voice-activation-above-lock"_s,
            u"privacy.app-sync-with-devices"_s,
            u"updates.exclude-driver-updates"_s,
            u"updates.automatic-update-mode"_s,
            u"updates.automatic-updates"_s,
            u"updates.delivery-optimization-mode"_s,
            u"updates.prevent-auto-reboot-signed-in"_s,
        };
        for (const auto& expectedId : expectedIds) {
            const auto id = TweakId::parse(expectedId);
            QVERIFY2(id.has_value(), qPrintable(expectedId));
            QVERIFY2(result.catalog->find(*id) != nullptr, qPrintable(expectedId));
        }

        const auto aeroShakeId = *TweakId::parse(u"behavior.disable-aero-shake");
        const auto* aeroShake = result.catalog->find(aeroShakeId);
        QVERIFY(aeroShake != nullptr);
        QCOMPARE(aeroShake->detection->location.hive, RegistryHive::CurrentUser);
        QCOMPARE(aeroShake->detection->location.key,
                 u"Software\\Policies\\Microsoft\\Windows\\Explorer"_s);
        QCOMPARE(aeroShake->detection->location.valueName, u"NoWindowMinimizingShortcuts"_s);

        const auto removableWriteId = *TweakId::parse(u"filesystem.removable-disk-write-access");
        const auto* removableWrite = result.catalog->find(removableWriteId);
        QVERIFY(removableWrite != nullptr);
        QCOMPARE(removableWrite->detection->location.hive, RegistryHive::LocalMachine);
        QCOMPARE(removableWrite->detection->location.key,
                 u"Software\\Policies\\Microsoft\\Windows\\RemovableStorageDevices\\{53f5630d-b6bf-11d0-94f2-00a0c91efb8b}"_s);
        QCOMPARE(removableWrite->detection->location.valueName, u"Deny_Write"_s);

        const auto inkId = *TweakId::parse(u"apps.windows-ink-workspace");
        const auto* ink = result.catalog->find(inkId);
        QVERIFY(ink != nullptr);
        QCOMPARE(ink->detection->statesByValue.value(0), u"disabled"_s);
        QCOMPARE(ink->detection->statesByValue.value(1), u"enabled_after_sign_in"_s);
        QCOMPARE(ink->detection->statesByValue.value(2), u"enabled"_s);
    }
};

QTEST_APPLESS_MAIN(TweakCatalogLoaderTest)

#include "TweakCatalogLoaderTest.moc"
