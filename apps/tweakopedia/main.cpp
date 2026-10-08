#include "app/AppController.h"
#include "content/TweakCatalogLoader.h"
#include "detection/RegistryDwordStateDetector.h"
#include "persistence/AppPaths.h"
#include "platform/WindowsRegistryBackend.h"
#include "platform/WindowsSystemProfileProvider.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class DesktopServices final : public app::IAppServices
{
public:
    DesktopServices()
        : paths_(QCoreApplication::applicationDirPath())
    {
    }

    content::CatalogLoadResult loadCatalog() override
    {
        const auto portable = paths_.contentRoot() + u"/tweaks"_s;
        const auto root = QFileInfo::exists(portable)
            ? portable
            : QStringLiteral(TWEAKOPEDIA_SOURCE_CONTENT_ROOT);
        return content::TweakCatalogLoader{}.loadDirectory(root);
    }

    domain::SystemProfile currentProfile() const override { return profile_.current(); }

    domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const domain::SystemProfile& profile) const override
    {
        return detection::RegistryDwordStateDetector{}.detect(tweak, registry_, profile);
    }

    app::AppOperationResult apply(const planning::ExecutionPlan&, const QString&) override
    {
        return {.status = app::AppOperationStatus::Failed,
                .code = u"workflow.not_connected"_s,
                .message = u"Применение будет подключено на следующем этапе."_s};
    }

    app::AppOperationResult rollback(const QUuid&) override
    {
        return {.status = app::AppOperationStatus::Failed,
                .code = u"workflow.not_connected"_s,
                .message = u"Возврат будет подключён на следующем этапе."_s};
    }

    QVector<persistence::TransactionRecord> history() const override { return {}; }

private:
    persistence::AppPaths paths_;
    platform::WindowsSystemProfileProvider profile_;
    mutable platform::WindowsRegistryBackend registry_;
};

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"Tweakopedia"_s);
    QCoreApplication::setOrganizationName(u"Tweakopedia"_s);

    DesktopServices services;
    app::AppController controller(services);
    (void)controller.startup();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(u"appController"_s, &controller);
    engine.load(QUrl(u"qrc:/qml/Main.qml"_s));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
