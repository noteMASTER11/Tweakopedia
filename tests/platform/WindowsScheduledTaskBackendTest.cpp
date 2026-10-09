#include "platform/WindowsScheduledTaskBackend.h"

#include <QScopeGuard>
#include <QtTest/QTest>

#include <windows.h>
#include <taskschd.h>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

VARIANT emptyVariant()
{
    VARIANT value;
    VariantInit(&value);
    return value;
}

BSTR bstr(const QString& value)
{
    return SysAllocStringLen(reinterpret_cast<const OLECHAR*>(value.utf16()),
                             static_cast<UINT>(value.size()));
}

bool createNeutralTask(const domain::ScheduledTaskLocation& location)
{
    ITaskService* service{};
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITaskService, reinterpret_cast<void**>(&service)))) return false;
    const auto releaseService = qScopeGuard([&] { service->Release(); });
    auto empty = emptyVariant();
    if (FAILED(service->Connect(empty, empty, empty, empty))) return false;
    ITaskFolder* root{};
    BSTR rootName = bstr(u"\\"_s);
    const auto freeRootName = qScopeGuard([&] { SysFreeString(rootName); });
    if (FAILED(service->GetFolder(rootName, &root))) return false;
    const auto releaseRoot = qScopeGuard([&] { root->Release(); });

    ITaskFolder* folder{};
    BSTR folderName = bstr(location.folder.mid(1));
    const auto freeFolderName = qScopeGuard([&] { SysFreeString(folderName); });
    auto sddl = emptyVariant();
    auto hr = root->CreateFolder(folderName, sddl, &folder);
    if (FAILED(hr)) {
        BSTR fullFolder = bstr(location.folder);
        const auto freeFullFolder = qScopeGuard([&] { SysFreeString(fullFolder); });
        hr = service->GetFolder(fullFolder, &folder);
    }
    if (FAILED(hr) || !folder) return false;
    const auto releaseFolder = qScopeGuard([&] { folder->Release(); });

    ITaskDefinition* definition{};
    if (FAILED(service->NewTask(0, &definition))) return false;
    const auto releaseDefinition = qScopeGuard([&] { definition->Release(); });
    IPrincipal* principal{};
    if (FAILED(definition->get_Principal(&principal))) return false;
    const auto releasePrincipal = qScopeGuard([&] { principal->Release(); });
    principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    principal->put_RunLevel(TASK_RUNLEVEL_LUA);

    IActionCollection* actions{};
    if (FAILED(definition->get_Actions(&actions))) return false;
    const auto releaseActions = qScopeGuard([&] { actions->Release(); });
    IAction* action{};
    if (FAILED(actions->Create(TASK_ACTION_EXEC, &action))) return false;
    const auto releaseAction = qScopeGuard([&] { action->Release(); });
    IExecAction* exec{};
    if (FAILED(action->QueryInterface(IID_IExecAction, reinterpret_cast<void**>(&exec)))) return false;
    const auto releaseExec = qScopeGuard([&] { exec->Release(); });
    BSTR path = bstr(u"%SystemRoot%\\System32\\cmd.exe"_s);
    BSTR arguments = bstr(u"/c exit 0"_s);
    const auto freePath = qScopeGuard([&] { SysFreeString(path); });
    const auto freeArguments = qScopeGuard([&] { SysFreeString(arguments); });
    if (FAILED(exec->put_Path(path)) || FAILED(exec->put_Arguments(arguments))) return false;

    IRegisteredTask* registered{};
    BSTR taskName = bstr(location.name);
    const auto freeTaskName = qScopeGuard([&] { SysFreeString(taskName); });
    hr = folder->RegisterTaskDefinition(
        taskName, definition, TASK_CREATE_OR_UPDATE, empty, empty,
        TASK_LOGON_INTERACTIVE_TOKEN, empty, &registered);
    if (registered) registered->Release();
    return SUCCEEDED(hr);
}

void removeTestFolder(const domain::ScheduledTaskLocation& location)
{
    ITaskService* service{};
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITaskService, reinterpret_cast<void**>(&service)))) return;
    const auto releaseService = qScopeGuard([&] { service->Release(); });
    auto empty = emptyVariant();
    if (FAILED(service->Connect(empty, empty, empty, empty))) return;
    ITaskFolder* root{};
    BSTR rootName = bstr(u"\\"_s);
    const auto freeRootName = qScopeGuard([&] { SysFreeString(rootName); });
    if (FAILED(service->GetFolder(rootName, &root))) return;
    const auto releaseRoot = qScopeGuard([&] { root->Release(); });
    ITaskFolder* folder{};
    BSTR fullFolderName = bstr(location.folder);
    const auto freeFullFolderName = qScopeGuard([&] { SysFreeString(fullFolderName); });
    if (SUCCEEDED(service->GetFolder(fullFolderName, &folder)) && folder) {
        const auto releaseFolder = qScopeGuard([&] { folder->Release(); });
        BSTR taskName = bstr(location.name);
        const auto freeTaskName = qScopeGuard([&] { SysFreeString(taskName); });
        folder->DeleteTask(taskName, 0);
    }
    BSTR folderName = bstr(location.folder.mid(1));
    const auto freeFolderName = qScopeGuard([&] { SysFreeString(folderName); });
    root->DeleteFolder(folderName, 0);
}

} // namespace

class WindowsScheduledTaskBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsAndChangesOnlyEnabledState()
    {
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize = qScopeGuard([&] { if (SUCCEEDED(initialized)) CoUninitialize(); });
        QVERIFY(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE);
        const domain::ScheduledTaskLocation location{
            .folder = u"\\TweakopediaTests-"_s
                + QUuid::createUuid().toString(QUuid::WithoutBraces),
            .name = u"Neutral"_s,
        };
        const auto cleanup = qScopeGuard([&] { removeTestFolder(location); });
        if (!createNeutralTask(location)) QSKIP("Task Scheduler test task could not be registered");
        platform::WindowsScheduledTaskBackend backend;

        auto current = backend.read(location);
        QVERIFY2(current.success, qPrintable(current.error));
        QVERIFY(current.enabled);
        QVERIFY(backend.setEnabled(location, false).success);
        current = backend.read(location);
        QVERIFY(current.success);
        QVERIFY(!current.enabled);
        QVERIFY(backend.setEnabled(location, true).success);
        QVERIFY(backend.read(location).enabled);
    }

    void reportsMissingWithoutCreatingTask()
    {
        platform::WindowsScheduledTaskBackend backend;
        const auto result = backend.read({u"\\TweakopediaMissing"_s, u"NoTask"_s});
        QVERIFY(result.missing);
        QVERIFY(!result.success);
    }
};

QTEST_APPLESS_MAIN(WindowsScheduledTaskBackendTest)

#include "WindowsScheduledTaskBackendTest.moc"
