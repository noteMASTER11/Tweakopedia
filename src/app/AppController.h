#pragma once

#include "app/HistoryListModel.h"
#include "app/QueueListModel.h"
#include "app/TweakListModel.h"
#include "content/CatalogError.h"
#include "content/TweakCatalogLoader.h"
#include "planning/ExecutionPlan.h"

#include <QObject>
#include <QVariantMap>

#include <optional>

namespace tweakopedia::app {

enum class AppOperationStatus { Succeeded, Cancelled, Failed };

struct AppOperationResult {
    AppOperationStatus status{AppOperationStatus::Failed};
    QString code;
    QString message;
    QUuid transactionId;
};

class IAppServices
{
public:
    virtual ~IAppServices() = default;
    [[nodiscard]] virtual content::CatalogLoadResult loadCatalog() = 0;
    [[nodiscard]] virtual domain::SystemProfile currentProfile() const = 0;
    [[nodiscard]] virtual domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const domain::SystemProfile& profile) const = 0;
    [[nodiscard]] virtual AppOperationResult apply(
        const planning::ExecutionPlan& plan,
        const QString& packageName) = 0;
    [[nodiscard]] virtual AppOperationResult rollback(const QUuid& transactionId) = 0;
    [[nodiscard]] virtual QVector<persistence::TransactionRecord> history() const = 0;
};

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TweakListModel* tweaks READ tweaks CONSTANT)
    Q_PROPERTY(QueueListModel* queue READ queue CONSTANT)
    Q_PROPERTY(HistoryListModel* history READ history CONSTANT)
    Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY previewChanged)
    Q_PROPERTY(QString lastErrorCode READ lastErrorCode NOTIFY errorChanged)

public:
    explicit AppController(IAppServices& services, QObject* parent = nullptr);

    [[nodiscard]] TweakListModel* tweaks() noexcept;
    [[nodiscard]] QueueListModel* queue() noexcept;
    [[nodiscard]] HistoryListModel* history() noexcept;
    [[nodiscard]] QString previewSummary() const;
    [[nodiscard]] QString lastErrorCode() const;

    Q_INVOKABLE bool startup();
    Q_INVOKABLE bool selectTarget(const QString& id, const QString& state);
    Q_INVOKABLE bool removeFromQueue(const QString& id);
    Q_INVOKABLE QVariantMap openExplanation(const QString& id) const;
    Q_INVOKABLE bool buildPreview();
    Q_INVOKABLE bool applyQueue(const QString& packageName);
    Q_INVOKABLE bool rollback(const QString& transactionId);

signals:
    void previewChanged();
    void errorChanged();

private:
    void refreshDetectedStates();
    void refreshModels();
    void setError(QString code, QString message = {});

    IAppServices* services_{};
    content::TweakCatalog catalog_;
    domain::SystemProfile profile_;
    QHash<domain::TweakId, domain::DetectedState> detected_;
    QHash<domain::TweakId, bool> supported_;
    planning::TweakQueue queueData_;
    std::optional<planning::ExecutionPlan> preview_;
    TweakListModel tweaksModel_;
    QueueListModel queueModel_;
    HistoryListModel historyModel_;
    QString lastErrorCode_;
    QString lastErrorMessage_;
};

} // namespace tweakopedia::app
