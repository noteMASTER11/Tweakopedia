#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>

#include <optional>

namespace tweakopedia::persistence {

enum class TransactionStatus {
    Pending,
    Running,
    Succeeded,
    Failed,
    RolledBack,
    Interrupted,
};

inline QString transactionStatusName(TransactionStatus status)
{
    switch (status) {
    case TransactionStatus::Pending: return QStringLiteral("pending");
    case TransactionStatus::Running: return QStringLiteral("running");
    case TransactionStatus::Succeeded: return QStringLiteral("succeeded");
    case TransactionStatus::Failed: return QStringLiteral("failed");
    case TransactionStatus::RolledBack: return QStringLiteral("rolled_back");
    case TransactionStatus::Interrupted: return QStringLiteral("interrupted");
    }
    return {};
}

inline std::optional<TransactionStatus> transactionStatusFromName(QStringView name)
{
    if (name == u"pending") return TransactionStatus::Pending;
    if (name == u"running") return TransactionStatus::Running;
    if (name == u"succeeded") return TransactionStatus::Succeeded;
    if (name == u"failed") return TransactionStatus::Failed;
    if (name == u"rolled_back") return TransactionStatus::RolledBack;
    if (name == u"interrupted") return TransactionStatus::Interrupted;
    return std::nullopt;
}

struct TransactionRecord {
    QUuid id;
    QString packageName;
    TransactionStatus status{TransactionStatus::Pending};
    QDateTime createdAtUtc;
    QDateTime updatedAtUtc;
    QString directory;
    QString error;

    [[nodiscard]] static TransactionRecord pending(
        QUuid id,
        QString packageName,
        QString directory)
    {
        const auto now = QDateTime::currentDateTimeUtc();
        return {
            .id = id,
            .packageName = std::move(packageName),
            .status = TransactionStatus::Pending,
            .createdAtUtc = now,
            .updatedAtUtc = now,
            .directory = std::move(directory),
        };
    }
};

} // namespace tweakopedia::persistence
