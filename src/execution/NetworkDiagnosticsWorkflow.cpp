#include "execution/NetworkDiagnosticsWorkflow.h"

#include "execution/NetworkDiagnosticsScript.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QSysInfo>

#include <windows.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

constexpr QLatin1StringView markerPrefix{"@@TWEAKOPEDIA@@"};
constexpr qsizetype maximumLineBuffer = 2 * 1024 * 1024;

struct CheckDefinition {
    const char16_t* id;
    const char16_t* title;
    SystemToolStage stage;
};

constexpr CheckDefinition checkDefinitions[] = {
    {u"adapters", u"Сетевые адаптеры", SystemToolStage::CollectNetworkAdapters},
    {u"dns", u"DNS", SystemToolStage::CheckNetworkDns},
    {u"proxy", u"Прокси", SystemToolStage::CheckNetworkProxy},
    {u"hosts", u"Файл hosts", SystemToolStage::CheckNetworkHosts},
    {u"routes", u"Маршруты", SystemToolStage::CheckNetworkRoutes},
    {u"firewall", u"Windows Firewall", SystemToolStage::CheckNetworkFirewall},
};

const CheckDefinition* definition(const QString& id)
{
    for (const auto& item : checkDefinitions) {
        if (id == item.id) return &item;
    }
    return nullptr;
}

QString powerShellPath()
{
    wchar_t directory[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(directory, MAX_PATH + 1);
    const auto systemDirectory = length > 0 && length <= MAX_PATH
        ? QString::fromWCharArray(directory, static_cast<qsizetype>(length))
        : QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot", "C:\\Windows")
                                   + u"/System32"_s);
    return QDir(systemDirectory).filePath(u"WindowsPowerShell/v1.0/powershell.exe"_s);
}

QString encodedScript()
{
    const auto script = networkDiagnosticsPowerShellScript();
    const QByteArray utf16(
        reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}

QString boundedString(const QJsonValue& value, const qsizetype maximum)
{
    return value.isString() ? value.toString().left(maximum) : QString{};
}

QJsonObject markerObjectInLine(const QString& line)
{
    const auto markerIndex = line.indexOf(markerPrefix);
    if (markerIndex < 0) return {};
    const auto jsonStart = markerIndex + markerPrefix.size();
    if (jsonStart >= line.size() || line.at(jsonStart) != u'{') return {};
    bool inString{};
    bool escaped{};
    int depth{};
    for (auto index = jsonStart; index < line.size(); ++index) {
        const auto character = line.at(index);
        if (inString) {
            if (escaped) escaped = false;
            else if (character == u'\\') escaped = true;
            else if (character == u'"') inString = false;
            continue;
        }
        if (character == u'"') inString = true;
        else if (character == u'{') ++depth;
        else if (character == u'}' && --depth == 0) {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(
                line.sliced(jsonStart, index - jsonStart + 1).toUtf8(), &error);
            return error.error == QJsonParseError::NoError && document.isObject()
                ? document.object() : QJsonObject{};
        }
    }
    return {};
}

bool isSerializedProgress(const QString& line)
{
    const auto trimmed = line.trimmed();
    return trimmed.startsWith(u"#< CLIXML"_s)
        || trimmed.startsWith(u"<Objs"_s)
        || trimmed.startsWith(u"</Objs"_s)
        || (trimmed.startsWith(u'<')
            && trimmed.contains(u"schemas.microsoft.com/powershell/2004/04"_s));
}

QJsonArray boundedFirewallItems(const QJsonValue& value)
{
    QJsonArray result;
    if (!value.isArray()) return result;
    constexpr const char16_t* fields[] = {
        u"key", u"title", u"description", u"profile", u"program", u"service",
        u"protocol", u"local_port", u"remote_port", u"local_address", u"remote_address",
    };
    const auto source = value.toArray();
    for (qsizetype index = 0; index < source.size() && index < 64; ++index) {
        if (!source.at(index).isObject()) continue;
        const auto object = source.at(index).toObject();
        const auto key = boundedString(object.value(u"key"_s), 512);
        if (key.isEmpty() || object.value(u"key"_s).toString().size() > 512) continue;
        QJsonObject item{{u"key"_s, key}};
        for (const auto* field : fields) {
            const QString name(field);
            if (name == u"key"_s) continue;
            const auto text = boundedString(object.value(name), 1024);
            if (!text.isEmpty()) item.insert(name, text);
        }
        result.append(item);
    }
    return result;
}

} // namespace

