#include "platform/WindowsSystemOverviewProvider.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <QVariant>

#include <algorithm>
#include <memory>
#include <optional>

#ifdef Q_OS_WIN
#define SECURITY_WIN32
#include <windows.h>
#include <lmcons.h>
#include <secext.h>
#include <wbemidl.h>
#include <dxgi1_2.h>
#endif

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

#ifdef Q_OS_WIN

template<typename T>
struct ComReleaser {
    void operator()(T* value) const
    {
        if (value) value->Release();
    }
};

template<typename T>
using ComPtr = std::unique_ptr<T, ComReleaser<T>>;

class ComApartment final
{
public:
    ComApartment()
    {
        const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initialized_ = SUCCEEDED(result);
        usable_ = initialized_ || result == RPC_E_CHANGED_MODE;
    }

    ~ComApartment()
    {
        if (initialized_) CoUninitialize();
    }

    [[nodiscard]] bool usable() const noexcept { return usable_; }

private:
    bool initialized_{};
    bool usable_{};
};

QString errorText(HRESULT result)
{
    return u"WMI: HRESULT 0x%1"_s.arg(
        static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
}

QVariant variantValue(const VARIANT& value)
{
    switch (value.vt) {
    case VT_BSTR:
        return QString::fromWCharArray(value.bstrVal);
    case VT_I1: return value.cVal;
    case VT_UI1: return value.bVal;
    case VT_I2: return value.iVal;
    case VT_UI2: return value.uiVal;
    case VT_I4:
    case VT_INT: return static_cast<int>(value.lVal);
    case VT_UI4:
    case VT_UINT: return static_cast<uint>(value.ulVal);
    case VT_I8: return QVariant::fromValue<qint64>(value.llVal);
    case VT_UI8: return QVariant::fromValue<quint64>(value.ullVal);
    case VT_BOOL: return value.boolVal == VARIANT_TRUE;
    case VT_NULL:
    case VT_EMPTY:
        return {};
    default:
        return {};
    }
}

class WmiConnection final
{
public:
    explicit WmiConnection(const wchar_t* namespaceName)
    {
        if (!apartment_.usable()) {
            error_ = u"Не удалось инициализировать COM."_s;
            return;
        }

        IWbemLocator* locator{};
        auto result = CoCreateInstance(
            CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (FAILED(result)) {
            error_ = errorText(result);
            return;
        }
        locator_.reset(locator);

        IWbemServices* services{};
        const auto namespaceValue = SysAllocString(namespaceName);
        result = locator_->ConnectServer(
            namespaceValue, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        SysFreeString(namespaceValue);
        if (FAILED(result)) {
            error_ = errorText(result);
            return;
        }
        services_.reset(services);

        result = CoSetProxyBlanket(
            services_.get(),
            RPC_C_AUTHN_WINNT,
            RPC_C_AUTHZ_NONE,
            nullptr,
            RPC_C_AUTHN_LEVEL_CALL,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr,
            EOAC_NONE);
        if (FAILED(result)) {
            error_ = errorText(result);
            services_.reset();
        }
    }

    [[nodiscard]] QString error() const { return error_; }
    [[nodiscard]] bool ready() const noexcept { return services_ != nullptr; }

    [[nodiscard]] QVector<QVariantMap> query(
        const QString& wql, const QStringList& properties) const
    {
        QVector<QVariantMap> rows;
        if (!services_) return rows;

        IEnumWbemClassObject* rawEnumerator{};
        const auto language = SysAllocString(L"WQL");
        const auto queryText = SysAllocString(
            reinterpret_cast<const wchar_t*>(wql.utf16()));
        const auto result = services_->ExecQuery(
            language,
            queryText,
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &rawEnumerator);
        SysFreeString(queryText);
        SysFreeString(language);
        if (FAILED(result) || !rawEnumerator) return rows;
        ComPtr<IEnumWbemClassObject> enumerator(rawEnumerator);

        while (true) {
            IWbemClassObject* rawObject{};
            ULONG returned{};
            const auto next = enumerator->Next(5000, 1, &rawObject, &returned);
            if (FAILED(next) || returned == 0 || !rawObject) break;
            ComPtr<IWbemClassObject> object(rawObject);
            QVariantMap row;
            for (const auto& property : properties) {
                VARIANT value;
                VariantInit(&value);
                if (SUCCEEDED(object->Get(
                        reinterpret_cast<const wchar_t*>(property.utf16()),
                        0, &value, nullptr, nullptr))) {
                    row.insert(property, variantValue(value));
                }
                VariantClear(&value);
            }
            rows.append(std::move(row));
        }
        return rows;
    }

private:
    ComApartment apartment_;
    ComPtr<IWbemLocator> locator_;
    ComPtr<IWbemServices> services_;
    QString error_;
};

QString displayName()
{
    ULONG size{};
    (void)GetUserNameExW(NameDisplay, nullptr, &size);
    if (size > 0) {
        std::wstring buffer(size, L'\0');
        if (GetUserNameExW(NameDisplay, buffer.data(), &size)) {
            return QString::fromWCharArray(buffer.c_str()).trimmed();
        }
    }
    wchar_t account[UNLEN + 1]{};
    DWORD accountSize = std::size(account);
    return GetUserNameW(account, &accountSize)
        ? QString::fromWCharArray(account).trimmed()
        : QString{};
}

QString biosMode()
{
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    if (!kernel) return {};
    using GetFirmwareTypeFunction = BOOL(WINAPI*)(PFIRMWARE_TYPE);
    const auto getFirmwareType = reinterpret_cast<GetFirmwareTypeFunction>(
        GetProcAddress(kernel, "GetFirmwareType"));
    if (!getFirmwareType) return {};
    FIRMWARE_TYPE type{FirmwareTypeUnknown};
    if (!getFirmwareType(&type)) return {};
    if (type == FirmwareTypeUefi) return u"UEFI"_s;
    if (type == FirmwareTypeBios) return u"Legacy BIOS"_s;
    return {};
}

QString biosDate(const QString& cimDate)
{
    if (cimDate.size() < 8) return {};
    const auto parsed = QDate::fromString(cimDate.left(8), u"yyyyMMdd"_s);
    return parsed.isValid() ? parsed.toString(u"dd.MM.yyyy"_s) : QString{};
}

QString mediaType(int value)
{
    switch (value) {
    case 3: return u"HDD"_s;
    case 4: return u"SSD"_s;
    case 5: return u"SCM"_s;
    default: return {};
    }
}

QString busType(int value)
{
    switch (value) {
    case 3: return u"ATA"_s;
    case 7: return u"USB"_s;
    case 8: return u"RAID"_s;
    case 11: return u"SATA"_s;
    case 14: return u"Виртуальный"_s;
    case 17: return u"NVMe"_s;
    case 18: return u"SCM"_s;
    default: return {};
    }
}

domain::DiskHealth healthFromCode(int value)
{
    switch (value) {
    case 0: return domain::DiskHealth::Healthy;
    case 1: return domain::DiskHealth::Warning;
    case 2: return domain::DiskHealth::Unhealthy;
    default: return domain::DiskHealth::Unknown;
    }
}

domain::DiskHealth healthFromStatus(const QString& value)
{
    if (value.compare(u"OK"_s, Qt::CaseInsensitive) == 0) {
        return domain::DiskHealth::Healthy;
    }
    if (value.contains(u"Pred Fail"_s, Qt::CaseInsensitive)
        || value.contains(u"Error"_s, Qt::CaseInsensitive)) {
        return domain::DiskHealth::Unhealthy;
    }
    if (!value.isEmpty()) return domain::DiskHealth::Warning;
    return domain::DiskHealth::Unknown;
}

struct StorageDisk {
    QString friendlyName;
    quint64 size{};
    QString media;
    QString bus;
    domain::DiskHealth health{domain::DiskHealth::Unknown};
};

struct DxgiAdapter {
    QString name;
    quint64 dedicatedVideoMemory{};
    quint64 sharedSystemMemory{};
    quint32 vendorId{};
    quint32 deviceId{};
};

QVector<DxgiAdapter> dxgiAdapters()
{
    IDXGIFactory1* rawFactory{};
    if (FAILED(CreateDXGIFactory1(
            __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&rawFactory)))) {
        return {};
    }
    ComPtr<IDXGIFactory1> factory(rawFactory);
    QVector<DxgiAdapter> result;
    for (UINT index = 0; ; ++index) {
        IDXGIAdapter1* rawAdapter{};
        if (factory->EnumAdapters1(index, &rawAdapter) == DXGI_ERROR_NOT_FOUND) break;
        if (!rawAdapter) continue;
        ComPtr<IDXGIAdapter1> adapter(rawAdapter);
        DXGI_ADAPTER_DESC1 description{};
        if (FAILED(adapter->GetDesc1(&description))
            || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            continue;
        }
        result.append({
            .name = QString::fromWCharArray(description.Description).simplified(),
            .dedicatedVideoMemory =
                static_cast<quint64>(description.DedicatedVideoMemory),
            .sharedSystemMemory =
                static_cast<quint64>(description.SharedSystemMemory),
            .vendorId = description.VendorId,
            .deviceId = description.DeviceId,
        });
    }
    return result;
}

const DxgiAdapter* findDxgiAdapter(
    const QString& name, const QVector<DxgiAdapter>& adapters)
{
    const auto normalized = name.simplified().toLower();
    for (const auto& adapter : adapters) {
        const auto candidate = adapter.name.simplified().toLower();
        if (candidate == normalized
            || candidate.contains(normalized)
            || normalized.contains(candidate)) {
            return &adapter;
        }
    }
    return nullptr;
}

quint64 saneWmiVideoMemory(const QVariant& value)
{
    bool valid{};
    const auto signedValue = value.toLongLong(&valid);
    constexpr qint64 maximumPlausibleBytes = 1024LL * 1024 * 1024 * 1024;
    return valid && signedValue > 0 && signedValue <= maximumPlausibleBytes
        ? static_cast<quint64>(signedValue)
        : 0;
}

QVector<StorageDisk> storageDisks()
{
    WmiConnection storage(L"ROOT\\Microsoft\\Windows\\Storage");
    if (!storage.ready()) return {};
    QVector<StorageDisk> result;
    const auto rows = storage.query(
        u"SELECT FriendlyName, Size, MediaType, BusType, HealthStatus FROM MSFT_PhysicalDisk"_s,
        {u"FriendlyName"_s, u"Size"_s, u"MediaType"_s, u"BusType"_s, u"HealthStatus"_s});
    for (const auto& row : rows) {
        result.append({
            .friendlyName = row.value(u"FriendlyName"_s).toString().trimmed(),
            .size = row.value(u"Size"_s).toULongLong(),
            .media = mediaType(row.value(u"MediaType"_s).toInt()),
            .bus = busType(row.value(u"BusType"_s).toInt()),
            .health = healthFromCode(row.value(u"HealthStatus"_s).toInt()),
        });
    }
    return result;
}

const StorageDisk* findStorageDisk(
    const QString& model, quint64 size, const QVector<StorageDisk>& storage)
{
    const auto normalized = model.trimmed().toLower();
    for (const auto& disk : storage) {
        const auto candidate = disk.friendlyName.trimmed().toLower();
        if (!candidate.isEmpty()
            && (normalized.contains(candidate) || candidate.contains(normalized))) {
            return &disk;
        }
    }
    for (const auto& disk : storage) {
        const auto difference = disk.size > size ? disk.size - size : size - disk.size;
        if (size > 0 && difference < size / 100) return &disk;
    }
    return nullptr;
}

#endif

} // namespace

domain::SystemOverviewSnapshot WindowsSystemOverviewProvider::collect() const
{
    domain::SystemOverviewSnapshot result;
#ifdef Q_OS_WIN
    WmiConnection cim(L"ROOT\\CIMV2");
    if (!cim.ready()) {
        result.error = cim.error().isEmpty()
            ? u"Не удалось подключиться к WMI."_s
            : cim.error();
        return result;
    }

    result.displayName = displayName();
    result.biosMode = biosMode();
    result.uptimeSeconds = GetTickCount64() / 1000;

    const auto systems = cim.query(
        u"SELECT Name, Manufacturer, Model, TotalPhysicalMemory FROM Win32_ComputerSystem"_s,
        {u"Name"_s, u"Manufacturer"_s, u"Model"_s, u"TotalPhysicalMemory"_s});
    if (!systems.isEmpty()) {
        const auto& system = systems.first();
        result.computerName = system.value(u"Name"_s).toString().trimmed();
        result.manufacturer = system.value(u"Manufacturer"_s).toString().trimmed();
        result.model = system.value(u"Model"_s).toString().trimmed();
        result.memory.totalBytes = system.value(u"TotalPhysicalMemory"_s).toULongLong();
    }

    const auto operatingSystems = cim.query(
        u"SELECT Caption, Version, BuildNumber, OSArchitecture, InstallDate, FreePhysicalMemory FROM Win32_OperatingSystem"_s,
        {u"Caption"_s, u"Version"_s, u"BuildNumber"_s, u"OSArchitecture"_s,
         u"InstallDate"_s, u"FreePhysicalMemory"_s});
    if (!operatingSystems.isEmpty()) {
        const auto& os = operatingSystems.first();
        result.os.caption = os.value(u"Caption"_s).toString().trimmed();
        result.os.version = os.value(u"Version"_s).toString().trimmed();
        result.os.buildNumber = os.value(u"BuildNumber"_s).toString().toUInt();
        result.os.architecture = os.value(u"OSArchitecture"_s).toString().trimmed();
        result.os.installDate = biosDate(os.value(u"InstallDate"_s).toString());
        result.memory.availableBytes =
            os.value(u"FreePhysicalMemory"_s).toULongLong() * 1024;
    }

    const auto baseboards = cim.query(
        u"SELECT Product FROM Win32_BaseBoard"_s, {u"Product"_s});
    if (!baseboards.isEmpty()) {
        result.baseboard = baseboards.first().value(u"Product"_s).toString().trimmed();
    }

    const auto biosRows = cim.query(
        u"SELECT SMBIOSBIOSVersion, ReleaseDate FROM Win32_BIOS"_s,
        {u"SMBIOSBIOSVersion"_s, u"ReleaseDate"_s});
    if (!biosRows.isEmpty()) {
        result.biosVersion =
            biosRows.first().value(u"SMBIOSBIOSVersion"_s).toString().trimmed();
        result.biosDate = biosDate(biosRows.first().value(u"ReleaseDate"_s).toString());
    }

    const auto processors = cim.query(
        u"SELECT Name, NumberOfCores, NumberOfLogicalProcessors, CurrentClockSpeed FROM Win32_Processor"_s,
        {u"Name"_s, u"NumberOfCores"_s, u"NumberOfLogicalProcessors"_s,
         u"CurrentClockSpeed"_s});
    if (!processors.isEmpty()) {
        const auto& processor = processors.first();
        result.processor = {
            .name = processor.value(u"Name"_s).toString().simplified(),
            .coreCount = processor.value(u"NumberOfCores"_s).toInt(),
            .logicalProcessorCount =
                processor.value(u"NumberOfLogicalProcessors"_s).toInt(),
            .currentClockMHz = processor.value(u"CurrentClockSpeed"_s).toInt(),
        };
    }

    const auto modules = cim.query(
        u"SELECT Capacity, ConfiguredClockSpeed FROM Win32_PhysicalMemory"_s,
        {u"Capacity"_s, u"ConfiguredClockSpeed"_s});
    quint64 moduleCapacity{};
    for (const auto& module : modules) {
        moduleCapacity += module.value(u"Capacity"_s).toULongLong();
        result.memory.speedMTs = qMax(
            result.memory.speedMTs,
            module.value(u"ConfiguredClockSpeed"_s).toInt());
    }
    result.memory.moduleCount = modules.size();
    if (moduleCapacity > 0) result.memory.totalBytes = moduleCapacity;

    const auto dxgi = dxgiAdapters();
    QSet<QString> matchedDxgi;
    const auto adapters = cim.query(
        u"SELECT Name, AdapterRAM, DriverVersion FROM Win32_VideoController"_s,
        {u"Name"_s, u"AdapterRAM"_s, u"DriverVersion"_s});
    for (const auto& adapter : adapters) {
        const auto name = adapter.value(u"Name"_s).toString().simplified();
        if (name.isEmpty()) continue;
        const auto* nativeAdapter = findDxgiAdapter(name, dxgi);
        if (nativeAdapter) matchedDxgi.insert(nativeAdapter->name.toLower());
        result.graphics.append({
            .name = name,
            .adapterRamBytes = nativeAdapter
                ? nativeAdapter->dedicatedVideoMemory
                : saneWmiVideoMemory(adapter.value(u"AdapterRAM"_s)),
            .sharedSystemMemoryBytes = nativeAdapter
                ? nativeAdapter->sharedSystemMemory : 0,
            .driverVersion = adapter.value(u"DriverVersion"_s).toString().trimmed(),
            .vendorId = nativeAdapter ? nativeAdapter->vendorId : 0,
            .deviceId = nativeAdapter ? nativeAdapter->deviceId : 0,
        });
    }
    for (const auto& adapter : dxgi) {
        if (matchedDxgi.contains(adapter.name.toLower())) continue;
        result.graphics.append({
            .name = adapter.name,
            .adapterRamBytes = adapter.dedicatedVideoMemory,
            .sharedSystemMemoryBytes = adapter.sharedSystemMemory,
            .vendorId = adapter.vendorId,
            .deviceId = adapter.deviceId,
        });
    }

    const auto storage = storageDisks();
    const auto diskRows = cim.query(
        u"SELECT Model, Size, InterfaceType, MediaType, Status FROM Win32_DiskDrive"_s,
        {u"Model"_s, u"Size"_s, u"InterfaceType"_s, u"MediaType"_s, u"Status"_s});
    for (const auto& row : diskRows) {
        const auto model = row.value(u"Model"_s).toString().simplified();
        const auto size = row.value(u"Size"_s).toULongLong();
        const auto* matched = findStorageDisk(model, size, storage);
        auto media = matched ? matched->media : QString{};
        auto bus = matched ? matched->bus
                           : row.value(u"InterfaceType"_s).toString().trimmed();
        if (media.isEmpty()) {
            const auto legacyMedia = row.value(u"MediaType"_s).toString();
            if (legacyMedia.contains(u"SSD"_s, Qt::CaseInsensitive)) media = u"SSD"_s;
            else if (!legacyMedia.isEmpty()) media = u"HDD"_s;
        }
        result.disks.append({
            .name = model,
            .sizeBytes = matched && matched->size > 0 ? matched->size : size,
            .mediaType = media,
            .busType = bus,
            .health = matched ? matched->health
                              : healthFromStatus(row.value(u"Status"_s).toString()),
        });
    }
#else
    result.error = u"Сведения доступны только в Windows."_s;
#endif
    return result;
}

} // namespace tweakopedia::platform
