#pragma once

#include <QString>
#include <QVector>

namespace tweakopedia::domain {

enum class DiskHealth {
    Unknown,
    Healthy,
    Warning,
    Unhealthy,
};

struct OperatingSystemOverview {
    QString caption;
    QString version;
    quint32 buildNumber{};
    QString architecture;
    QString installDate;
};

struct ProcessorOverview {
    QString name;
    int coreCount{};
    int logicalProcessorCount{};
    int currentClockMHz{};
};

struct MemoryOverview {
    quint64 totalBytes{};
    quint64 availableBytes{};
    int speedMTs{};
    int moduleCount{};
};

struct GraphicsAdapterOverview {
    QString name;
    quint64 adapterRamBytes{};
    QString driverVersion;
};

struct DiskOverview {
    QString name;
    quint64 sizeBytes{};
    QString mediaType;
    QString busType;
    DiskHealth health{DiskHealth::Unknown};
};

struct SystemOverviewSnapshot {
    QString error;
    QString displayName;
    QString computerName;
    QString manufacturer;
    QString model;
    QString baseboard;
    QString biosVersion;
    QString biosDate;
    QString biosMode;
    quint64 uptimeSeconds{};
    OperatingSystemOverview os;
    ProcessorOverview processor;
    MemoryOverview memory;
    QVector<GraphicsAdapterOverview> graphics;
    QVector<DiskOverview> disks;
};

} // namespace tweakopedia::domain