NetworkDiagnosticsWorkflow::NetworkDiagnosticsWorkflow(
    IDismProcessRunner& runner,
    QString logsRoot,
    QObject* parent)
    : QObject(parent), runner_(&runner), logsRoot_(std::move(logsRoot))
{
}

bool NetworkDiagnosticsWorkflow::start(const SystemToolRequest& request)
{
    if (active_ || request.requestId.isNull()
        || request.tool != SystemToolKind::NetworkDiagnostics
        || request.operation != SystemToolOperation::Diagnose
        || !request.target.isEmpty()) {
        return false;
    }
    request_ = request;
    if (!log_.open(logsRoot_, request.tool, request.operation,
                   {u"Windows: "_s + QSysInfo::prettyProductName(),
                    u"Architecture: "_s + QSysInfo::currentCpuArchitecture()})) {
        emit finished({.requestId = request.requestId,
                       .status = SystemToolResultStatus::Failed,
                       .code = u"tool.log_open_failed"_s,
                       .message = log_.errorString()});
        return false;
    }

    checks_ = {};
    lineBuffer_.clear();
    decoder_.resetState();
    active_ = true;
    const QStringList arguments{
        u"-NoLogo"_s, u"-NoProfile"_s, u"-NonInteractive"_s,
        u"-ExecutionPolicy"_s, u"Bypass"_s,
        u"-OutputFormat"_s, u"Text"_s,
        u"-EncodedCommand"_s, encodedScript(),
    };
    const auto executable = powerShellPath();
    log_.appendCommand(u"powershell.exe"_s,
                       {u"-NoLogo"_s, u"-NoProfile"_s, u"-NonInteractive"_s,
                        u"-ExecutionPolicy"_s, u"Bypass"_s,
                        u"-OutputFormat"_s, u"Text"_s,
                        u"-EncodedCommand"_s, u"<embedded>"_s});
    emit outputReady(request.requestId, u"> Диагностика сети PowerShell\n"_s);
    if (!runner_->start(
            executable,
            arguments,
            [this](const QByteArray& bytes) { consumeOutput(bytes); },
            [this](DismProcessResult result) { processFinished(std::move(result)); })) {
        complete({.requestId = request.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .code = u"network.runner_busy"_s,
                  .message = u"Не удалось запустить диагностику сети."_s});
    }
    return active_;
}

bool NetworkDiagnosticsWorkflow::active() const noexcept
{
    return active_;
}

void NetworkDiagnosticsWorkflow::consumeOutput(const QByteArray& bytes)
{
    log_.appendOutput(bytes);
    consumeText(decoder_(bytes));
}

void NetworkDiagnosticsWorkflow::consumeText(const QString& text)
{
    lineBuffer_.append(text);
    qsizetype newline{};
    while ((newline = lineBuffer_.indexOf(u'\n')) >= 0) {
        auto line = lineBuffer_.first(newline);
        lineBuffer_.remove(0, newline + 1);
        if (line.endsWith(u'\r')) line.chop(1);
        processLine(std::move(line));
    }
    if (lineBuffer_.size() > maximumLineBuffer)
        lineBuffer_ = lineBuffer_.right(maximumLineBuffer);
}

void NetworkDiagnosticsWorkflow::processLine(QString line)
{
    const auto marker = markerObjectInLine(line);
    if (!marker.isEmpty()) processMarker(marker);
    if (marker.isEmpty() && !isSerializedProgress(line))
        emit outputReady(request_.requestId, line + u'\n');
}

