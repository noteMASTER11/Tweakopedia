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

    void loadsTypedFeatureStoreTweak()
    {
        const auto result = loadSingleFixture(u"valid/feature-end-task.yaml"_s);

        QVERIFY2(result.errors.isEmpty(), qPrintable(formatErrors(result)));
        QVERIFY(result.catalog.has_value());
        const auto* tweak = result.catalog->find(*TweakId::parse(u"experimental.end-task"_s));
        QVERIFY(tweak != nullptr);
        QVERIFY(tweak->featureDetection.has_value());
        QCOMPARE(tweak->featureDetection->featureId, 42592269U);
        const auto* operation = std::get_if<SetFeatureStateOperation>(
            &tweak->states.at(2).operations.first());
        QVERIFY(operation != nullptr);
        QCOMPARE(operation->featureId, 42592269U);
        QCOMPARE(operation->state, FeatureEnabledState::Enabled);
    }

    void loadsDependencyAndConflictIds()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        copyFixture(u"valid/win32-long-paths.yaml"_s, directory.path());
        copyFixture(u"valid/related-tweaks.yaml"_s, directory.path());

        const auto result = TweakCatalogLoader{}.loadDirectory(directory.path());

        QVERIFY2(result.errors.isEmpty(), qPrintable(formatErrors(result)));
        QVERIFY(result.catalog.has_value());
        const auto* tweak = result.catalog->find(*TweakId::parse(u"filesystem.related-test"_s));
        QVERIFY(tweak != nullptr);
        QCOMPARE(tweak->dependencies.size(), 1);
        QCOMPARE(tweak->dependencies.first().toString(), u"filesystem.win32-long-paths"_s);
        QCOMPARE(tweak->conflicts.size(), 1);
        QCOMPARE(tweak->conflicts.first().toString(), u"filesystem.win32-long-paths"_s);
    }

    void rejectsUnknownRelationId()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-relation.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"relation.unknown"));
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
        QCOMPARE(result.catalog->size(), 435);

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
            u"apps.paint-cocreator"_s,
            u"apps.paint-generative-fill"_s,
            u"apps.paint-image-creator"_s,
            u"apps.paint-generative-erase"_s,
            u"apps.paint-background-removal"_s,
            u"apps.click-to-do-user"_s,
            u"apps.click-to-do-device"_s,
            u"apps.windows-copilot-user"_s,
            u"apps.windows-copilot-device"_s,
            u"apps.recall-user-data-analysis"_s,
            u"apps.recall-device-data-analysis"_s,
            u"apps.recall-enablement"_s,
            u"apps.recall-snapshot-saving"_s,
            u"apps.edge-copilot-page-context"_s,
            u"apps.edge-copilot-page-data"_s,
            u"apps.edge-sidebar"_s,
            u"apps.edge-entra-copilot-context"_s,
            u"apps.edge-history-ai-search"_s,
            u"apps.edge-inline-compose"_s,
            u"apps.edge-local-ai-model"_s,
            u"apps.edge-new-tab-copilot"_s,
            u"apps.brave-vpn"_s,
            u"apps.brave-wallet"_s,
            u"apps.brave-leo"_s,
            u"apps.brave-rewards"_s,
            u"apps.brave-talk"_s,
            u"apps.brave-news"_s,
            u"apps.chrome-ai-suggestions"_s,
            u"apps.chrome-ai-history-search"_s,
            u"apps.chrome-local-ai-model"_s,
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
            u"experimental.end-task"_s,
            u"experimental.file-explorer-gallery"_s,
            u"experimental.create-archive-wizard"_s,
            u"experimental.sudo"_s,
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
            u"behavior.snap-bar"_s,
            u"behavior.snap-layouts-maximize"_s,
            u"boot.hide-last-user-name"_s,
            u"boot.last-logon-info"_s,
            u"boot.lock-screen"_s,
            u"boot.login-network-icon"_s,
            u"boot.login-power-button"_s,
            u"boot.lock-screen-camera"_s,
            u"boot.password-reveal-button"_s,
            u"boot.require-ctrl-alt-delete"_s,
            u"boot.verbose-logon-messages"_s,
            u"boot.lock-screen-content-suggestions"_s,
            u"boot.lock-screen-spotlight-overlay"_s,
            u"desktop.account-notifications"_s,
            u"desktop.copilot-button"_s,
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
            u"gaming.game-dvr-user"_s,
            u"gaming.app-capture-user"_s,
            u"power.hibernation"_s,
            u"power.modern-standby-network-ac"_s,
            u"power.modern-standby-network-battery"_s,
            u"privacy.activity-feed"_s,
            u"privacy.advertising-id-block"_s,
            u"privacy.feedback-prompts"_s,
            u"privacy.diagnostic-data-level"_s,
            u"privacy.first-logon-privacy-screen"_s,
            u"privacy.online-speech-recognition"_s,
            u"privacy.inking-typing-improvement"_s,
            u"privacy.implicit-ink-collection"_s,
            u"privacy.implicit-text-collection"_s,
            u"privacy.contact-harvesting"_s,
            u"privacy.input-personalization-consent"_s,
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
            u"apps.ai-service-auto-start"_s,
            u"apps.notepad-ai-features"_s,
            u"behavior.alt-tab-app-tabs"_s,
            u"behavior.snap-assist"_s,
            u"desktop.phone-link-start"_s,
            u"desktop.search-highlights"_s,
            u"desktop.share-drag-tray"_s,
            u"desktop.start-all-apps"_s,
            u"desktop.start-recommended"_s,
            u"desktop.taskbar-search-mode"_s,
            u"desktop.toast-notifications"_s,
            u"devices.device-companion-apps"_s,
            u"filesystem.bitlocker-auto-encryption"_s,
            u"filesystem.storage-sense"_s,
            u"power.fast-startup"_s,
            u"privacy.find-my-device"_s,
            u"privacy.location-services"_s,
            u"updates.early-feature-updates"_s,
            u"apps.chrome-ai-mode"_s,
            u"apps.chrome-autofill-predictions"_s,
            u"apps.chrome-ai-themes"_s,
            u"apps.chrome-devtools-ai"_s,
            u"apps.chrome-gemini-find-fill"_s,
            u"apps.chrome-gemini"_s,
            u"apps.chrome-gemini-spark"_s,
            u"apps.chrome-help-me-write"_s,
            u"apps.chrome-search-content-sharing"_s,
            u"apps.chrome-smart-tab-sharing"_s,
            u"apps.chrome-tab-compare"_s,
            u"apps.chrome-third-party-ai-chat"_s,
            u"apps.chrome-voice-typing"_s,
            u"apps.edge-new-tab-content"_s,
            u"apps.edge-default-top-sites"_s,
            u"apps.edge-shopping-assistant"_s,
            u"apps.edge-tab-organization"_s,
            u"apps.edge-alternate-error-pages"_s,
            u"apps.edge-user-feedback"_s,
            u"apps.edge-recommendations"_s,
            u"apps.edge-default-browser-prompt"_s,
            u"apps.edge-default-browser-campaign"_s,
            u"apps.edge-spotlight-recommendations"_s,
            u"apps.edge-acrobat-subscription-button"_s,
            u"apps.edge-startup-boost"_s,
            u"apps.edge-background-mode"_s,
            u"apps.edge-new-tab-quick-links"_s,
            u"apps.edge-address-bar-trending-suggestions"_s,
            u"desktop.welcome-experience-user"_s,
            u"desktop.start-suggestions-user"_s,
            u"desktop.system-pane-suggestions"_s,
            u"desktop.start-iris-recommendations"_s,
            u"desktop.tips-user"_s,
            u"desktop.soft-landing-user"_s,
            u"desktop.settings-suggestions-338393"_s,
            u"desktop.settings-suggestions-353694"_s,
            u"desktop.settings-suggestions-353696"_s,
            u"desktop.settings-suggestions-353698"_s,
            u"desktop.settings-account-notifications"_s,
            u"desktop.setup-completion-suggestions"_s,
            u"desktop.suggested-service-notifications"_s,
            u"desktop.phone-link-suggestions"_s,
            u"desktop.start-account-notifications-user"_s,
            u"desktop.windows-backup-reminders"_s,
            u"privacy.advertising-id-user"_s,
            u"privacy.tailored-experiences-user"_s,
            u"privacy.online-speech-user"_s,
            u"apps.edge-personalization-reporting"_s,
            u"apps.edge-diagnostic-data"_s,
            u"desktop.search-web-suggestions-user"_s,
            u"desktop.cortana-consent"_s,
            u"desktop.chat-taskbar"_s,
            u"desktop.meet-now-taskbar"_s,
            u"desktop.desktop-spotlight-collection"_s,
            u"gaming.controller-game-bar"_s,
            u"desktop.device-search-history-user"_s,
            u"desktop.taskbar-end-task-switch"_s,
            u"desktop.desktop-spotlight-icon"_s,
            u"desktop.start-all-apps-view-mode"_s,
            u"desktop.explorer-gallery-navigation"_s,
            u"desktop.explorer-home-navigation"_s,
            u"desktop.onedrive-navigation"_s,
            u"desktop.explorer-hub-mode"_s,
            u"desktop.taskbar-people"_s,
            u"desktop.thumbnail-cache"_s,
            u"desktop.network-thumbnail-cache"_s,
            u"desktop.recent-documents-tracking"_s,
            u"behavior.autoplay-user"_s,
            u"desktop.file-copy-details"_s,
            u"desktop.auto-tray-icons"_s,
            u"desktop.classic-bing-search"_s,
            u"desktop.classic-cortana-search"_s,
            u"desktop.lock-screen-rotating-images-user"_s,
            u"desktop.content-delivery-master-user"_s,
            u"desktop.feature-management-content-user"_s,
            u"desktop.oem-preinstalled-apps-user"_s,
            u"desktop.preinstalled-apps-user"_s,
            u"desktop.silent-app-installation-user"_s,
            u"desktop.subscribed-content-master-user"_s,
            u"desktop.windows-spotlight-action-center"_s,
            u"desktop.windows-spotlight-settings"_s,
            u"desktop.organizational-messages"_s,
            u"privacy.device-name-diagnostic-data"_s,
            u"privacy.telemetry-opt-in-change-notification"_s,
            u"privacy.diagnostic-settings-page"_s,
            u"privacy.diagnostic-data-viewer"_s,
            u"privacy.onesettings-downloads"_s,
            u"privacy.onesettings-auditing"_s,
            u"behavior.application-impact-telemetry"_s,
            u"behavior.program-compatibility-assistant"_s,
            u"behavior.user-action-recorder"_s,
            u"behavior.16-bit-applications"_s,
            u"devices.driver-downloads-metered"_s,
            u"updates.driver-search-order"_s,
            u"updates.metered-downloads"_s,
            u"updates.pause-access"_s,
            u"updates.safeguard-holds"_s,
            u"updates.internet-locations"_s,
            u"updates.restart-notifications"_s,
            u"updates.wake-for-installation"_s,
            u"updates.settings-access"_s,
            u"network.insecure-guest-logons"_s,
            u"gaming.hardware-accelerated-gpu-scheduling"_s,
            u"gaming.game-mode-auto"_s,
            u"gaming.game-mode-enabled"_s,
            u"network.multimedia-network-throttling"_s,
            u"gaming.game-task-priority"_s,
            u"network.rdp-hardware-gpu"_s,
            u"network.rdp-avc-hardware-encoding"_s,
            u"network.rdp-wddm-driver"_s,
            u"network.rdp-client-hardware-mode"_s,
            u"network.rdp-avc444-mode"_s,
            u"apps.defender-network-protection"_s,
            u"apps.defender-controlled-folder-access"_s,
            u"apps.defender-check-signatures-before-scan"_s,
            u"apps.defender-mapped-drive-full-scan"_s,
            u"apps.defender-catchup-full-scan"_s,
            u"apps.defender-catchup-quick-scan"_s,
            u"apps.defender-cpu-throttle-idle-scans"_s,
            u"apps.defender-scan-only-if-idle"_s,
            u"apps.defender-restore-point"_s,
            u"apps.defender-randomize-scheduled-tasks"_s,
            u"apps.defender-user-interface"_s,
            u"apps.defender-intrusion-prevention"_s,
            u"apps.defender-on-access-protection"_s,
            u"apps.defender-block-at-first-sight"_s,
            u"apps.smartscreen-shell"_s,
            u"apps.edge-smartscreen"_s,
            u"apps.edge-smartscreen-override"_s,
            u"apps.edge-smartscreen-file-override"_s,
            u"apps.edge-smartscreen-trusted-downloads"_s,
            u"apps.edge-smartscreen-pua"_s,
            u"devices.install-restore-point"_s,
            u"devices.install-admin-override"_s,
            u"devices.install-removable"_s,
            u"devices.install-unspecified"_s,
            u"devices.install-layered-evaluation"_s,
            u"devices.install-driver-ranking"_s,
            u"filesystem.removable-disk-read-access"_s,
            u"filesystem.removable-disk-execute-access"_s,
            u"filesystem.removable-storage-all-access"_s,
            u"filesystem.cd-dvd-read-access"_s,
            u"filesystem.cd-dvd-write-access"_s,
            u"filesystem.cd-dvd-execute-access"_s,
            u"filesystem.removable-disk-read-access-user"_s,
            u"filesystem.removable-disk-write-access-user"_s,
            u"filesystem.removable-disk-execute-access-user"_s,
            u"filesystem.cd-dvd-read-access-user"_s,
            u"filesystem.removable-storage-remote-session"_s,
            u"desktop.explorer-delete-confirmation"_s,
            u"desktop.windows-hotkeys"_s,
            u"desktop.explorer-folder-options"_s,
            u"desktop.explorer-security-tab"_s,
            u"desktop.explorer-context-menus"_s,
            u"desktop.explorer-manage-computer"_s,
            u"desktop.explorer-cd-burning"_s,
            u"desktop.explorer-common-dialog-history"_s,
            u"desktop.explorer-network-actions"_s,
            u"desktop.explorer-recycle-bin"_s,
            u"desktop.explorer-approved-shell-extensions"_s,
            u"desktop.explorer-hardware-tab"_s,
            u"power.power-throttling"_s,
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

        const auto searchModeId = *TweakId::parse(u"desktop.taskbar-search-mode");
        const auto* searchMode = result.catalog->find(searchModeId);
        QVERIFY(searchMode != nullptr);
        QCOMPARE(searchMode->states.size(), 4);
        QCOMPARE(searchMode->detection->statesByValue.value(0), u"hidden"_s);
        QCOMPARE(searchMode->detection->statesByValue.value(1), u"icon"_s);
        QCOMPARE(searchMode->detection->statesByValue.value(2), u"box"_s);
        QCOMPARE(searchMode->detection->statesByValue.value(3), u"icon_and_label"_s);

        const auto altTabId = *TweakId::parse(u"behavior.alt-tab-app-tabs");
        const auto* altTab = result.catalog->find(altTabId);
        QVERIFY(altTab != nullptr);
        QCOMPARE(altTab->states.size(), 4);
        QCOMPARE(altTab->detection->statesByValue.value(3), u"none"_s);
        QCOMPARE(altTab->detection->statesByValue.value(2), u"three"_s);
        QCOMPARE(altTab->detection->statesByValue.value(1), u"five"_s);
        QCOMPARE(altTab->detection->statesByValue.value(0), u"twenty"_s);

        const auto chromeHistoryId = *TweakId::parse(u"apps.chrome-ai-history-search");
        const auto* chromeHistory = result.catalog->find(chromeHistoryId);
        QVERIFY(chromeHistory != nullptr);
        QCOMPARE(chromeHistory->states.size(), 3);
        QCOMPARE(chromeHistory->detection->statesByValue.value(0),
                 u"enabled_model_improvement"_s);
        QCOMPARE(chromeHistory->detection->statesByValue.value(1), u"enabled_private"_s);
        QCOMPARE(chromeHistory->detection->statesByValue.value(2), u"disabled"_s);

        const auto chromeThemesId = *TweakId::parse(u"apps.chrome-ai-themes"_s);
        const auto* chromeThemes = result.catalog->find(chromeThemesId);
        QVERIFY(chromeThemes != nullptr);
        QCOMPARE(chromeThemes->states.size(), 3);
        QCOMPARE(chromeThemes->detection->statesByValue.value(0),
                 u"enabled_model_improvement"_s);
        QCOMPARE(chromeThemes->detection->statesByValue.value(1), u"enabled_private"_s);
        QCOMPARE(chromeThemes->detection->statesByValue.value(2), u"disabled"_s);

        const auto topSitesId = *TweakId::parse(u"apps.edge-default-top-sites"_s);
        const auto* topSites = result.catalog->find(topSitesId);
        QVERIFY(topSites != nullptr);
        QCOMPARE(topSites->detection->statesByValue.value(0), u"enabled"_s);
        QCOMPARE(topSites->detection->statesByValue.value(1), u"disabled"_s);

        const auto meetNowId = *TweakId::parse(u"desktop.meet-now-taskbar"_s);
        const auto* meetNow = result.catalog->find(meetNowId);
        QVERIFY(meetNow != nullptr);
        QCOMPARE(meetNow->compatibility.operatingSystems,
                 QVector{WindowsFamily::Windows10});
        QVERIFY(meetNow->compatibility.maximumBuild.has_value());
        QCOMPARE(*meetNow->compatibility.maximumBuild, 19045U);

        const auto chatId = *TweakId::parse(u"desktop.chat-taskbar"_s);
        const auto* chat = result.catalog->find(chatId);
        QVERIFY(chat != nullptr);
        QCOMPARE(chat->compatibility.operatingSystems,
                 QVector{WindowsFamily::Windows11});
        QCOMPARE(chat->compatibility.minimumBuild, 22000U);

        const auto allAppsViewId = *TweakId::parse(u"desktop.start-all-apps-view-mode"_s);
        const auto* allAppsView = result.catalog->find(allAppsViewId);
        QVERIFY(allAppsView != nullptr);
        QCOMPARE(allAppsView->states.size(), 3);
        QCOMPARE(allAppsView->detection->statesByValue.value(0), u"category"_s);
        QCOMPARE(allAppsView->detection->statesByValue.value(1), u"grid"_s);
        QCOMPARE(allAppsView->detection->statesByValue.value(2), u"list"_s);

        const auto driverSearchId = *TweakId::parse(u"updates.driver-search-order"_s);
        const auto* driverSearch = result.catalog->find(driverSearchId);
        QVERIFY(driverSearch != nullptr);
        QCOMPARE(driverSearch->states.size(), 3);
        QCOMPARE(driverSearch->detection->statesByValue.value(0), u"never"_s);
        QCOMPARE(driverSearch->detection->statesByValue.value(1), u"always"_s);
        QCOMPARE(driverSearch->detection->statesByValue.value(2), u"when_needed"_s);

        const auto throttlingId = *TweakId::parse(u"network.multimedia-network-throttling"_s);
        const auto* throttling = result.catalog->find(throttlingId);
        QVERIFY(throttling != nullptr);
        QCOMPARE(throttling->detection->statesByValue.value(10), u"enabled"_s);
        QCOMPARE(throttling->detection->statesByValue.value(4294967295U), u"disabled"_s);

        const auto rdpHardwareId = *TweakId::parse(u"network.rdp-client-hardware-mode"_s);
        const auto* rdpHardware = result.catalog->find(rdpHardwareId);
        QVERIFY(rdpHardware != nullptr);
        QCOMPARE(rdpHardware->detection->statesByValue.value(0), u"disabled"_s);
        QCOMPARE(rdpHardware->detection->statesByValue.value(1), u"enabled"_s);

        const auto orgMessagesId = *TweakId::parse(u"desktop.organizational-messages"_s);
        const auto* orgMessages = result.catalog->find(orgMessagesId);
        QVERIFY(orgMessages != nullptr);
        QCOMPARE(orgMessages->compatibility.minimumBuild, 19041U);

        const auto networkProtectionId = *TweakId::parse(u"apps.defender-network-protection"_s);
        const auto* networkProtection = result.catalog->find(networkProtectionId);
        QVERIFY(networkProtection != nullptr);
        QCOMPARE(networkProtection->states.size(), 3);
        QCOMPARE(networkProtection->detection->statesByValue.value(0), u"disabled"_s);
        QCOMPARE(networkProtection->detection->statesByValue.value(1), u"block"_s);
        QCOMPARE(networkProtection->detection->statesByValue.value(2), u"audit"_s);

        const auto edgeSmartScreenId = *TweakId::parse(u"apps.edge-smartscreen"_s);
        const auto* edgeSmartScreen = result.catalog->find(edgeSmartScreenId);
        QVERIFY(edgeSmartScreen != nullptr);
        QCOMPARE(edgeSmartScreen->compatibility.requiredComponents,
                 QVector{u"edge"_s});

        const auto removableUserId = *TweakId::parse(u"filesystem.removable-disk-read-access-user"_s);
        const auto* removableUser = result.catalog->find(removableUserId);
        QVERIFY(removableUser != nullptr);
        QCOMPARE(removableUser->detection->location.hive, RegistryHive::CurrentUser);
        QCOMPARE(removableUser->detection->location.valueName, u"Deny_Read"_s);

        const auto driverRankingId = *TweakId::parse(u"devices.install-driver-ranking"_s);
        const auto* driverRanking = result.catalog->find(driverRankingId);
        QVERIFY(driverRanking != nullptr);
        QCOMPARE(driverRanking->detection->location.valueName, u"AllSigningEqual"_s);
        QCOMPARE(driverRanking->detection->statesByValue.value(0), u"ranked"_s);
        QCOMPARE(driverRanking->detection->statesByValue.value(1), u"equal"_s);

        const auto throttlingPolicyId = *TweakId::parse(u"power.power-throttling"_s);
        const auto* throttlingPolicy = result.catalog->find(throttlingPolicyId);
        QVERIFY(throttlingPolicy != nullptr);
        QCOMPARE(throttlingPolicy->detection->statesByValue.value(0), u"enabled"_s);
        QCOMPARE(throttlingPolicy->detection->statesByValue.value(1), u"disabled"_s);
    }
};

QTEST_APPLESS_MAIN(TweakCatalogLoaderTest)

#include "TweakCatalogLoaderTest.moc"
