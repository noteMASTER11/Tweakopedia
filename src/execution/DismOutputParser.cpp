#include "execution/DismOutputParser.h"

#include <QRegularExpression>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {

void DismOutputParser::reset(SystemToolStage stage)
{
    stage_ = stage;
    recent_.clear();
    progress_ = -1;
    health_ = ComponentStoreHealth::Unknown;
}

void DismOutputParser::consume(QByteArrayView chunk)
{
    recent_.append(chunk.data(), chunk.size());
    if (recent_.size() > 8192) recent_.remove(0, recent_.size() - 8192);

    const auto text = QString::fromLatin1(recent_);
    static const QRegularExpression percentage(u"(\\d{1,3}(?:\\.\\d+)?)%"_s);
    auto matches = percentage.globalMatch(text);
    while (matches.hasNext()) {
        bool ok{};
        const auto value = matches.next().captured(1).toDouble(&ok);
        if (ok && value >= 0.0 && value <= 100.0) {
            progress_ = std::max(progress_, static_cast<int>(value));
        }
    }

    if (stage_ != SystemToolStage::CheckHealth
        && stage_ != SystemToolStage::VerifyRestore) {
        return;
    }
    if (recent_.contains("No component store corruption detected.")) {
        health_ = ComponentStoreHealth::Healthy;
    } else if (recent_.contains("The component store cannot be repaired.")) {
        health_ = ComponentStoreHealth::NonRepairable;
    } else if (recent_.contains("The component store is repairable.")) {
        health_ = ComponentStoreHealth::Repairable;
    }
}

int DismOutputParser::progress() const noexcept
{
    return progress_;
}

ComponentStoreHealth DismOutputParser::health() const noexcept
{
    return health_;
}

} // namespace tweakopedia::execution
