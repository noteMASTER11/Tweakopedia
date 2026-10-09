#include "platform/WindowsComponentBackend.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

QString dismPath()
{
    const auto root = qEnvironmentVariable("SystemRoot");
    return QDir(root.isEmpty() ? u"C:/Windows"_s : root).filePath(u"System32/dism.exe"_s);
}

struct ProcessResult {
    bool started{};
    bool finished{};
    int exitCode{-1};
    QString output;
    QString error;
};

ProcessResult runDism(const QStringList& arguments)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(dismPath(), arguments);
    if (!process.waitForStarted(10'000)) {
        return {.error = process.errorString()};
    }
    if (!process.waitForFinished(600'000)) {
        process.kill();
        process.waitForFinished(5'000);
        return {.started = true, .error = u"DISM не завершился за отведённое время."_s};
    }
    return {.started = true,
            .finished = true,
            .exitCode = process.exitCode(),
            .output = QString::fromLocal8Bit(process.readAll()),
            .error = process.error() == QProcess::UnknownError ? QString{} : process.errorString()};
}

std::optional<domain::WindowsComponentState> parseState(
    domain::WindowsComponentKind kind, const QString& output)
{
    const QRegularExpression statePattern(u"(?:^|\\r?\\n)\\s*State\\s*:\\s*([^\\r\\n]+)"_s,
                                          QRegularExpression::CaseInsensitiveOption);
    const auto match = statePattern.match(output);
    if (!match.hasMatch()) return std::nullopt;
    const auto value = match.captured(1).trimmed().toLower();
    if (kind == domain::WindowsComponentKind::Feature) {
        if (value == u"enabled") return domain::WindowsComponentState::Enabled;
        if (value == u"disabled") return domain::WindowsComponentState::Disabled;
        if (value.contains(u"payload removed")) return domain::WindowsComponentState::Absent;
    } else {
        if (value == u"installed") return domain::WindowsComponentState::Enabled;
        if (value == u"not present") return domain::WindowsComponentState::Absent;
    }
    return std::nullopt;
}

bool looksUnsupported(const QString& output)
{
    const auto lower = output.toLower();
    return lower.contains(u"0x800f080c") || lower.contains(u"unknown feature")
        || lower.contains(u"capability name is unknown") || lower.contains(u"not recognized");
}

} // namespace

QStringList WindowsComponentBackend::queryArguments(
    const domain::WindowsComponentTarget& target)
{
    if (!domain::isValidWindowsComponentTarget(target)) return {};
    if (target.kind == domain::WindowsComponentKind::Feature) {
        return {u"/Online"_s, u"/English"_s, u"/Get-FeatureInfo"_s,
                u"/FeatureName:"_s + target.name};
    }
    return {u"/Online"_s, u"/English"_s, u"/Get-CapabilityInfo"_s,
            u"/CapabilityName:"_s + target.name};
}

QStringList WindowsComponentBackend::mutationArguments(
    const domain::WindowsComponentTarget& target, domain::WindowsComponentState state)
{
    if (!domain::isValidWindowsComponentTarget(target)
        || state == domain::WindowsComponentState::Unsupported) return {};
    QStringList result{u"/Online"_s, u"/English"_s};
    if (target.kind == domain::WindowsComponentKind::Feature) {
        if (state == domain::WindowsComponentState::Enabled) result << u"/Enable-Feature"_s;
        else result << u"/Disable-Feature"_s;
        result << (u"/FeatureName:"_s + target.name);
        if (state == domain::WindowsComponentState::Absent) result << u"/Remove"_s;
    } else {
        if (state == domain::WindowsComponentState::Disabled) return {};
        result << (state == domain::WindowsComponentState::Enabled
                       ? u"/Add-Capability"_s : u"/Remove-Capability"_s)
               << (u"/CapabilityName:"_s + target.name);
    }
    result << u"/NoRestart"_s;
    return result;
}

WindowsComponentQueryResult WindowsComponentBackend::query(
    const domain::WindowsComponentTarget& target) const
{
    const auto arguments = queryArguments(target);
    if (arguments.isEmpty()) return {.error = u"Недопустимое имя компонента Windows."_s};
    const auto process = runDism(arguments);
    if (!process.started || !process.finished) return {.error = process.error};
    if (process.exitCode != 0) {
        if (looksUnsupported(process.output)) {
            return WindowsComponentQueryResult::unsupported(
                u"Компонент отсутствует в текущем образе Windows."_s);
        }
        return {.error = u"DISM завершился с кодом %1: %2"_s
                             .arg(process.exitCode).arg(process.output.trimmed())};
    }
    const auto state = parseState(target.kind, process.output);
    if (!state) return {.error = u"DISM не вернул распознаваемое состояние компонента."_s};
    return WindowsComponentQueryResult::present(*state);
}

WindowsComponentMutationResult WindowsComponentBackend::setState(
    const domain::WindowsComponentTarget& target, domain::WindowsComponentState state)
{
    const auto arguments = mutationArguments(target, state);
    if (arguments.isEmpty()) return {.error = u"Недопустимая операция с компонентом Windows."_s};
    const auto process = runDism(arguments);
    if (!process.started || !process.finished) return {.error = process.error};
    constexpr int restartRequired = 3010;
    if (process.exitCode != 0 && process.exitCode != restartRequired) {
        return {.error = u"DISM завершился с кодом %1: %2"_s
                             .arg(process.exitCode).arg(process.output.trimmed())};
    }
    return {.success = true, .restartRequired = process.exitCode == restartRequired};
}

} // namespace tweakopedia::platform
