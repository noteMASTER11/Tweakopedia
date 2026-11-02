import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../apps/tweakopedia/qml/pages"
import "../../apps/tweakopedia/qml/style"

ApplicationWindow {
    id: window

    width: 1240
    height: 900
    visible: true
    color: FluentTheme.canvas
    title: "Tweakopedia telemetry preview"

    QtObject {
        id: fakeController

        function telemetryProfile(profileId) {
            return {
                profileId: profileId,
                entries: [
                    {
                        id: "privacy.diagnostic-data-level",
                        title: "Уровень диагностических данных Windows",
                        summary: "Определяет объём сведений о работе Windows, отправляемых Microsoft.",
                        group: "Диагностические данные",
                        currentTitle: "Обязательные и дополнительные",
                        targetTitle: profileId === "strict" ? "Отключено" : "Только обязательные",
                        status: "change"
                    },
                    {
                        id: "privacy.advertising-id-block",
                        title: "Рекламный идентификатор",
                        summary: "Запрещает приложениям использовать общий рекламный профиль.",
                        group: "Персонализация",
                        currentTitle: "Разрешён",
                        targetTitle: "Запрещён",
                        status: "change"
                    }
                ],
                totalCount: 2,
                configuredCount: 0,
                changeCount: 2,
                unsupportedCount: 0,
                queuedCount: 0
            }
        }

        function queueTelemetryProfile(profileId) { return 2 }
    }

    TelemetryToolPage {
        anchors.fill: parent
        controller: fakeController
    }
}
