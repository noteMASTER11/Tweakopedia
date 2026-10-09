[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $InputManifest,

    [Parameter(Mandatory)]
    [string] $OutputRoot,

    [Parameter(Mandatory)]
    [string] $AuditOutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Quote-Yaml([AllowEmptyString()][string] $Value) {
    return "'" + $Value.Replace("'", "''") + "'"
}

function Reject-Candidate([object] $Candidate, [string] $Reason) {
    $Candidate.decision = 'rejected'
    $Candidate.tweak_id = $null
    $Candidate.reason_code = $Reason
}

function Get-Verification([string] $Path) {
    if ($Path -match '(?i)\\Policies\\Microsoft\\Edge') { return 'microsoft-edge-policy' }
    if ($Path -match '(?i)\\Policies\\Mozilla\\Firefox') { return 'mozilla-policy-templates' }
    if ($Path -match '(?i)\\Policies\\Google\\Chrome') { return 'chrome-enterprise-policy' }
    if ($Path -match '(?i)\\Policies\\') { return 'local-windows-admx' }
    return 'source-payload-crosscheck'
}

function Get-Category([string] $TweakId) {
    $candidate = ($TweakId -split '\.')[0]
    if ($candidate -in @('behavior', 'desktop', 'privacy', 'network', 'updates', 'power', 'devices')) {
        return $candidate
    }
    return 'behavior'
}

function Get-Subcategory([string] $Category, [string] $Path) {
    switch ($Category) {
        'behavior' {
            if ($Path -match '(?i)\\Policies\\') { return 'group-policy' }
            if ($Path -match '(?i)\\AutoPlay|\\AutoplayHandlers|\\StorageDevicePolicies') { return 'media' }
            return 'windows'
        }
        'desktop' {
            if ($Path -match '(?i)\\Search|Windows Search') { return 'search' }
            if ($Path -match '(?i)\\Taskband|\\Taskbar|Feeds|ShellFeedsTaskbarViewMode') { return 'taskbar' }
            if ($Path -match '(?i)\\DWM|WindowMetrics|VisualEffects|UserPreferencesMask') { return 'effects' }
            if ($Path -match '(?i)\\Start|StartMenu') { return 'start' }
            return 'explorer'
        }
        'privacy' {
            if ($Path -match '(?i)AppPrivacy|ConsentStore') { return 'app-permissions' }
            if ($Path -match '(?i)Activity|PublishUserActivities|UploadUserActivities') { return 'activity' }
            if ($Path -match '(?i)Advertising|Personalization|InputPersonalization') { return 'personalization' }
            return 'diagnostics'
        }
        'network' {
            if ($Path -match '(?i)Lanman|Sharing|NetworkProvider') { return 'sharing' }
            if ($Path -match '(?i)DNS|Dnscache|NetworkList') { return 'discovery' }
            return 'performance'
        }
        'updates' {
            if ($Path -match '(?i)Driver') { return 'drivers' }
            if ($Path -match '(?i)DeliveryOptimization') { return 'delivery' }
            return 'automatic'
        }
        'power' { return 'advanced-platform' }
        'devices' {
            if ($Path -match '(?i)DriverSearching|DeviceInstall|DriverInstall') { return 'installation' }
            return 'metadata'
        }
    }
}

function Get-Restart([string] $Path) {
    if ($Path -match '(?i)^HKLM\\SYSTEM\\') { return 'reboot' }
    if ($Path -match '(?i)\\Explorer|\\DWM|\\Control Panel\\Desktop') { return 'explorer' }
    return 'none'
}

function Get-Title([object] $Candidate, [string] $ValueName) {
    $titles = @($Candidate.details.titles | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    $base = if ($titles.Count -gt 0) { [string]$titles[0] } else { 'Параметр Windows' }
    if ($base.Length -gt 90) { $base = $base.Substring(0, 87) + '…' }
    return "$base — параметр $ValueName"
}

function Convert-ObservedValue([string] $NativeType, [AllowEmptyString()][string] $Raw) {
    switch ($NativeType) {
        'REG_DWORD' {
            $text = $Raw.Trim()
            if ($text -match '^(?i)dword:(?<hex>[0-9a-f]{8})$') {
                return [pscustomobject]@{ Type = 'dword'; Value = [Convert]::ToUInt32($Matches.hex, 16) }
            }
            [uint32] $number = 0
            if ([uint32]::TryParse($text, [ref]$number)) {
                return [pscustomobject]@{ Type = 'dword'; Value = $number }
            }
            return $null
        }
        'REG_SZ' {
            $text = $Raw
            if ($text.Length -ge 2 -and $text[0] -eq '"' -and $text[$text.Length - 1] -eq '"') {
                $text = $text.Substring(1, $text.Length - 2) -replace '\\"', '"'
            }
            return [pscustomobject]@{ Type = 'string'; Value = $text }
        }
        'REG_BINARY' {
            if ($Raw -notmatch '^(?i)hex:(?<bytes>[0-9a-f,\s]+)$') { return $null }
            $hex = $Matches.bytes -replace '[^0-9a-fA-F]', ''
            if ([string]::IsNullOrEmpty($hex) -or ($hex.Length % 2) -ne 0) { return $null }
            return [pscustomobject]@{ Type = 'binary'; Value = $hex.ToLowerInvariant() }
        }
        'REG_EXPAND_SZ' {
            if ($Raw -notmatch '^(?i)hex\(2\):(?<bytes>[0-9a-f,\s]+)$') { return $null }
            $hex = $Matches.bytes -replace '[^0-9a-fA-F]', ''
            if ([string]::IsNullOrEmpty($hex) -or ($hex.Length % 2) -ne 0) { return $null }
            $bytes = [byte[]]::new($hex.Length / 2)
            for ($index = 0; $index -lt $bytes.Length; $index++) {
                $bytes[$index] = [Convert]::ToByte($hex.Substring($index * 2, 2), 16)
            }
            $text = [Text.Encoding]::Unicode.GetString($bytes).TrimEnd([char]0)
            return [pscustomobject]@{ Type = 'expand_string'; Value = $text }
        }
    }
    return $null
}

function Get-ValueLabel([object] $Value) {
    if ($Value.Type -eq 'dword') { return "Значение $($Value.Value)" }
    if ($Value.Type -eq 'binary') { return "Двоичные данные $($Value.Value)" }
    $text = [string]$Value.Value
    if ($text.Length -gt 56) { $text = $text.Substring(0, 53) + '…' }
    if ([string]::IsNullOrEmpty($text)) { return 'Пустая строка' }
    return "Строка «$text»"
}

function Add-ValueYaml(
    [object] $Candidate,
    [string] $Hive,
    [string] $Key,
    [string] $View,
    [string] $ValueName,
    [object[]] $Values,
    [string] $Destination
) {
    $id = [string]$Candidate.tweak_id
    $category = Get-Category $id
    $subcategory = Get-Subcategory $category "$Hive\$Key"
    $title = Get-Title $Candidate $ValueName
    $location = "$Hive\$Key\$ValueName"
    $restart = Get-Restart "$Hive\$Key"
    $impact = if ($Hive -eq 'HKLM' -or $Key -match '(?i)\\Policies\\') { 'medium' } else { 'low' }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("id: $id")
    $lines.Add("title: $(Quote-Yaml $title)")
    $lines.Add("category: $category")
    $lines.Add("subcategory: $subcategory")
    $lines.Add('kind: setting')
    $lines.Add("summary: $(Quote-Yaml "Управляет значением реестра $ValueName и показывает доступные варианты без скрытых команд.")")
    $lines.Add('explanation:')
    $lines.Add("  purpose: $(Quote-Yaml "Позволяет явно выбрать сохранённое значение параметра $ValueName или удалить его.")")
    $lines.Add("  mechanism: $(Quote-Yaml 'Windows и приложения читают этот параметр из реестра; Tweakopedia записывает точный тип и содержимое значения.')")
    $lines.Add("  effect: $(Quote-Yaml 'Результат зависит от компонента, которому принадлежит объект. Доступные состояния повторяют однозначные значения из исходных твикеров.')")
    $lines.Add("  tradeoffs: $(Quote-Yaml 'Неизвестные текущей версии Windows значения отображаются как пользовательское состояние и не подменяются догадкой.')")
    $lines.Add("  recommendation: $(Quote-Yaml 'Перед применением сопоставьте выбранное буквальное значение с описанием и текущим состоянием параметра.')")
    $lines.Add("  technical_details: $(Quote-Yaml "Объект: $location; представление: $View.")")
    $lines.Add('compatibility:')
    $lines.Add('  architectures: [x64]')
    $lines.Add('  os: [windows10, windows11]')
    $lines.Add('  min_build: 10240')
    $lines.Add('states:')
    for ($index = 0; $index -lt $Values.Count; $index++) {
        $stateId = "value_$index"
        $value = $Values[$index]
        $lines.Add("  ${stateId}:")
        $lines.Add("    title: $(Quote-Yaml (Get-ValueLabel $value))")
        $lines.Add('    operations:')
        $lines.Add('      - type: registry.set_value')
        $lines.Add("        hive: $Hive")
        $lines.Add("        key: $(Quote-Yaml $Key)")
        $lines.Add("        value_name: $(Quote-Yaml $ValueName)")
        $lines.Add("        view: $View")
        $lines.Add("        value_type: $($value.Type)")
        if ($value.Type -eq 'dword') {
            $lines.Add("        value: $($value.Value)")
        } else {
            $lines.Add("        value: $(Quote-Yaml ([string]$value.Value))")
        }
    }
    $lines.Add('  absent:')
    $lines.Add("    title: 'Значение отсутствует'")
    $lines.Add('    operations:')
    $lines.Add('      - type: registry.delete_value')
    $lines.Add("        hive: $Hive")
    $lines.Add("        key: $(Quote-Yaml $Key)")
    $lines.Add("        value_name: $(Quote-Yaml $ValueName)")
    $lines.Add("        view: $View")
    $lines.Add('windows_defaults: []')
    $lines.Add('detect:')
    $lines.Add('  type: registry.value')
    $lines.Add("  hive: $Hive")
    $lines.Add("  key: $(Quote-Yaml $Key)")
    $lines.Add("  value_name: $(Quote-Yaml $ValueName)")
    $lines.Add("  view: $View")
    $lines.Add('  states:')
    for ($index = 0; $index -lt $Values.Count; $index++) {
        $value = $Values[$index]
        $lines.Add("    value_${index}:")
        $lines.Add("      value_type: $($value.Type)")
        if ($value.Type -eq 'dword') {
            $lines.Add("      value: $($value.Value)")
        } else {
            $lines.Add("      value: $(Quote-Yaml ([string]$value.Value))")
        }
    }
    $lines.Add('  missing_state: absent')
    $lines.Add('dependencies: []')
    $lines.Add('conflicts: []')
    $lines.Add("restart: $restart")
    $lines.Add("impact: $impact")
    $lines.Add('reversibility: reversible')
    $lines.Add('requires_network: false')
    Set-Content -LiteralPath $Destination -Value $lines -Encoding utf8NoBOM
}

$manifest = Get-Content -LiteralPath $InputManifest -Raw | ConvertFrom-Json -Depth 30
$contentRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\content\tweaks'))
$resolvedOutput = [IO.Path]::GetFullPath($OutputRoot)
$expectedLeaf = Join-Path $contentRoot 'imported-registry'
if (-not $resolvedOutput.Equals([IO.Path]::GetFullPath($expectedLeaf), [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be the dedicated imported-registry directory under content/tweaks: $expectedLeaf"
}
if (Test-Path -LiteralPath $resolvedOutput) {
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

$generated = 0
foreach ($candidate in $manifest.candidates) {
    if ($candidate.decision -ne 'accepted' -or $candidate.object_type -notlike 'registry_*') { continue }

    if ($candidate.object_type -eq 'registry_tree') {
        $types = @($candidate.details.observed_types)
        if (-not ($types -contains 'create_tree' -and $types -contains 'delete_tree')) {
            Reject-Candidate $candidate 'tree-payload-unavailable'
            continue
        }
        Reject-Candidate $candidate 'tree-import-not-required'
        continue
    }

    $parts = @(([string]$candidate.normalized_object) -split '\|')
    if ($parts.Count -ne 3 -or $parts[0] -notmatch '^(?<hive>HK(?:CU|LM|CR|U))\\(?<key>.+)$') {
        Reject-Candidate $candidate 'invalid-registry-object'
        continue
    }
    $hive = $Matches.hive.ToUpperInvariant()
    $key = $Matches.key
    $view = $parts[1]
    $valueName = $parts[2]
    if ($hive -eq 'HKU' -and $key -match '^(?i)Default\\') {
        Reject-Candidate $candidate 'offline-user-hive'
        continue
    }
    if ($view -notin @('registry32', 'registry64')) {
        Reject-Candidate $candidate 'invalid-registry-view'
        continue
    }

    $nativeTypes = @($candidate.details.observed_types | Where-Object { $_ -ne 'delete' } | Sort-Object -Unique)
    if ($nativeTypes.Count -ne 1 -or $nativeTypes[0] -notin @('REG_DWORD', 'REG_SZ', 'REG_BINARY', 'REG_EXPAND_SZ')) {
        Reject-Candidate $candidate 'insufficient-typed-payload'
        continue
    }
    $values = [Collections.Generic.List[object]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $parseFailed = $false
    foreach ($rawValue in @($candidate.details.observed_values)) {
        $parsed = Convert-ObservedValue $nativeTypes[0] ([string]$rawValue)
        if ($null -eq $parsed) {
            $parseFailed = $true
            break
        }
        $identity = "$($parsed.Type)|$($parsed.Value)"
        if ($seen.Add($identity)) { $values.Add($parsed) }
    }
    if ($parseFailed -or $values.Count -eq 0) {
        Reject-Candidate $candidate 'unresolved-payload'
        continue
    }

    $verification = Get-Verification "$hive\$key"
    $candidate.details | Add-Member -NotePropertyName verification -NotePropertyValue $verification -Force
    $candidate.reason_code = $null
    $fileBase = ([string]$candidate.tweak_id) -replace '[^a-zA-Z0-9.-]', '-'
    if ($fileBase.Length -gt 70) { $fileBase = $fileBase.Substring(0, 70).TrimEnd('-') }
    Add-ValueYaml $candidate $hive $key $view $valueName $values.ToArray() (Join-Path $resolvedOutput "$fileBase.yaml")
    $generated++
}

$decisionSummary = [ordered]@{}
$manifest.candidates | Group-Object decision | Sort-Object Name | ForEach-Object {
    $decisionSummary[$_.Name] = $_.Count
}
$typeSummary = [ordered]@{}
$manifest.candidates | Group-Object object_type | Sort-Object Name | ForEach-Object {
    $typeSummary[$_.Name] = $_.Count
}
$manifest.summary.candidates = @($manifest.candidates).Count
$manifest.summary.decisions = [pscustomobject]$decisionSummary
$manifest.summary.object_types = [pscustomobject]$typeSummary
$manifest | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $AuditOutputPath -Encoding utf8NoBOM

Write-Output "Generated registry tweaks: $generated"
Write-Output "Audit manifest: $AuditOutputPath"
