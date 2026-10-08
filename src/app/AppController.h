#pragma once

#include "app/HistoryListModel.h"
#include "app/CategoryListModel.h"
#include "app/QueueListModel.h"
#include "app/TweakListModel.h"
#include "app/TweakFilterProxyModel.h"
#include "content/CatalogError.h"
#include "content/CategoryCatalogLoader.h"
#include "content/TweakCatalogLoader.h"
#include "planning/ExecutionPlan.h"
#include "domain/SystemOverview.h"

#include <QFutureWatcher>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace tweakopedia::app {

enum class AppOperationStatus { Succeeded, Cancelled, Failed };

struct AppOperationResult {
    AppOperationStatus status{AppOperationStatus::Failed};
    QString code;
    QString message;
    QUuid transactionId;
};

using ProgressCallback = std::function<void(int, const QString&)>;

class IAppServices
{
public:
    virtual ~IAppServices() = default;
    [[nodiscard]] virtual content::CatalogLoadResult loadCatalog() = 0;
    [[nodiscard]] virtual content::CatalogLoadResult loadAppRemovalCatalog() = 0;
    [[nodiscard]] virtual content::CategoryCatalogLoadResult loadCategories() = 0;
    [[nodiscard]] virtual domain::SystemProfile currentProfile() const = 0;
    [[nodiscard]] virtual domain::SystemOverviewSnapshot systemOverview() const = 0;
    [[nodiscard]] virtual domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const domain::SystemProfile& profile) const = 0;
    [[nodiscard]] virtual AppOperationResult apply(
        const planning::ExecutionPlan& plan,
        const QString& packageName,
        const ProgressCallback& progress) = 0;
    [[nodiscard]] virtual AppOperationResult rollback(
        const QUuid& transactionId,
        const ProgressCallback& progress) = 0;
    [[nodiscard]] virtual bool restartComputer() = 0;
    [[nodiscard]] virtual QVector<persistence::TransactionRecord> history() const = 0;
};

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TweakListModel* tweaks READ tweaks CONSTANT)
    Q_PROPERTY(TweakFilterProxyModel* filteredTweaks READ filteredTweaks CONSTANT)
    Q_PROPERTY(CategoryListModel* categories READ categories CONSTANT)
    Q_PROPERTY(QueueListModel* queue READ queue CONSTANT)
    Q_PROPERTY(HistoryListModel* history READ history CONSTANT)
    Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY previewChanged)
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY previewChanged)
    Q_PROPERTY(QVariantList previewOperations READ previewOperations NOTIFY previewChanged)
    Q_PROPERTY(QString lastErrorCode READ lastErrorCode NOTIFY errorChanged)
    Q_PROPERTY(int applyProgress READ applyProgress NOTIFY operationChanged)
    Q_PROPERTY(QString applyStatus READ applyStatus NOTIFY operationChanged)
    Q_PROPERTY(QString applyMessage READ applyMessage NOTIFY operationChanged)
    Q_PROPERTY(bool rebootRequired READ rebootRequired NOTIFY operationChanged)
    Q_PROPERTY(QVariantMap systemOverview READ systemOverview NOTIFY systemOverviewChanged)
    Q_PROPERTY(bool systemOverviewLoading READ systemOverviewLoading NOTIFY systemOverviewChanged)
    Q_PROPERTY(QString systemOverviewError READ systemOverviewError NOTIFY systemOverviewChanged)
    Q_PROPERTY(QString appRemovalScanStatus READ appRemovalScanStatus NOTIFY appRemovalScanChanged)
    Q_PROPERTY(QString appRemovalScanError READ appRemovalScanError NOTIFY appRemovalScanChanged)

public:
    explicit AppController(IAppServices& services, QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] TweakListModel* tweaks() noexcept;
    [[nodiscard]] TweakFilterProxyModel* filteredTweaks() noexcept;
    [[nodiscard]] CategoryListModel* categories() noexcept;
    [[nodiscard]] QueueListModel* queue() noexcept;
    [[nodiscard]] HistoryListModel* history() noexcept;
    [[nodiscard]] QString previewSummary() const;
    [[nodiscard]] bool previewReady() const noexcept;
    [[nodiscard]] QVariantList previewOperations() const;
    [[nodiscard]] QString lastErrorCode() const;
    [[nodiscard]] int applyProgress() const noexcept;
    [[nodiscard]] QString applyStatus() const;
    [[nodiscard]] QString applyMessage() const;
    [[nodiscard]] bool rebootRequired() const noexcept;
    [[nodiscard]] QVariantMap systemOverview() const;
    [[nodiscard]] bool systemOverviewLoading() const noexcept;
    [[nodiscard]] QString systemOverviewError() const;
    [[nodiscard]] QString appRemovalScanStatus() const;
    [[nodiscard]] QString appRemovalScanError() const;

    Q_INVOKABLE bool startup();
    Q_INVOKABLE void setTweakSearch(const QString& query);
    Q_INVOKABLE void setTweakCategory(const QString& categoryId);
    Q_INVOKABLE bool selectTarget(const QString& id, const QString& state);
    Q_INVOKABLE bool removeFromQueue(const QString& id);
    Q_INVOKABLE QVariantMap openExplanation(const QString& id) const;
    Q_INVOKABLE bool buildPreview();
    Q_INVOKABLE bool applyQueue(const QString& packageName);
    Q_INVOKABLE bool rollback(const QString& transactionId);
    Q_INVOKABLE bool restartComputer();
    Q_INVOKABLE void refreshSystemOverview();
    Q_INVOKABLE void scanInstalledApps();

signals:
    void previewChanged();
    void errorChanged();
    void operationChanged();
    void systemOverviewChanged();
    void appRemovalScanChanged();

private:
    void refreshDetectedStates();
    void refreshModels();
    void resetOperationState();
    void setError(QString code, QString message = {});

    IAppServices* services_{};
    content::TweakCatalog catalog_;
    content::CategoryCatalog categoryCatalog_;
    domain::SystemProfile profile_;
    QHash<domain::TweakId, domain::DetectedState> detected_;
    QHash<domain::TweakId, bool> supported_;
    planning::TweakQueue queueData_;
    std::optional<planning::ExecutionPlan> preview_;
    TweakListModel tweaksModel_;
    TweakFilterProxyModel filteredTweaksModel_;
    CategoryListModel categoriesModel_;
    QueueListModel queueModel_;
    HistoryListModel historyModel_;
    QString lastErrorCode_;
    QString lastErrorMessage_;
    int applyProgress_{};
    QString applyStatus_{QStringLiteral("idle")};
    QString applyMessage_;
    bool rebootRequired_{};
    QFutureWatcher<domain::SystemOverviewSnapshot> systemOverviewWatcher_;
    QFutureWatcher<content::CatalogLoadResult> appRemovalScanWatcher_;
    QVariantMap systemOverview_;
    QString systemOverviewError_;
    bool systemOverviewLoading_{};
    QString appRemovalScanStatus_{QStringLiteral("idle")};
    QString appRemovalScanError_;
};

} // namespace tweakopedia::app
