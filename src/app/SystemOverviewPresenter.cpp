#include "app/SystemOverviewPresenter.h"

#include <QLocale>
#include <QStringList>

#include <cmath>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString cleanNumber(double value, int maximumDecimals = 1)
{
    auto result = QLocale::c().toString(value, 'f', maximumDecimals);
    while (result.contains(u'.') && result.endsWith(u'0')) result.chop(1);
    if (result.endsWith(u'.')) result.chop(1);
    return result.replace(u'.', u',');
}

QString decimalSize(quint64 bytes)
{
    constexpr double terabyte = 1000.0 * 1000.0 * 1000.0 * 1000.0;
    constexpr double gigabyte = 1000.0 * 1000.0 * 1000.0;
    if (bytes >= static_cast<quint64>(terabyte)) {
        return cleanNumber(static_cast<double>(bytes) / terabyte) + u" ТБ"_s;
    }
    if (bytes >= static_cast<quint64>(gigabyte)) {
        return cleanNumber(static_cast<double>(bytes) / gigabyte) + u" ГБ"_s;
    }
    return cleanNumber(static_cast<double>(bytes) / (1000.0 * 1000.0)) + u" МБ"_s;
}

QString binaryGigabytes(quint64 bytes)
{
    constexpr double gibibyte = 1024.0 * 1024.0 * 1024.0;
    return cleanNumber(static_cast<double>(bytes) / gibibyte) + u" ГБ"_s;
}

QString uptimeText(quint64 seconds)
{
    const auto days = seconds / 86400;
    const auto hours = (seconds % 86400) / 3600;
    const auto minutes = (seconds % 3600) / 60;
    QStringList parts;
    if (days > 0) parts.append(QString::number(days) + u" д"_s);
    if (hours > 0 || days > 0) parts.append(QString::number(hours) + u" ч"_s);
    parts.append(QString::number(minutes) + u" мин"_s);
    return parts.join(u" "_s);
}

QString logoName(quint32 build)
{
    if (build >= 22000) return u"windows11"_s;
    if (build >= 10240) return u"windows10"_s;
    return u"windows"_s;
}

QPair<QString, QString> healthPresentation(domain::DiskHealth health)
{
    switch (health) {
    case domain::DiskHealth::Healthy: return {u"Исправен"_s, u"good"_s};
    case domain::DiskHealth::Warning: return {u"Требует внимания"_s, u"warning"_s};
    case domain::DiskHealth::Unhealthy: return {u"Обнаружена проблема"_s, u"bad"_s};
    case domain::DiskHealth::Unknown: return {u"Нет данных"_s, u"neutral"_s};
    }
    return {};
}

} // namespace

QVariantMap SystemOverviewPresenter::present(
    const domain::SystemOverviewSnapshot& snapshot)
{
    const auto clock = QLocale::c().toString(
        static_cast<double>(snapshot.processor.currentClockMHz) / 1000.0, 'f', 2)
        .replace(u'.', u',');
    const auto processorDetails = u"%1 ядер · %2 потока · %3 ГГц"_s
        .arg(snapshot.processor.coreCount)
        .arg(snapshot.processor.logicalProcessorCount)
        .arg(clock);

    const auto total = snapshot.memory.totalBytes;
    const auto available = qMin(snapshot.memory.availableBytes, total);
    const auto usedPercent = total == 0
        ? 0
        : qRound((static_cast<double>(total - available) / static_cast<double>(total)) * 100.0);
    QStringList memoryDetails;
    memoryDetails.append(binaryGigabytes(available) + u" доступно"_s);
    if (snapshot.memory.speedMTs > 0) {
        memoryDetails.append(QString::number(snapshot.memory.speedMTs) + u" MT/s"_s);
    }
    if (snapshot.memory.moduleCount > 0) {
        memoryDetails.append(QString::number(snapshot.memory.moduleCount) + u" модуля"_s);
    }

    QVariantList graphics;
    for (const auto& adapter : snapshot.graphics) {
        QStringList details;
        if (adapter.adapterRamBytes > 0) details.append(binaryGigabytes(adapter.adapterRamBytes));
        if (!adapter.driverVersion.isEmpty()) {
            details.append(u"драйвер "_s + adapter.driverVersion);
        }
        graphics.append(QVariantMap{
            {u"name"_s, adapter.name},
            {u"details"_s, details.isEmpty() ? u"Дополнительные сведения отсутствуют"_s
                                             : details.join(u" · "_s)},
        });
    }

    QVariantList disks;
    for (const auto& disk : snapshot.disks) {
        const auto [healthText, healthTone] = healthPresentation(disk.health);
        QStringList details{decimalSize(disk.sizeBytes)};
        if (!disk.mediaType.isEmpty()) details.append(disk.mediaType);
        if (!disk.busType.isEmpty() && disk.busType != disk.mediaType) details.append(disk.busType);
        disks.append(QVariantMap{
            {u"name"_s, disk.name},
            {u"details"_s, details.join(u" · "_s)},
            {u"healthText"_s, healthText},
            {u"healthTone"_s, healthTone},
        });
    }

    QStringList osSummary;
    if (!snapshot.os.version.isEmpty()) osSummary.append(snapshot.os.version);
    if (snapshot.os.buildNumber > 0) {
        osSummary.append(u"сборка "_s + QString::number(snapshot.os.buildNumber));
    }
    if (!snapshot.os.architecture.isEmpty()) osSummary.append(snapshot.os.architecture);

    QStringList biosSummary;
    if (!snapshot.biosVersion.isEmpty()) biosSummary.append(snapshot.biosVersion);
    if (!snapshot.biosDate.isEmpty()) biosSummary.append(snapshot.biosDate);

    return {
        {u"greetingName"_s, snapshot.displayName},
        {u"computerName"_s, snapshot.computerName},
        {u"manufacturer"_s, snapshot.manufacturer},
        {u"model"_s, snapshot.model},
        {u"baseboard"_s, snapshot.baseboard},
        {u"biosSummary"_s, biosSummary.join(u" · "_s)},
        {u"biosMode"_s, snapshot.biosMode},
        {u"uptime"_s, uptimeText(snapshot.uptimeSeconds)},
        {u"logo"_s, logoName(snapshot.os.buildNumber)},
        {u"osCaption"_s, snapshot.os.caption},
        {u"osSummary"_s, osSummary.join(u" · "_s)},
        {u"processor"_s, QVariantMap{
            {u"title"_s, snapshot.processor.name},
            {u"details"_s, processorDetails},
        }},
        {u"memory"_s, QVariantMap{
            {u"title"_s, binaryGigabytes(snapshot.memory.totalBytes)},
            {u"details"_s, memoryDetails.join(u" · "_s)},
            {u"usedPercent"_s, usedPercent},
        }},
        {u"graphics"_s, graphics},
        {u"disks"_s, disks},
    };
}

} // namespace tweakopedia::app
