#pragma once

#include "domain/BcdElement.h"
#include "platform/IBcdBackend.h"

#include <QByteArray>
#include <QString>

namespace tweakopedia::execution {

struct BcdExecutionResult {
    bool success{};
    QString code;
    QString message;
};

struct BcdCaptureResult : BcdExecutionResult {
    domain::BcdElementSnapshot snapshot;
};

class BcdElementExecutor final
{
public:
    explicit BcdElementExecutor(platform::IBcdBackend& backend);
    [[nodiscard]] BcdCaptureResult capture(const domain::BcdElementSpec& spec) const;
    [[nodiscard]] BcdExecutionResult compareBefore(
        const domain::BcdElementSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] BcdExecutionResult apply(const domain::SetBcdElementOperation& operation) const;
    [[nodiscard]] BcdExecutionResult restore(const domain::BcdElementSnapshot& snapshot) const;
    [[nodiscard]] static QByteArray fingerprint(const domain::BcdElementSnapshot& snapshot);
private:
    platform::IBcdBackend* backend_{};
};

} // namespace tweakopedia::execution