void NetworkDiagnosticsWorkflow::processMarker(const QJsonObject& marker)
{
    const auto type = marker.value(u"type"_s).toString();
    const auto id = marker.value(u"id"_s).toString();
    const auto* item = definition(id);
    if (!item) return;
    if (type == u"stage") {
        emit stageChanged({request_.requestId, item->stage});
        return;
    }
    if (type != u"check") return;
    for (const auto& existing : checks_) {
        if (existing.toObject().value(u"id"_s).toString() == id) return;
    }
    const auto status = marker.value(u"status"_s).toString();
    if (status != u"ok" && status != u"attention" && status != u"unavailable") return;
    QJsonArray details;
    const auto sourceDetails = marker.value(u"details"_s).toArray();
    for (qsizetype index = 0; index < sourceDetails.size() && index < 32; ++index) {
        const auto text = boundedString(sourceDetails.at(index), 512);
        if (!text.isEmpty()) details.append(text);
    }
    QJsonObject check{
        {u"id"_s, id},
        {u"title"_s, boundedString(marker.value(u"title"_s), 80)},
        {u"status"_s, status},
        {u"summary"_s, boundedString(marker.value(u"summary"_s), 300)},
        {u"details"_s, details},
    };
    if (id == u"firewall"_s)
        check.insert(u"items"_s, boundedFirewallItems(marker.value(u"items"_s)));
    checks_.append(check);
}

void NetworkDiagnosticsWorkflow::processFinished(DismProcessResult result)
{
    consumeText(decoder_({}));
    if (!lineBuffer_.isEmpty()) processLine(std::exchange(lineBuffer_, {}));
    if (result.termination != DismProcessTermination::Normal || result.exitCode != 0) {
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = result.termination == DismProcessTermination::FailedToStart
                      ? u"network.start_failed"_s : u"network.process_failed"_s,
                  .message = result.error.isEmpty()
                      ? u"Диагностика сети завершилась с ошибкой."_s : result.error});
        return;
    }
    if (checks_.isEmpty()) {
        complete({.requestId = request_.requestId,
                  .status = SystemToolResultStatus::Failed,
                  .exitCode = result.exitCode,
                  .code = u"network.no_results"_s,
                  .message = u"Диагностика не вернула структурированный результат."_s});
        return;
    }
    const auto details = buildDetails();
    const auto attention = details.value(u"attention_count"_s).toInt();
    const auto unavailable = details.value(u"unavailable_count"_s).toInt();
    complete({.requestId = request_.requestId,
              .status = SystemToolResultStatus::Completed,
              .exitCode = 0,
              .outcome = attention > 0 ? SystemToolOutcome::IssuesFound
                  : unavailable > 0 ? SystemToolOutcome::Incomplete
                                    : SystemToolOutcome::Healthy,
              .details = details});
}

QJsonObject NetworkDiagnosticsWorkflow::buildDetails() const
{
    QJsonArray ordered;
    int ok{};
    int attention{};
    int unavailable{};
    for (const auto& definition : checkDefinitions) {
        QJsonObject found;
        for (const auto& value : checks_) {
            const auto object = value.toObject();
            if (object.value(u"id"_s).toString() == definition.id) {
                found = object;
                break;
            }
        }
        if (found.isEmpty()) {
            found = {{u"id"_s, QString(definition.id)},
                     {u"title"_s, QString(definition.title)},
                     {u"status"_s, u"unavailable"_s},
                     {u"summary"_s, u"Проверка не вернула результат"_s},
                     {u"details"_s, QJsonArray{}}};
        }
        const auto status = found.value(u"status"_s).toString();
        if (status == u"ok") ++ok;
        else if (status == u"attention") ++attention;
        else ++unavailable;
        ordered.append(found);
    }
    return {{u"checks"_s, ordered}, {u"ok_count"_s, ok},
            {u"attention_count"_s, attention},
            {u"unavailable_count"_s, unavailable}};
}

void NetworkDiagnosticsWorkflow::complete(SystemToolResult result)
{
    if (!active_) return;
    active_ = false;
    log_.close();
    result.logPath = log_.path();
    emit finished(result);
}

} // namespace tweakopedia::execution
