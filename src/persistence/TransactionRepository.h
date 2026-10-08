#pragma once

#include "persistence/Database.h"
#include "persistence/TransactionRecord.h"

#include <optional>

namespace tweakopedia::persistence {

class TransactionRepository final
{
public:
    explicit TransactionRepository(Database& database);

    [[nodiscard]] bool insert(const TransactionRecord& record);
    [[nodiscard]] bool updateStatus(
        const QUuid& id,
        TransactionStatus status,
        const QString& error = {});
    [[nodiscard]] std::optional<TransactionRecord> find(const QUuid& id) const;
    [[nodiscard]] QString lastError() const;

private:
    Database* database_{};
    mutable QString lastError_;
};

} // namespace tweakopedia::persistence
