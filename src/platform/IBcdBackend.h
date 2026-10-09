#pragma once

#include "domain/BcdElement.h"

#include <QString>

namespace tweakopedia::platform {

struct BcdReadResult {
    bool success{};
    bool missing{true};
    std::optional<domain::BcdValue> value;
    QString error;

    static BcdReadResult present(domain::BcdValue value)
    {
        return {.success = true, .missing = false, .value = std::move(value)};
    }
    static BcdReadResult missingElement() { return {}; }
    static BcdReadResult failed(QString error)
    {
        return {.success = false, .missing = false, .error = std::move(error)};
    }
};

struct BcdMutationResult {
    bool success{};
    QString error;
};

class IBcdBackend
{
public:
    virtual ~IBcdBackend() = default;
    virtual BcdReadResult read(const domain::BcdElementSpec& spec) const = 0;
    virtual BcdMutationResult set(
        const domain::BcdElementSpec& spec, const domain::BcdValue& value) = 0;
    virtual BcdMutationResult remove(const domain::BcdElementSpec& spec) = 0;
};

} // namespace tweakopedia::platform
