#include "platform/WindowsAppxPackageProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {

QSet<QString> AppxPackageQueryResult::packageNames() const
{
    QSet<QString> result;
    for (const auto& package : packages) result.insert(package.name);
    return result;
}

AppxPackageQueryResult WindowsAppxPackageProvider::parse(const QByteArray& json)
{
    AppxPackageQueryResult result;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        result.error = u"PowerShell вернул некорректный список AppX-пакетов."_s;
        return result;
    }

    QMap<QString, AppxPackageIdentity> unique;
    for (const auto& value : document.array()) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        const auto name = object.value(u"Name"_s).toString().trimmed();
        const auto fullName = object.value(u"PackageFullName"_s).toString().trimmed();
        if (name.isEmpty() || fullName.isEmpty()) continue;
        const auto key = name.toLower();
        if (!unique.contains(key)) {
            unique.insert(key, {.name = name, .fullName = fullName});
        }
    }
    result.packages.reserve(unique.size());
    for (auto iterator = unique.cbegin(); iterator != unique.cend(); ++iterator) {
        result.packages.append(iterator.value());
    }
    return result;
}

AppxPackageQueryResult WindowsAppxPackageProvider::installedForCurrentUser() const
{
#ifdef Q_OS_WIN
    static const QString script =
        u"[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); "
         "@(" 
         "Get-AppxPackage -ErrorAction Stop | "
         "Select-Object Name,PackageFullName" 
         ") | ConvertTo-Json -Compress"_s;
    QProcess process;
    process.setProgram(u"powershell.exe"_s);
    process.setArguments({
        u"-NoLogo"_s,
        u"-NoProfile"_s,
        u"-NonInteractive"_s,
        u"-ExecutionPolicy"_s,
        u"Bypass"_s,
        u"-Command"_s,
        script,
    });
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return {.error = u"Не удалось получить список установленных AppX-пакетов."_s};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto details = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {.error = details.isEmpty()
                ? u"PowerShell завершил поиск AppX-пакетов с ошибкой."_s : details};
    }
    return parse(process.readAllStandardOutput());
#else
    return {.error = u"Поиск AppX-пакетов доступен только в Windows."_s};
#endif
}

AppxPackageMutationResult WindowsAppxPackageProvider::removeCurrentUser(const QString& fullName)
{
#ifdef Q_OS_WIN
    static const QString script =
        u"$ErrorActionPreference='Stop'; "
         "$p=$env:TWEAKOPEDIA_APPX_FULL_NAME; "
         "if([string]::IsNullOrWhiteSpace($p)){exit 2}; "
         "$package=Get-AppxPackage -ErrorAction Stop | "
         "Where-Object {$_.PackageFullName -ceq $p}; "
         "if($null -eq $package){exit 0}; "
         "Remove-AppxPackage -Package $p -ErrorAction Stop; "
         "if(Get-AppxPackage -ErrorAction Stop | "
         "Where-Object {$_.PackageFullName -ceq $p}){exit 3}"_s;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"TWEAKOPEDIA_APPX_FULL_NAME"_s, fullName);
    process.setProcessEnvironment(environment);
    process.setProgram(u"powershell.exe"_s);
    process.setArguments({
        u"-NoLogo"_s,
        u"-NoProfile"_s,
        u"-NonInteractive"_s,
        u"-ExecutionPolicy"_s,
        u"Bypass"_s,
        u"-Command"_s,
        script,
    });
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished();
        return {.error = u"Не удалось завершить удаление AppX-пакета."_s};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto details = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {.error = details.isEmpty()
                ? u"PowerShell не удалил AppX-пакет."_s : details};
    }
    return {.success = true};
#else
    Q_UNUSED(fullName)
    return {.error = u"Удаление AppX-пакетов доступно только в Windows."_s};
#endif
}

} // namespace tweakopedia::platform
