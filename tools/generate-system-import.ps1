[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $InputManifest,
    [Parameter(Mandatory)][string] $OutputRoot,
    [Parameter(Mandatory)][string] $AuditOutputPath
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

function Set-Verification([object] $Candidate, [string] $Verification) {
    $Candidate.details | Add-Member -NotePropertyName verification `
        -NotePropertyValue $Verification -Force
    $Candidate.reason_code = $null
}

function Get-FirstTitle([object] $Candidate, [string] $Fallback) {
    $titles = @($Candidate.details.titles | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($titles.Count -eq 0) { return $Fallback }
    return [string]$titles[0]
}

function Get-CatalogIdsByBaseName {
    $result = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
    $contentRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\content\tweaks'))
    foreach ($file in Get-ChildItem -LiteralPath $contentRoot -Recurse -File -Filter '*.yaml') {
        if ($file.FullName -match '[\\/]imported-(?:registry|system)[\\/]') { continue }
        $match = [regex]::Match((Get-Content -LiteralPath $file.FullName -Raw), '(?m)^id:\s*(?<id>[^\r\n]+)')
        if ($match.Success) {
            $result[$file.BaseName] = $match.Groups['id'].Value.Trim().Trim("'").Trim('"')
        }
    }
    return $result
}

function Resolve-DocumentationCandidates([object] $Manifest) {
    $catalog = Get-CatalogIdsByBaseName
    $duplicates = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
    $knownDuplicates = @{
        'Balloon Tooltips'='legacy-balloon-notifications'; 'Battery Flyout'='legacy-battery-flyout';
        "Cortana's Search Box On Top"='cortana-search-box-position'; 'Date & Time Pane'='legacy-clock-flyout';
        'Disable Action Center'='notification-center'; 'Disable Live Tiles'='live-tile-notifications';
        'Disable Quick Action Buttons'='quick-action-buttons'; 'Disable Web Search'='web-search';
        'Hover to Select for Virtual Desktop'='virtual-desktop-hover-selection';
        'Increase Taskbar Transparency Level'='taskbar-enhanced-transparency';
        'Action Center Always Open'='action-center-persistent'; 'Make Taskbar Opaque'='taskbar-transparency';
        'New Share Pane'='experimental-share-pane'; 'OneDrive Flyout Style'='onedrive-experimental-flyout';
        'Open Last Active Window'='last-active-taskbar-window';
        'Show Seconds on Taskbar Clock'='taskbar-clock-seconds';
        'Taskbar Button Flash Count'='taskbar-flash-count'; 'Taskbar Thumbnails'='taskbar-thumbnail-delay';
        'Wallpaper Quality'='wallpaper-jpeg-quality'; 'White Search Box'='cortana-white-search-box';
        'Windows Version on Desktop'='version-watermark'; 'Network Flyout'='network-flyout';
        'Verbose Logon Messages'='verbose-logon-messages'; 'Show Last Logon Info'='last-logon-info';
        'Power button on the Login Screen'='login-power-button'; 'Chkdsk Timeout at Boot'='chkdsk-timeout';
        'Disable Aero Shake'='disable-aero-shake'; 'Disable Aero Snap'='snap-assist';
        'Disable App Lookup in Store'='store-app-lookup'; 'Disable Automatic Maintenance'='automatic-maintenance';
        'Disable Downloads Blocking'='download-zone-information'; 'Disable Driver Updates'='exclude-driver-updates';
        'Disable Reboot After Updates'='prevent-auto-reboot-signed-in';
        'Enable Crash on Ctrl+Scroll Lock'='crash-on-ctrl-scroll-lock'; 'New Apps Notification'='new-app-notification';
        'Show BSOD, Disable Smiley'='bsod-details'; 'Disable Lock Screen'='lock-screen';
        'Enable CTRL + ALT + DEL'='require-ctrl-alt-delete'; 'Hide Last User Name'='hide-last-user-name';
        'Login Screen Image'='login-background-image'; 'Network icon on Lock Screen'='login-network-icon';
        'Disable UAC'='uac'; 'Enable UAC for Built-in Administrator'='uac-built-in-administrator';
        'Protection Against Unwanted Software'='defender-pua-protection';
        'Windows Defender Tray Icon'='defender-user-interface'; 'Auto-update Store apps'='store-auto-updates';
        'Disable Cortana'='cortana'; 'Disable Windows Ink Workspace'='windows-ink-workspace';
        'Start Screen Power Button'='start-power-options'; 'Disable Password Reveal Button'='password-reveal-button';
        'TCP/IP Router'='ip-routing'; 'Network Drives over UAC'='elevated-mapped-drives';
        'Encryption Context Menu'='explorer-encryption-context-menu';
        '"Do this for all current items" Checkbox'='explorer-confirm-all-items';
        'Compressed Overlay Icon'='explorer-compressed-encrypted-colors';
        'Drive Letters'='drive-letter-position'; 'File Explorer Starting Folder'='explorer-starting-folder';
        'Administrative Shares'='administrative-shares'; 'Colored Title Bars'='colored-title-bars';
        'Slow Down Animations'='slow-window-animations'; 'Startup Sound'='startup-sound'
    }
    $knownDuplicates.GetEnumerator() | ForEach-Object { $duplicates[$_.Key] = $_.Value }

    foreach ($candidate in $Manifest.candidates) {
        if ($candidate.decision -ne 'accepted' -or $candidate.object_type -ne 'documentation_feature') { continue }
        $title = Get-FirstTitle $candidate ''
        if ($duplicates.ContainsKey($title) -and $catalog.ContainsKey($duplicates[$title])) {
            $candidate.decision = 'duplicate'
            $candidate.tweak_id = $catalog[$duplicates[$title]]
            $candidate.reason_code = $null
        } else {
            Reject-Candidate $candidate 'documentation-payload-unresolved'
        }
    }

    $groupDuplicates = @{
        'CombineButtons'='taskbar-combine-buttons';
        'CombineMMButtons'='taskbar-multi-monitor-combine';
        'ExplorerLocation'='explorer-starting-folder';
        'DriveLetterPosition'='drive-letter-position';
        'ShowTabsInAltTab'='alt-tab-app-tabs';
        'StartAllAppsView'='start-all-apps-view-mode';
        'MultiMon'='taskbar-multi-monitor-apps';
        'SearchIcon'='taskbar-search-mode'
    }
    foreach ($candidate in $Manifest.candidates) {
        if ($candidate.decision -ne 'accepted' -or $candidate.object_type -ne 'feature_configuration') { continue }
        $group = [string]$candidate.normalized_object
        if ($groupDuplicates.ContainsKey($group) -and $catalog.ContainsKey($groupDuplicates[$group])) {
            $candidate.decision = 'duplicate'
            $candidate.tweak_id = $catalog[$groupDuplicates[$group]]
            $candidate.reason_code = $null
        } else {
            Reject-Candidate $candidate 'composite-ui-group'
        }
    }
}

function New-CommonHeader(
    [string] $Id, [string] $Title, [string] $Category, [string] $Subcategory,
    [string] $Summary, [string] $Mechanism, [string] $Effect,
    [string] $Tradeoffs, [string] $Recommendation, [string] $TechnicalDetails,
    [uint32] $MinimumBuild = 10240
) {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("id: $Id")
    $lines.Add("title: $(Quote-Yaml $Title)")
    $lines.Add("category: $Category")
    $lines.Add("subcategory: $Subcategory")
    $lines.Add('kind: setting')
    $lines.Add("summary: $(Quote-Yaml $Summary)")
    $lines.Add('explanation:')
    $lines.Add("  purpose: $(Quote-Yaml $Summary)")
    $lines.Add("  mechanism: $(Quote-Yaml $Mechanism)")
    $lines.Add("  effect: $(Quote-Yaml $Effect)")
    $lines.Add("  tradeoffs: $(Quote-Yaml $Tradeoffs)")
    $lines.Add("  recommendation: $(Quote-Yaml $Recommendation)")
    $lines.Add("  technical_details: $(Quote-Yaml $TechnicalDetails)")
    $lines.Add('compatibility:')
    $lines.Add('  architectures: [x64]')
    $lines.Add('  os: [windows10, windows11]')
    $lines.Add("  min_build: $MinimumBuild")
    return ,$lines
}

function Add-Footer([Collections.Generic.List[string]] $Lines, [string] $Restart, [string] $Impact) {
    $Lines.Add('dependencies: []')
    $Lines.Add('conflicts: []')
    $Lines.Add("restart: $Restart")
    $Lines.Add("impact: $Impact")
    $Lines.Add('reversibility: reversible')
    $Lines.Add('requires_network: false')
}

function Write-ScheduledTask([object] $Candidate, [string] $Destination) {
    $path = ([string]$Candidate.normalized_object).TrimStart('\')
    if ($path -match '[%$!]' -or [string]::IsNullOrWhiteSpace($path)) {
        Reject-Candidate $Candidate 'unresolved-task-name'; return $false
    }
    $lastSlash = $path.LastIndexOf('\')
    $folder = if ($lastSlash -ge 0) { '\' + $path.Substring(0, $lastSlash) } else { '\' }
    $name = if ($lastSlash -ge 0) { $path.Substring($lastSlash + 1) } else { $path }
    if ([string]::IsNullOrWhiteSpace($name)) { Reject-Candidate $Candidate 'unresolved-task-name'; return $false }
    $title = (Get-FirstTitle $Candidate 'Задача планировщика') + " — $name"
    $lines = New-CommonHeader $Candidate.tweak_id $title 'behavior' 'system-components' `
        "Управляет запуском задачи планировщика $name." `
        'Меняет только свойство Enabled существующей задачи через Task Scheduler API.' `
        'Выключенная задача не запускается по своим триггерам; включённая снова следует своему расписанию.' `
        'Если соответствующее приложение не установлено, параметр отображается как неподдерживаемый.' `
        'Изменяйте состояние только после проверки назначения задачи.' `
        "Задача: $folder\$name."
    $lines.Add('states:')
    foreach ($state in @(@('enabled','Включено','true'), @('disabled','Выключено','false'))) {
        $lines.Add("  $($state[0]):")
        $lines.Add("    title: $(Quote-Yaml $state[1])")
        $lines.Add('    operations:')
        $lines.Add('      - type: scheduled_task.set_enabled')
        $lines.Add("        folder: $(Quote-Yaml $folder)")
        $lines.Add("        name: $(Quote-Yaml $name)")
        $lines.Add("        enabled: $($state[2])")
    }
    $lines.Add('windows_defaults: []')
    $lines.Add('detect:')
    $lines.Add('  type: scheduled_task')
    $lines.Add("  folder: $(Quote-Yaml $folder)")
    $lines.Add("  name: $(Quote-Yaml $name)")
    $lines.Add('  enabled_state: enabled')
    $lines.Add('  disabled_state: disabled')
    Add-Footer $lines 'none' 'medium'
    Set-Content -LiteralPath $Destination -Value $lines -Encoding utf8NoBOM
    Set-Verification $Candidate 'fixed-task-scheduler-location'
    return $true
}

function Write-PowerSetting([object] $Candidate, [string] $Destination) {
    $object = [string]$Candidate.normalized_object
    if ($object -notmatch '^/(?<mode>setacvalueindex|setdcvalueindex)\|scheme_current\|(?<subgroup>[0-9a-f-]{36})\|(?<setting>[0-9a-f-]{36})$') {
        Reject-Candidate $Candidate 'unsupported-or-parameterized-power-command'; return $false
    }
    $mode = $Matches.mode; $subgroup = $Matches.subgroup; $setting = $Matches.setting
    if ($subgroup -eq '4f971e98-33f7-4e44-8701-4c8104130c33') {
        Reject-Candidate $Candidate 'invalid-power-subgroup-guid'; return $false
    }
    $source = if ($mode -eq 'setacvalueindex') { 'ac' } else { 'dc' }
    $indices = [Collections.Generic.List[uint32]]::new()
    foreach ($raw in @($Candidate.details.observed_values)) {
        [uint32]$index = 0
        if (-not [uint32]::TryParse([string]$raw, [ref]$index)) {
            Reject-Candidate $Candidate 'unresolved-power-index'; return $false
        }
        if (-not $indices.Contains($index)) { $indices.Add($index) }
    }
    if ($indices.Count -eq 0) { Reject-Candidate $Candidate 'unresolved-power-index'; return $false }
    $title = (Get-FirstTitle $Candidate 'Параметр питания') + " — $setting ($($source.ToUpperInvariant()))"
    $lines = New-CommonHeader $Candidate.tweak_id $title 'power' 'advanced-platform' `
        'Управляет одним индексом активной схемы питания.' `
        'PowrProf записывает фиксированное значение отдельно для питания от сети или батареи.' `
        'Новое значение начинает участвовать в работе активной схемы питания.' `
        'Эффект зависит от поддержки GUID прошивкой, драйвером и текущим оборудованием.' `
        'Если назначение GUID не отображается системой, оставьте текущее значение.' `
        "Активная схема; subgroup $subgroup; setting $setting; source $source."
    $lines.Add('states:')
    for ($i=0; $i -lt $indices.Count; $i++) {
        $lines.Add("  index_${i}:")
        $lines.Add("    title: 'Индекс $($indices[$i])'")
        $lines.Add('    operations:')
        $lines.Add('      - type: power.set_index')
        $lines.Add('        scheme: active')
        $lines.Add("        subgroup: $subgroup")
        $lines.Add("        setting: $setting")
        $lines.Add("        source: $source")
        $lines.Add("        index: $($indices[$i])")
    }
    $lines.Add('windows_defaults: []')
    $lines.Add('detect:')
    $lines.Add('  type: power.setting')
    $lines.Add('  scheme: active')
    $lines.Add("  subgroup: $subgroup")
    $lines.Add("  setting: $setting")
    $lines.Add("  source: $source")
    $lines.Add('  states:')
    for ($i=0; $i -lt $indices.Count; $i++) { $lines.Add("    $($indices[$i]): index_${i}") }
    Add-Footer $lines 'none' 'medium'
    Set-Content -LiteralPath $Destination -Value $lines -Encoding utf8NoBOM
    Set-Verification $Candidate 'windows-power-setting-guid'
    return $true
}

function Write-BcdElement([object] $Candidate, [string] $Destination) {
    $object = [string]$Candidate.normalized_object
    $definition = switch ($object.ToLowerInvariant()) {
        '{current}|disabledynamictick' { [pscustomobject]@{ Type='0x260000A5'; Kind='boolean'; Title='Динамический системный таймер'; Missing='default'; States=@(@('default','По умолчанию','delete',''),@('disabled','Динамический таймер выключен','set','true')) } }
        '{current}|nx' { [pscustomobject]@{ Type='0x25000020'; Kind='integer'; Title='Политика предотвращения выполнения данных'; Missing='opt_in'; States=@(@('opt_in','Только компоненты Windows','set','0'),@('always_on','Всегда включено','set','3')) } }
        '{current}|bootmenupolicy' { [pscustomobject]@{ Type='0x250000C2'; Kind='integer'; Title='Стиль меню загрузки'; Missing='standard'; States=@(@('legacy','Классическое меню','set','0'),@('standard','Стандартное меню','set','1')) } }
        default { $null }
    }
    if ($null -eq $definition) { Reject-Candidate $Candidate 'unsupported-bcd-element'; return $false }
    $lines = New-CommonHeader $Candidate.tweak_id $definition.Title 'boot' 'diagnostics' `
        "Управляет параметром загрузчика $object." `
        'Значение записывается типизированно через BCD WMI Provider в текущую загрузочную запись.' `
        'Выбранная политика применяется при следующей загрузке Windows.' `
        'Некорректная политика загрузки может изменить поведение старта системы.' `
        'Используйте штатный вариант, если нет конкретной причины менять загрузчик.' `
        "Объект {current}; element type $($definition.Type); kind $($definition.Kind)."
    $lines.Add('states:')
    foreach ($state in $definition.States) {
        $lines.Add("  $($state[0]):")
        $lines.Add("    title: $(Quote-Yaml $state[1])")
        $lines.Add('    operations:')
        $operation = if ($state[2] -eq 'delete') { 'bcd.delete_element' } else { 'bcd.set_element' }
        $lines.Add("      - type: $operation")
        $lines.Add("        object_id: '{current}'")
        $lines.Add("        element_type: $($definition.Type)")
        $lines.Add("        value_kind: $($definition.Kind)")
        if ($state[2] -eq 'set') { $lines.Add("        value: $($state[3])") }
    }
    $lines.Add('windows_defaults: []')
    $lines.Add('detect:')
    $lines.Add('  type: bcd.element')
    $lines.Add("  object_id: '{current}'")
    $lines.Add("  element_type: $($definition.Type)")
    $lines.Add("  value_kind: $($definition.Kind)")
    $lines.Add('  states:')
    foreach ($state in $definition.States | Where-Object { $_[2] -eq 'set' }) {
        $lines.Add("    $($state[0]): $($state[3])")
    }
    $lines.Add("  missing_state: $($definition.Missing)")
    Add-Footer $lines 'reboot' 'high'
    Set-Content -LiteralPath $Destination -Value $lines -Encoding utf8NoBOM
    Set-Verification $Candidate 'microsoft-bcd-element-enumeration'
    return $true
}

function Write-WindowsFeature([object] $Candidate, [string] $Destination) {
    $name = [string]$Candidate.normalized_object
    if ($name -notmatch '^[A-Za-z0-9_.-]+$') { Reject-Candidate $Candidate 'invalid-feature-name'; return $false }
    $title = switch ($name) {
        'Microsoft-Windows-Subsystem-Linux' { 'Подсистема Windows для Linux' }
        'VirtualMachinePlatform' { 'Платформа виртуальной машины' }
        'Containers-DisposableClientVM' { 'Песочница Windows' }
        default { "Дополнительный компонент $name" }
    }
    $minimumBuild = if ($name -eq 'Containers-DisposableClientVM') { 18362 } else { 16299 }
    $lines = New-CommonHeader $Candidate.tweak_id $title 'apps' 'optional-features' `
        "Включает или отключает дополнительный компонент Windows $name." `
        'Приложение передаёт DISM фиксированное имя компонента без произвольной командной строки.' `
        'После включения компонент становится доступен приложениям; после отключения его функции недоступны.' `
        'Изменение состава Windows может потребовать перезагрузки и дополнительного места на диске.' `
        'Включайте компонент только для сценариев, которым он требуется.' `
        "DISM Online FeatureName $name." $minimumBuild
    $lines.Add('states:')
    foreach ($state in @('disabled','enabled')) {
        $lines.Add("  ${state}:")
        $lines.Add("    title: $(if ($state -eq 'enabled') { "'Включено'" } else { "'Выключено'" })")
        $lines.Add('    operations:')
        $lines.Add('      - type: windows_feature.set_state')
        $lines.Add("        name: $name")
        $lines.Add("        state: $state")
    }
    $lines.Add('windows_defaults: []')
    $lines.Add('detect:')
    $lines.Add('  type: windows_component')
    $lines.Add('  component_kind: feature')
    $lines.Add("  name: $name")
    $lines.Add('  states:')
    $lines.Add('    disabled: disabled')
    $lines.Add('    absent: disabled')
    $lines.Add('    enabled: enabled')
    Add-Footer $lines 'reboot' 'medium'
    Set-Content -LiteralPath $Destination -Value $lines -Encoding utf8NoBOM
    Set-Verification $Candidate 'microsoft-optional-feature-name'
    return $true
}

$manifest = Get-Content -LiteralPath $InputManifest -Raw | ConvertFrom-Json -Depth 30
Resolve-DocumentationCandidates $manifest

$contentRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\content\tweaks'))
$resolvedOutput = [IO.Path]::GetFullPath($OutputRoot)
$expected = [IO.Path]::GetFullPath((Join-Path $contentRoot 'imported-system'))
if (-not $resolvedOutput.Equals($expected, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be $expected"
}
if (Test-Path -LiteralPath $resolvedOutput) { Remove-Item -LiteralPath $resolvedOutput -Recurse -Force }
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null

$generated = 0
foreach ($candidate in $manifest.candidates) {
    if ($candidate.decision -ne 'accepted') { continue }
    $type = [string]$candidate.object_type
    if ($type -notin @('scheduled_task','power_setting','bcd_element','windows_feature')) { continue }
    $fileBase = ([string]$candidate.tweak_id) -replace '[^a-zA-Z0-9.-]', '-'
    if ($fileBase.Length -gt 70) { $fileBase = $fileBase.Substring(0,70).TrimEnd('-') }
    $destination = Join-Path $resolvedOutput "$fileBase.yaml"
    $written = switch ($type) {
        'scheduled_task' { Write-ScheduledTask $candidate $destination }
        'power_setting' { Write-PowerSetting $candidate $destination }
        'bcd_element' { Write-BcdElement $candidate $destination }
        'windows_feature' { Write-WindowsFeature $candidate $destination }
    }
    if ($written) { $generated++ }
}

$decisionSummary = [ordered]@{}
$manifest.candidates | Group-Object decision | Sort-Object Name | ForEach-Object { $decisionSummary[$_.Name]=$_.Count }
$typeSummary = [ordered]@{}
$manifest.candidates | Group-Object object_type | Sort-Object Name | ForEach-Object { $typeSummary[$_.Name]=$_.Count }
$manifest.summary.candidates = @($manifest.candidates).Count
$manifest.summary.decisions = [pscustomobject]$decisionSummary
$manifest.summary.object_types = [pscustomobject]$typeSummary
$manifest | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $AuditOutputPath -Encoding utf8NoBOM
Write-Output "Generated system tweaks: $generated"
Write-Output "Audit manifest: $AuditOutputPath"
