#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include <optional>

namespace tweakopedia::domain {

class TweakId final
{
public:
    TweakId() = default;

    [[nodiscard]] static std::optional<TweakId> parse(QStringView candidate)
    {
        static const QRegularExpression pattern(
            QStringLiteral("^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?(?:\\.[a-z0-9](?:[a-z0-9-]*[a-z0-9])?)*$"));

        const auto value = candidate.toString();
        if (!pattern.match(value).hasMatch()) {
            return std::nullopt;
        }
        return TweakId(value);
    }

    [[nodiscard]] bool isValid() const noexcept
    {
        return !value_.isEmpty();
    }

    [[nodiscard]] const QString& toString() const noexcept
    {
        return value_;
    }

    friend bool operator==(const TweakId&, const TweakId&) = default;

private:
    explicit TweakId(QString value)
        : value_(std::move(value))
    {
    }

    QString value_;
};

inline size_t qHash(const TweakId& id, size_t seed = 0) noexcept
{
    return qHash(id.toString(), seed);
}

} // namespace tweakopedia::domain
