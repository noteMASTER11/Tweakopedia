#pragma once

#include <QString>

namespace tweakopedia::execution {

inline QString networkDiagnosticsPowerShellScript()
{
    return QStringLiteral(R"PS(
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$marker = '@@TWEAKOPEDIA@@'

function Emit-Object($value) {
    Write-Output ($marker + ($value | ConvertTo-Json -Compress -Depth 6))
}

function Run-Check([string]$Id, [string]$Title, [scriptblock]$Body) {
    Emit-Object ([ordered]@{ type = 'stage'; id = $Id })
    Write-Output ''
    Write-Output ("=== {0} ===" -f $Title)
    try {
        $result = & $Body
        $details = @($result.Details | Select-Object -First 32 | ForEach-Object { ([string]$_).Substring(0, [Math]::Min(([string]$_).Length, 512)) })
        Emit-Object ([ordered]@{
            type = 'check'; id = $Id; title = $Title
            status = [string]$result.Status; summary = [string]$result.Summary
            details = $details
        })
        Write-Output ([string]$result.Summary)
        $details | ForEach-Object { Write-Output ("  {0}" -f $_) }
    } catch {
        $summary = 'Не удалось выполнить проверку: ' + $_.Exception.Message
        Emit-Object ([ordered]@{
            type = 'check'; id = $Id; title = $Title
            status = 'unavailable'; summary = $summary; details = @()
        })
        Write-Output $summary
    }
}

Write-Output 'Диагностика сетевой конфигурации Windows'

Run-Check 'adapters' 'Сетевые адаптеры' {
    $adapters = @(Get-NetAdapter -ErrorAction Stop | Sort-Object Name)
    $active = @($adapters | Where-Object Status -eq 'Up')
    $details = @()
    foreach ($adapter in $adapters) {
        $details += ("{0}: {1}, {2}" -f $adapter.Name, $adapter.Status, $adapter.LinkSpeed)
        if ($adapter.Status -eq 'Up') {
            $configuration = Get-NetIPConfiguration -InterfaceIndex $adapter.ifIndex -ErrorAction SilentlyContinue
            $ipv4 = @($configuration.IPv4Address | ForEach-Object IPAddress)
            $gateway = @($configuration.IPv4DefaultGateway | ForEach-Object NextHop)
            if ($ipv4.Count -gt 0) { $details += ("{0}: IPv4 {1}" -f $adapter.Name, ($ipv4 -join ', ')) }
            if ($gateway.Count -gt 0) { $details += ("{0}: шлюз {1}" -f $adapter.Name, ($gateway -join ', ')) }
        }
    }
    if ($active.Count -gt 0) {
        [pscustomobject]@{ Status = 'ok'; Summary = "Активных адаптеров: $($active.Count)"; Details = $details }
    } else {
        [pscustomobject]@{ Status = 'attention'; Summary = 'Активные сетевые адаптеры не найдены'; Details = $details }
    }
}

Run-Check 'dns' 'DNS' {
    $rows = @(Get-DnsClientServerAddress -AddressFamily IPv4 -ErrorAction Stop |
        Where-Object { $_.ServerAddresses.Count -gt 0 })
    $details = @($rows | ForEach-Object { "{0}: {1}" -f $_.InterfaceAlias, ($_.ServerAddresses -join ', ') })
    $resolved = $false
    try {
        $answer = Resolve-DnsName 'www.microsoft.com' -DnsOnly -QuickTimeout -ErrorAction Stop
        $resolved = @($answer).Count -gt 0
    } catch { $details += ('Контрольный запрос: ' + $_.Exception.Message) }
    if ($rows.Count -eq 0) {
        [pscustomobject]@{ Status = 'attention'; Summary = 'DNS-серверы не назначены'; Details = $details }
    } elseif ($resolved) {
        [pscustomobject]@{ Status = 'ok'; Summary = 'Разрешение доменных имён работает'; Details = $details }
    } else {
        [pscustomobject]@{ Status = 'attention'; Summary = 'Контрольный DNS-запрос не выполнен'; Details = $details }
    }
}

Run-Check 'proxy' 'Прокси' {
    $path = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings'
    $settings = Get-ItemProperty -Path $path -ErrorAction SilentlyContinue
    $winHttp = @(& "$env:SystemRoot\System32\netsh.exe" winhttp show proxy 2>&1 | ForEach-Object { [string]$_ })
    $details = @($winHttp | Select-Object -First 12)
    if ($settings.ProxyEnable -eq 1) { $details += ('Пользовательский прокси: ' + [string]$settings.ProxyServer) }
    if ($settings.AutoConfigURL) { $details += ('PAC: ' + [string]$settings.AutoConfigURL) }
    if ($settings.ProxyEnable -eq 1 -or $settings.AutoConfigURL) {
        [pscustomobject]@{ Status = 'attention'; Summary = 'Обнаружена пользовательская конфигурация прокси'; Details = $details }
    } else {
        [pscustomobject]@{ Status = 'ok'; Summary = 'Пользовательский прокси и PAC не заданы'; Details = $details }
    }
}

Run-Check 'hosts' 'Файл hosts' {
    $path = Join-Path $env:SystemRoot 'System32\drivers\etc\hosts'
    $active = @(Get-Content -LiteralPath $path -ErrorAction Stop | Where-Object {
        $_ -notmatch '^\s*(#|$)' -and $_ -notmatch '^\s*(127\.0\.0\.1|::1)\s'
    })
    if ($active.Count -gt 0) {
        [pscustomobject]@{ Status = 'attention'; Summary = "Нестандартных строк: $($active.Count)"; Details = @($active | Select-Object -First 32) }
    } else {
        [pscustomobject]@{ Status = 'ok'; Summary = 'Нестандартные строки не найдены'; Details = @() }
    }
}

Run-Check 'routes' 'Маршруты' {
    $defaults = @(Get-NetRoute -AddressFamily IPv4 -DestinationPrefix '0.0.0.0/0' -ErrorAction Stop |
        Where-Object State -ne 'Invalid' | Sort-Object RouteMetric)
    $persistent = @(Get-NetRoute -AddressFamily IPv4 -PolicyStore PersistentStore -ErrorAction SilentlyContinue |
        Where-Object DestinationPrefix -ne '0.0.0.0/0')
    $details = @($defaults | ForEach-Object { "По умолчанию: {0}, интерфейс {1}, метрика {2}" -f $_.NextHop, $_.InterfaceAlias, $_.RouteMetric })
    $details += @($persistent | Select-Object -First 20 | ForEach-Object { "Постоянный: {0} через {1}" -f $_.DestinationPrefix, $_.NextHop })
    if ($defaults.Count -eq 1 -and $persistent.Count -eq 0) {
        [pscustomobject]@{ Status = 'ok'; Summary = 'Маршрут по умолчанию определён однозначно'; Details = $details }
    } else {
        [pscustomobject]@{ Status = 'attention'; Summary = "Маршрутов по умолчанию: $($defaults.Count), постоянных: $($persistent.Count)"; Details = $details }
    }
}

Run-Check 'firewall' 'Windows Firewall' {
    $profiles = @(Get-NetFirewallProfile -ErrorAction Stop)
    $disabled = @($profiles | Where-Object Enabled -eq $false)
    $custom = @(Get-NetFirewallRule -Enabled True -Direction Inbound -Action Allow -ErrorAction Stop |
        Where-Object { [string]::IsNullOrWhiteSpace($_.Group) })
    $details = @($profiles | ForEach-Object { "{0}: включён={1}, вход={2}, выход={3}" -f $_.Name, $_.Enabled, $_.DefaultInboundAction, $_.DefaultOutboundAction })
    $details += @($custom | Select-Object -First 20 | ForEach-Object { 'Входящее разрешение: ' + $_.DisplayName })
    if ($disabled.Count -gt 0 -or $custom.Count -gt 0) {
        [pscustomobject]@{ Status = 'attention'; Summary = "Отключённых профилей: $($disabled.Count), пользовательских входящих разрешений: $($custom.Count)"; Details = $details }
    } else {
        [pscustomobject]@{ Status = 'ok'; Summary = 'Профили включены, пользовательских входящих разрешений нет'; Details = $details }
    }
}
)PS");
}

} // namespace tweakopedia::execution
