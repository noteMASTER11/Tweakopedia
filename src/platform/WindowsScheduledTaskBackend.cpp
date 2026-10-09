#include "platform/WindowsScheduledTaskBackend.h"

#include <QScopeGuard>

#include <windows.h>
#include <taskschd.h>

#include <utility>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

template<typename T>
class ComPtr final
{
public:
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept
        : value_(std::exchange(other.value_, nullptr))
    {
    }
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this == &other) return *this;
        if (value_) value_->Release();
        value_ = std::exchange(other.value_, nullptr);
        return *this;
    }
    ~ComPtr() { if (value_) value_->Release(); }
    T** put() { return &value_; }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
private:
    T* value_{};
};

class Bstr final
{
public:
    explicit Bstr(const QString& value)
        : value_(SysAllocStringLen(reinterpret_cast<const OLECHAR*>(value.utf16()),
                                   static_cast<UINT>(value.size()))) {}
    ~Bstr() { SysFreeString(value_); }
    operator BSTR() const { return value_; }
private:
    BSTR value_{};
};

QString hresultMessage(HRESULT result)
{
    wchar_t* buffer{};
    const auto length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(result), 0,
        reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    const auto message = length ? QString::fromWCharArray(buffer, static_cast<qsizetype>(length)).trimmed()
                                : u"HRESULT 0x%1"_s.arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
    if (buffer) LocalFree(buffer);
    return message;
}

bool validLocation(const domain::ScheduledTaskLocation& location)
{
    return location.folder.startsWith(u'\\') && !location.folder.contains(u".."_s)
        && !location.name.trimmed().isEmpty() && !location.name.contains(u'\\')
        && !location.name.contains(u'/');
}

struct OpenTaskResult {
    ComPtr<IRegisteredTask> task;
    bool missing{};
    QString error;
};

OpenTaskResult openTask(const domain::ScheduledTaskLocation& location)
{
    OpenTaskResult result;
    if (!validLocation(location)) {
        result.error = u"Некорректный путь задачи планировщика."_s;
        return result;
    }
    ComPtr<ITaskService> service;
    auto hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                               IID_ITaskService, reinterpret_cast<void**>(service.put()));
    if (FAILED(hr)) {
        result.error = hresultMessage(hr);
        return result;
    }
    VARIANT empty;
    VariantInit(&empty);
    hr = service->Connect(empty, empty, empty, empty);
    if (FAILED(hr)) {
        result.error = hresultMessage(hr);
        return result;
    }
    ComPtr<ITaskFolder> folder;
    hr = service->GetFolder(Bstr(location.folder), folder.put());
    if (FAILED(hr)) {
        result.missing = hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
            || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
        if (!result.missing) result.error = hresultMessage(hr);
        return result;
    }
    hr = folder->GetTask(Bstr(location.name), result.task.put());
    if (FAILED(hr)) {
        result.missing = hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
            || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
        if (!result.missing) result.error = hresultMessage(hr);
    }
    return result;
}

template<typename Function>
auto withCom(Function&& function)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const auto cleanup = qScopeGuard([&] {
        if (initialized == S_OK || initialized == S_FALSE) CoUninitialize();
    });
    return function(initialized);
}

} // namespace

ScheduledTaskReadResult WindowsScheduledTaskBackend::read(
    const domain::ScheduledTaskLocation& location)
{
    return withCom([&](HRESULT initialized) -> ScheduledTaskReadResult {
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
            return ScheduledTaskReadResult::failed(hresultMessage(initialized));
        }
        auto opened = openTask(location);
        if (opened.missing) return ScheduledTaskReadResult::missingTask();
        if (!opened.task) return ScheduledTaskReadResult::failed(opened.error);
        VARIANT_BOOL enabled{};
        const auto hr = opened.task->get_Enabled(&enabled);
        if (FAILED(hr)) return ScheduledTaskReadResult::failed(hresultMessage(hr));
        return ScheduledTaskReadResult::present(enabled == VARIANT_TRUE);
    });
}

ScheduledTaskWriteResult WindowsScheduledTaskBackend::setEnabled(
    const domain::ScheduledTaskLocation& location,
    bool enabled)
{
    return withCom([&](HRESULT initialized) -> ScheduledTaskWriteResult {
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
            return {.error = hresultMessage(initialized)};
        }
        auto opened = openTask(location);
        if (!opened.task) return {.error = opened.missing
            ? u"Задача планировщика отсутствует."_s : opened.error};
        const auto hr = opened.task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
        if (FAILED(hr)) return {.error = hresultMessage(hr)};
        return {.success = true};
    });
}

} // namespace tweakopedia::platform
