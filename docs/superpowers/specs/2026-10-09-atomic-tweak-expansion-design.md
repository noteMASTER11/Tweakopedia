# Atomic Tweak Expansion Design

## Scope

Expand the catalog from 273 to 335 entries without changing the execution engine or QML. Add 62 independently controlled DWORD-backed settings that use the existing registry detection, queue, transaction and rollback path.

## Sources and acceptance rule

- Use Win11Debloat and Win-Debloat-Tools only as discovery material.
- Confirm browser policy names, value types and meanings against current Chromium/Chrome Enterprise or Microsoft Edge policy definitions.
- Confirm Windows policy semantics against current Microsoft documentation where it exists.
- Omit source entries that are deprecated, contradictory, require registry deletion for a faithful state, or need an operation type the engine does not support.
- Do not add source links or scholarly apparatus to user-facing articles.

## Catalog package

### Chrome — 13 entries

`apps.chrome-ai-mode`, `apps.chrome-autofill-predictions`, `apps.chrome-ai-themes`, `apps.chrome-devtools-ai`, `apps.chrome-gemini-find-fill`, `apps.chrome-gemini`, `apps.chrome-gemini-spark`, `apps.chrome-help-me-write`, `apps.chrome-search-content-sharing`, `apps.chrome-smart-tab-sharing`, `apps.chrome-tab-compare`, `apps.chrome-third-party-ai-chat`, `apps.chrome-voice-typing`.

### Microsoft Edge — 15 entries

`apps.edge-new-tab-content`, `apps.edge-default-top-sites`, `apps.edge-shopping-assistant`, `apps.edge-tab-organization`, `apps.edge-alternate-error-pages`, `apps.edge-user-feedback`, `apps.edge-recommendations`, `apps.edge-default-browser-prompt`, `apps.edge-default-browser-campaign`, `apps.edge-spotlight-recommendations`, `apps.edge-acrobat-subscription-button`, `apps.edge-startup-boost`, `apps.edge-background-mode`, `apps.edge-new-tab-quick-links`, `apps.edge-address-bar-trending-suggestions`.

### Windows suggestions — 16 entries

`desktop.welcome-experience-user`, `desktop.start-suggestions-user`, `desktop.system-pane-suggestions`, `desktop.start-iris-recommendations`, `desktop.tips-user`, `desktop.soft-landing-user`, `desktop.settings-suggestions-338393`, `desktop.settings-suggestions-353694`, `desktop.settings-suggestions-353696`, `desktop.settings-suggestions-353698`, `desktop.settings-account-notifications`, `desktop.setup-completion-suggestions`, `desktop.suggested-service-notifications`, `desktop.phone-link-suggestions`, `desktop.start-account-notifications-user`, `desktop.windows-backup-reminders`.

### User and diagnostic controls — 5 entries

`privacy.advertising-id-user`, `privacy.tailored-experiences-user`, `privacy.online-speech-user`, `apps.edge-personalization-reporting`, `apps.edge-diagnostic-data`.

### Shell and system controls — 13 entries

`desktop.search-web-suggestions-user`, `desktop.cortana-consent`, `desktop.chat-taskbar`, `desktop.meet-now-taskbar`, `desktop.desktop-spotlight-collection`, `gaming.controller-game-bar`, `desktop.device-search-history-user`, `desktop.taskbar-end-task-switch`, `desktop.desktop-spotlight-icon`, `desktop.start-all-apps-view-mode`, `desktop.explorer-gallery-navigation`, `desktop.explorer-home-navigation`, `desktop.onedrive-navigation`.

## Presentation

Every entry supplies a Russian summary and complete six-part article. Boolean settings use Fluent toggles; enum policies use the existing dropdown. Existing category and subcategory IDs are reused so the left navigation and Tweakopedia tree update automatically.

## Verification

- A content test requires exactly 335 catalog entries and all 62 new IDs.
- Representative tests verify a tri-state Chrome policy, an inverted DWORD, a Windows 10-only entry, a Windows 11-only entry and the three-state Start menu selector.
- Run all 47 CTest targets, build the release package and validate the single-file EXE layout.
