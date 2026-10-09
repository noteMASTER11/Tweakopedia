[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]] $SourceRoots,

    [Parameter(Mandatory)]
    [string] $ExistingContentRoot,

    [Parameter(Mandatory)]
    [string] $OutputPath,

    [string] $WinaeroFeatureFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$candidates = [System.Collections.Generic.Dictionary[string, object]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$existingRegistry = [System.Collections.Generic.Dictionary[string, string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$existingFeatures = [System.Collections.Generic.Dictionary[string, string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$existingAppx = [System.Collections.Generic.Dictionary[string, string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)

function Get-StableHash([string] $Text, [int] $Length = 12) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text.ToLowerInvariant())
    $hash = [System.Security.Cryptography.SHA256]::HashData($bytes)
    return ([Convert]::ToHexString($hash).ToLowerInvariant()).Substring(0, $Length)
}

function ConvertTo-IdSlug([string] $Text) {
    $normalized = $Text.Normalize([Text.NormalizationForm]::FormD)
    $builder = [Text.StringBuilder]::new()
    foreach ($character in $normalized.ToCharArray()) {
        $category = [Globalization.CharUnicodeInfo]::GetUnicodeCategory($character)
        if ($category -eq [Globalization.UnicodeCategory]::NonSpacingMark) { continue }
        [void]$builder.Append($character)
    }
    $slug = $builder.ToString().ToLowerInvariant() -replace '[^a-z0-9]+', '-'
    $slug = $slug.Trim('-')
    if ([string]::IsNullOrEmpty($slug)) { return 'candidate' }
    if ($slug.Length -gt 48) { $slug = $slug.Substring(0, 48).TrimEnd('-') }
    return $slug
}

function Normalize-RegistryPath([string] $RawPath) {
    $path = $RawPath.Trim().Trim('"', "'") -replace '/', '\'
    $path = $path -replace '^Registry::', ''
    $path = $path -replace '^HKLM:\\?', 'HKLM\'
    $path = $path -replace '^HKCU:\\?', 'HKCU\'
    $path = $path -replace '^HKCR:\\?', 'HKCR\'
    $path = $path -replace '^HKU:\\?', 'HKU\'
    $path = $path -replace '^HKEY_LOCAL_MACHINE\\', 'HKLM\'
    $path = $path -replace '^HKEY_CURRENT_USER\\', 'HKCU\'
    $path = $path -replace '^HKEY_CLASSES_ROOT\\', 'HKCR\'
    $path = $path -replace '^HKEY_USERS\\', 'HKU\'
    $path = $path -replace '(?i)\\ControlSet00[1-9]\\', '\CurrentControlSet\'
    $path = $path -replace '\\+', '\'
    return $path.TrimEnd('\')
}

function Get-RegistryViewAndPath([string] $RawPath) {
    $path = Normalize-RegistryPath $RawPath
    $view = 'registry64'
    if ($path -match '(?i)^(HKLM|HKCU)\\SOFTWARE\\WOW6432Node\\(?<tail>.+)$') {
        $path = "$($Matches[1])\SOFTWARE\$($Matches['tail'])"
        $view = 'registry32'
    }
    return [pscustomobject]@{ Path = $path; View = $view }
}

function Get-RegistryObject([string] $RawPath, [string] $ValueName) {
    $normalized = Get-RegistryViewAndPath $RawPath
    $name = $ValueName.Trim().Trim('"', "'")
    if ($name -eq '@' -or [string]::IsNullOrEmpty($name)) { $name = '(default)' }
    return "$($normalized.Path)|$($normalized.View)|$name"
}

function Get-CandidateKey([string] $ObjectType, [string] $NormalizedObject) {
    return ($ObjectType.ToLowerInvariant() + '|' + $NormalizedObject.ToLowerInvariant())
}

function Add-Candidate {
    param(
        [Parameter(Mandatory)][string] $ObjectType,
        [Parameter(Mandatory)][string] $NormalizedObject,
        [Parameter(Mandatory)][string] $Source,
        [string] $Title,
        [string] $ObservedType,
        [AllowEmptyString()][string] $ObservedValue
    )

    if ([string]::IsNullOrWhiteSpace($NormalizedObject)) { return }
    $key = Get-CandidateKey $ObjectType $NormalizedObject
    if (!$candidates.ContainsKey($key)) {
        $candidates[$key] = [ordered]@{
            candidate_key = $key
            object_type = $ObjectType
            normalized_object = $NormalizedObject
            decision = $null
            tweak_id = $null
            reason_code = $null
            details = [ordered]@{
                titles = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                sources = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                observed_types = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                observed_values = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                occurrence_count = 0
            }
        }
    }
    $candidate = $candidates[$key]
    $candidate.details.occurrence_count++
    if ($candidate.details.sources.Count -lt 12) { [void]$candidate.details.sources.Add($Source) }
    if ($Title) { [void]$candidate.details.titles.Add($Title.Trim()) }
    if ($ObservedType) { [void]$candidate.details.observed_types.Add($ObservedType.Trim()) }
    if (($PSBoundParameters.ContainsKey('ObservedValue')) -and
        $candidate.details.observed_values.Count -lt 24) {
        [void]$candidate.details.observed_values.Add($ObservedValue.Trim())
    }
}

function Get-YamlScalar([string] $Text, [string] $Name) {
    $match = [regex]::Match($Text, "(?m)^\s*$([regex]::Escape($Name)):\s*(?<value>[^\r\n]+)")
    if (!$match.Success) { return $null }
    return $match.Groups['value'].Value.Trim().Trim('"', "'")
}

function Read-ExistingCatalog([string] $ContentRoot) {
    $tweakRoot = Join-Path $ContentRoot 'tweaks'
    foreach ($file in Get-ChildItem -LiteralPath $tweakRoot -Recurse -File -Filter '*.yaml') {
        if ($file.FullName.StartsWith(
                [IO.Path]::GetFullPath((Join-Path $tweakRoot 'imported-registry')),
                [StringComparison]::OrdinalIgnoreCase)) { continue }
        $text = Get-Content -LiteralPath $file.FullName -Raw
        $id = Get-YamlScalar $text 'id'
        if (!$id) { continue }

        $featureMatches = [regex]::Matches($text, '(?m)^\s*feature_id:\s*(?<id>\d+)\s*$')
        foreach ($match in $featureMatches) {
            $featureId = $match.Groups['id'].Value
            if (!$existingFeatures.ContainsKey($featureId)) { $existingFeatures[$featureId] = $id }
        }

        $registryMatches = [regex]::Matches(
            $text,
            '(?ms)\bhive:\s*[''"]?(?<hive>HKLM|HKCU|HKCR|HKU)[''"]?\s*(?:,|\r?\n).*?\bkey:\s*[''"](?<key>[^''"]+)[''"]\s*(?:,|\r?\n).*?\bvalue_name:\s*[''"]?(?<name>[^,}\]\r\n]+)')
        foreach ($match in $registryMatches) {
            $object = Get-RegistryObject "$($match.Groups['hive'].Value)\$($match.Groups['key'].Value)" $match.Groups['name'].Value
            if (!$existingRegistry.ContainsKey($object)) { $existingRegistry[$object] = $id }
        }
    }

    $appsPath = Join-Path $ContentRoot 'apps.json'
    if (Test-Path -LiteralPath $appsPath) {
        $apps = Get-Content -LiteralPath $appsPath -Raw | ConvertFrom-Json
        foreach ($app in $apps.Apps) {
            if ($app.RemovalMethod -ne 'Appx') { continue }
            $slug = ($app.AppId.ToLowerInvariant() -replace '[^a-z0-9.]', '-') -replace '-+', '-'
            $slug = $slug -replace '\.+', '.'
            $existingAppx[$app.AppId] = "apps.remove.$slug"
        }
    }
}

function Get-SourceLabel([string] $Root, [string] $File) {
    $rootName = Split-Path $Root -Leaf
    $relative = [IO.Path]::GetRelativePath($Root, $File)
    return "$rootName/$($relative -replace '\\', '/')"
}

function Read-RegFile([string] $Root, [IO.FileInfo] $File) {
    $source = Get-SourceLabel $Root $File.FullName
    $currentKey = $null
    $logicalLines = [System.Collections.Generic.List[string]]::new()
    $continued = ''
    foreach ($physicalLine in Get-Content -LiteralPath $File.FullName) {
        $part = $physicalLine.Trim()
        if ($part.EndsWith('\')) {
            $continued += $part.Substring(0, $part.Length - 1)
            continue
        }
        $logicalLines.Add($continued + $part)
        $continued = ''
    }
    if ($continued) { $logicalLines.Add($continued) }
    foreach ($line in $logicalLines) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(?<delete>-)?(?<path>HKEY_[^\]]+|HK(?:LM|CU|CR|U)\\[^\]]+)\]$') {
            $currentKey = Normalize-RegistryPath $Matches.path
            if ($Matches['delete']) {
                Add-Candidate -ObjectType 'registry_tree' -NormalizedObject $currentKey -Source $source `
                    -Title $File.BaseName -ObservedType 'delete_tree'
            }
            continue
        }
        if (!$currentKey) { continue }
        if ($trimmed -match '^(?<name>@|"(?:[^"\\]|\\.)*")\s*=\s*(?<data>.+)$') {
            $name = $Matches.name.Trim('"')
            $data = $Matches.data.Trim()
            $nativeType = if ($data -eq '-') { 'delete' }
                elseif ($data -match '^(?i)dword:') { 'REG_DWORD' }
                elseif ($data -match '^(?i)hex\(b\):') { 'REG_QWORD' }
                elseif ($data -match '^(?i)hex\(2\):') { 'REG_EXPAND_SZ' }
                elseif ($data -match '^(?i)hex\(7\):') { 'REG_MULTI_SZ' }
                elseif ($data -match '^(?i)hex(?:\([0-9a-f]+\))?:') { 'REG_BINARY' }
                else { 'REG_SZ' }
            Add-Candidate -ObjectType 'registry_value' `
                -NormalizedObject (Get-RegistryObject $currentKey $name) -Source $source `
                -Title $File.BaseName -ObservedType $nativeType -ObservedValue $data
        }
    }
}

function Resolve-ScriptToken([string] $Token, [hashtable] $Variables) {
    $value = $Token.Trim().Trim('"', "'")
    if ($value -match '^\$(?<name>[A-Za-z_][A-Za-z0-9_]*)$' -and $Variables.ContainsKey($Matches['name'])) {
        return $Variables[$Matches['name']]
    }
    foreach ($name in @($Variables.Keys | Sort-Object Length -Descending)) {
        $value = $value.Replace("`$$name", [string]$Variables[$name], [StringComparison]::OrdinalIgnoreCase)
    }
    return $value
}

function Read-ScriptFile([string] $Root, [IO.FileInfo] $File) {
    $source = Get-SourceLabel $Root $File.FullName
    $text = Get-Content -LiteralPath $File.FullName -Raw
    $variables = @{}
    foreach ($match in [regex]::Matches($text, '(?m)^\s*\$(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*["''](?<value>HK(?:LM|CU|CR|U):?\\[^"'']+)["'']\s*$')) {
        $variables[$match.Groups['name'].Value] = $match.Groups['value'].Value
    }

    foreach ($line in $text -split '\r?\n') {
        $trimmed = $line.Trim()
        if ($trimmed.StartsWith('#') -or [string]::IsNullOrWhiteSpace($trimmed)) { continue }

        if ($trimmed -match '(?i)(?:Set|New)-ItemProperty(?:Verified)?\b.*?-Path\s+(?<path>"[^"]+"|''[^'']+''|\$[A-Za-z_][A-Za-z0-9_]*).*?-Name\s+(?<name>"[^"]+"|''[^'']+''|[A-Za-z0-9_. -]+?)(?:\s+-Type\s+(?<type>\w+))?(?:\s+-Value\s+(?<value>[^\s;]+))?(?:\s|$)') {
            $path = Resolve-ScriptToken $Matches['path'] $variables
            $name = Resolve-ScriptToken $Matches['name'] $variables
            if ($path -match '^(?i)(HKLM|HKCU|HKCR|HKU)') {
                Add-Candidate -ObjectType 'registry_value' -NormalizedObject (Get-RegistryObject $path $name) `
                    -Source $source -Title $File.BaseName -ObservedType $Matches['type'] -ObservedValue $Matches['value']
            }
            continue
        }
        if ($trimmed -match '(?i)Remove-ItemProperty(?:Verified)?\b.*?-Path\s+(?<path>"[^"]+"|''[^'']+''|\$[A-Za-z_][A-Za-z0-9_]*).*?-Name\s+(?<name>"[^"]+"|''[^'']+''|[A-Za-z0-9_. -]+?)(?:\s|$)') {
            $path = Resolve-ScriptToken $Matches['path'] $variables
            $name = Resolve-ScriptToken $Matches['name'] $variables
            if ($path -match '^(?i)(HKLM|HKCU|HKCR|HKU)') {
                Add-Candidate -ObjectType 'registry_value' -NormalizedObject (Get-RegistryObject $path $name) `
                    -Source $source -Title $File.BaseName -ObservedType 'delete'
            }
            continue
        }
        if ($trimmed -match '(?i)\breg(?:\.exe)?\s+add\s+(?<path>"[^"]+"|''[^'']+''|\S+)(?<tail>.*)$') {
            $path = $Matches['path'].Trim('"', "'")
            $tail = $Matches['tail']
            $name = if ($tail -match '(?i)(?:^|\s)/v\s+(?<name>"[^"]*"|''[^'']*''|\S+)') {
                $Matches['name'].Trim('"', "'")
            } elseif ($tail -match '(?i)(?:^|\s)/ve(?:\s|$)') {
                '(default)'
            } else {
                $null
            }
            if ($name) {
                $nativeType = if ($tail -match '(?i)(?:^|\s)/t\s+(?<type>REG_[A-Z0-9_]+)') {
                    $Matches['type'].ToUpperInvariant()
                } else {
                    'REG_SZ'
                }
                $hasData = $tail -match '(?i)(?:^|\s)/d\s+(?<data>"[^"]*"|''[^'']*''|\S+)'
                $data = if ($hasData) { $Matches['data'].Trim('"', "'") } else { '' }
                Add-Candidate -ObjectType 'registry_value' `
                    -NormalizedObject (Get-RegistryObject $path $name) `
                    -Source $source -Title $File.BaseName -ObservedType $nativeType `
                    -ObservedValue $data
            }
            continue
        }
        if ($trimmed -match '(?i)\breg(?:\.exe)?\s+delete\s+(?<path>"[^"]+"|''[^'']+''|\S+)(?<tail>.*)$') {
            $path = $Matches['path'].Trim('"', "'")
            $tail = $Matches['tail']
            $name = if ($tail -match '(?i)(?:^|\s)/v\s+(?<name>"[^"]*"|''[^'']*''|\S+)') {
                $Matches['name'].Trim('"', "'")
            } elseif ($tail -match '(?i)(?:^|\s)/ve(?:\s|$)') {
                '(default)'
            } else {
                $null
            }
            if ($name) {
                Add-Candidate -ObjectType 'registry_value' `
                    -NormalizedObject (Get-RegistryObject $path $name) `
                    -Source $source -Title $File.BaseName -ObservedType 'delete'
            }
            continue
        }
        if ($trimmed -match '(?i)\b(?:sc(?:\.exe)?\s+(?:config|start|stop)|Set-Service|Set-ServiceStartup)\b(?<tail>.*)$') {
            $tail = $Matches['tail']
            $name = if ($tail -match '(?i)(?:-Name|-Services?)\s+["'']?(?<name>[A-Za-z0-9_.-]+)') { $Matches['name'] } else { $File.BaseName }
            Add-Candidate -ObjectType 'service' -NormalizedObject $name -Source $source -Title $File.BaseName -ObservedType 'service-management'
            continue
        }
        if ($trimmed -match '(?i)\bschtasks(?:\.exe)?\b.*?/TN\s+(?:"(?<double>[^"]+)"|''(?<single>[^'']+)''|(?<bare>\S+))\s+(?<tail>.*)$') {
            $task = @($Matches['double'], $Matches['single'], $Matches['bare']) |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
                Select-Object -First 1
            $task = $task.Trim()
            $tail = $Matches['tail']
            if ($tail -match '(?i)/(?<state>ENABLE|DISABLE)') {
                Add-Candidate -ObjectType 'scheduled_task' -NormalizedObject $task -Source $source `
                    -Title $File.BaseName -ObservedType $Matches['state']
            }
            continue
        }
        if ($trimmed -match '(?i)\bbcdedit(?:\.exe)?\s+/(?<verb>set|deletevalue)\s+(?<tail>[^|>&]+)') {
            $verb = $Matches['verb'].ToLowerInvariant()
            $tokens = @(($Matches['tail'].Trim() -split '\s+') |
                Where-Object { $_ } | ForEach-Object { $_ -replace '`', '' })
            $objectId = '{current}'
            if ($tokens.Count -gt 0 -and $tokens[0] -match '^`?\{(?<id>[A-Za-z0-9_-]+)\}`?$') {
                $objectId = "{$($Matches['id'].ToLowerInvariant())}"
                $tokens = @($tokens | Select-Object -Skip 1)
            }
            if ($tokens.Count -gt 0) {
                $element = $tokens[0].ToLowerInvariant()
                $value = if ($verb -eq 'set' -and $tokens.Count -gt 1) { $tokens[1] } else { '' }
                Add-Candidate -ObjectType 'bcd_element' -NormalizedObject "$objectId|$element" -Source $source `
                    -Title $File.BaseName -ObservedType $verb -ObservedValue $value
            }
            continue
        }
        if ($trimmed -match '(?i)\bpowercfg(?:\.exe)?\s+(?<verb>/(?:setacvalueindex|setdcvalueindex)|-(?:change|setactive))\s*(?<tail>.*)$') {
            $verb = $Matches['verb'].ToLowerInvariant()
            $tokens = @(($Matches['tail'].Trim() -split '\s+') | Where-Object { $_ })
            $value = ''
            if ($verb -in @('/setacvalueindex', '/setdcvalueindex') -and $tokens.Count -ge 4) {
                $object = "$verb|$($tokens[0].ToLowerInvariant())|$($tokens[1].ToLowerInvariant())|$($tokens[2].ToLowerInvariant())"
                $value = $tokens[3]
            } elseif ($verb -eq '-change' -and $tokens.Count -ge 2) {
                $object = "$verb|$($tokens[0].ToLowerInvariant())"
                $value = $tokens[1]
            } elseif ($verb -eq '-setactive' -and $tokens.Count -ge 1) {
                $object = "$verb|$($tokens[0].ToLowerInvariant())"
            } else {
                $object = ($verb + '|' + (($Matches['tail'] -replace '\s+', ' ').Trim().ToLowerInvariant()))
            }
            Add-Candidate -ObjectType 'power_setting' -NormalizedObject $object -Source $source `
                -Title $File.BaseName -ObservedType $verb -ObservedValue $value
            continue
        }
        if ($trimmed -match '(?i)\b(?<verb>Enable|Disable)-WindowsOptionalFeature\b.*?-FeatureName\s+["'']?(?<name>[A-Za-z0-9_.-]+)') {
            Add-Candidate -ObjectType 'windows_feature' -NormalizedObject $Matches['name'] -Source $source `
                -Title $File.BaseName -ObservedType $Matches['verb']
            continue
        }
        if ($trimmed -match '(?i)\b(?<verb>Enable|Disable)-WindowsFeature\s*\(?\s*["''](?<name>[A-Za-z0-9_.-]+)["'']') {
            Add-Candidate -ObjectType 'windows_feature' -NormalizedObject $Matches['name'] -Source $source `
                -Title $File.BaseName -ObservedType $Matches['verb']
            continue
        }
        if ($trimmed -match '(?i)\b(?<verb>Add|Remove)-WindowsCapability\b.*?-Name\s+["'']?(?<name>[A-Za-z0-9_.~-]+)') {
            Add-Candidate -ObjectType 'windows_capability' -NormalizedObject $Matches['name'] -Source $source `
                -Title $File.BaseName -ObservedType $Matches['verb']
            continue
        }
    }

    if ($text -match '(?im)^\s*(?:Copy-Item|Remove-Item|Move-Item|copy\s|del\s|erase\s)') {
        $operation = if ($text -match '(?i)cache|cookie|temp|history|journal|log') { 'cleanup-script' }
            elseif ($text -match '(?i)install|setup|download') { 'installation-script' }
            else { 'file-script' }
        Add-Candidate -ObjectType 'file_script' -NormalizedObject $source -Source $source `
            -Title $File.BaseName -ObservedType $operation
    }
}

function Read-AppCatalog([string] $Root, [IO.FileInfo] $File) {
    try { $document = Get-Content -LiteralPath $File.FullName -Raw | ConvertFrom-Json } catch { return }
    if (!$document.PSObject.Properties['Apps']) { return }
    $source = Get-SourceLabel $Root $File.FullName
    foreach ($app in $document.Apps) {
        if (!$app.AppId) { continue }
        Add-Candidate -ObjectType 'appx' -NormalizedObject ([string]$app.AppId) -Source $source `
            -Title ([string]$app.FriendlyName) -ObservedType ([string]$app.RemovalMethod) `
            -ObservedValue ([string]$app.Description)
    }
}

function Read-FeatureCatalog([string] $Root, [IO.FileInfo] $File) {
    try { $document = Get-Content -LiteralPath $File.FullName -Raw | ConvertFrom-Json } catch { return }
    if (!$document.PSObject.Properties['UiGroups']) { return }
    $source = Get-SourceLabel $Root $File.FullName
    foreach ($group in $document.UiGroups) {
        if (!$group.PSObject.Properties['GroupId']) { continue }
        $ids = if ($group.PSObject.Properties['Values']) {
            @($group.Values | ForEach-Object {
                if ($_.PSObject.Properties['FeatureIds']) { $_.FeatureIds }
            } | Sort-Object -Unique)
        } elseif ($group.PSObject.Properties['FeatureIds']) {
            @($group.FeatureIds | Sort-Object -Unique)
        } else {
            @()
        }
        $label = if ($group.PSObject.Properties['Label']) { [string]$group.Label } else { [string]$group.GroupId }
        Add-Candidate -ObjectType 'feature_configuration' -NormalizedObject ([string]$group.GroupId) `
            -Source $source -Title $label -ObservedType 'ViVe UI group' `
            -ObservedValue ($ids -join ',')
    }
}

function Read-WinaeroFeatures([string] $HtmlPath) {
    if ([string]::IsNullOrWhiteSpace($HtmlPath) -or !(Test-Path -LiteralPath $HtmlPath)) { return }
    $html = Get-Content -LiteralPath $HtmlPath -Raw
    foreach ($match in [regex]::Matches($html, '<span class="lwptoc_item_label">(?<title>.*?)</span>', 'IgnoreCase')) {
        $title = [Net.WebUtility]::HtmlDecode(($match.Groups['title'].Value -replace '<[^>]+>', '')).Trim()
        if (!$title) { continue }
        $object = 'winaero|' + (ConvertTo-IdSlug $title)
        Add-Candidate -ObjectType 'documentation_feature' -NormalizedObject $object `
            -Source 'Winaero/feature-list' -Title $title -ObservedType 'documented-feature'
    }
}

function Set-CandidateDecision([System.Collections.IDictionary] $Candidate) {
    $type = [string]$Candidate.object_type
    $object = [string]$Candidate.normalized_object
    $key = [string]$Candidate.candidate_key
    $hash = Get-StableHash $key 10
    $title = if ($Candidate.details.titles.Count) { @($Candidate.details.titles)[0] } else { $object }
    $slug = ConvertTo-IdSlug $title

    if ($type -eq 'registry_value') {
        if ($object -match '(?i)\\SYSTEM\\CurrentControlSet\\Services\\') {
            $Candidate.object_type = 'service'
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'service-management'; return
        }
        if ($existingRegistry.ContainsKey($object)) {
            $Candidate.decision = 'duplicate'; $Candidate.tweak_id = $existingRegistry[$object]; return
        }
        if ($object -match '(?i)\\SYSTEM\\CurrentControlSet\\Control\\Power\\PowerSettings\\') {
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'power-schema-metadata'; return
        }
        if ($object -match '[%$!]' -or $object -match '(?i)^HKLM\\NTUSER\\') {
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'unresolved-or-offline-path'; return
        }
        if ($object -match '(?i)^HKCU\\Environment\|registry(?:32|64)\|(?:TEMP|TMP)$' -and
            (@($Candidate.details.titles) -join ' ') -match '(?i)clear|cleanup|очист') {
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'transient-script-helper'; return
        }
        $category = if ($object -match '(?i)WindowsUpdate|DeliveryOptimization') { 'updates' }
            elseif ($object -match '(?i)Explorer|Taskbar|DWM|Themes|Search') { 'desktop' }
            elseif ($object -match '(?i)DataCollection|Privacy|Advertising|ConsentStore') { 'privacy' }
            elseif ($object -match '(?i)Tcpip|Network|Lanman|Wlan') { 'network' }
            elseif ($object -match '(?i)Power|Energy') { 'power' }
            elseif ($object -match '(?i)Device|Driver|Printers') { 'devices' }
            else { 'behavior' }
        $Candidate.decision = 'accepted'; $Candidate.tweak_id = "$category.$slug-$hash"; return
    }
    if ($type -eq 'registry_tree') {
        if ($object -match '(?i)\\SYSTEM\\CurrentControlSet\\Services(?:\\|$)') {
            $Candidate.object_type = 'service'
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'service-management'; return
        }
        $Candidate.decision = 'accepted'; $Candidate.tweak_id = "behavior.$slug-$hash"; return
    }
    if ($type -eq 'service') {
        $Candidate.decision = 'rejected'; $Candidate.reason_code = 'service-management'; return
    }
    if ($type -eq 'appx') {
        if ($existingAppx.ContainsKey($object)) {
            $Candidate.decision = 'duplicate'; $Candidate.tweak_id = $existingAppx[$object]; return
        }
        if ($Candidate.details.observed_types.Contains('Appx')) {
            $Candidate.decision = 'accepted'; $Candidate.tweak_id = "apps.remove.$slug-$hash"; return
        }
        $Candidate.decision = 'rejected'; $Candidate.reason_code = 'unsupported-removal-method'; return
    }
    if ($type -eq 'feature_configuration') {
        $knownIds = @($Candidate.details.observed_values | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
        $existingId = $knownIds | Where-Object { $existingFeatures.ContainsKey($_) } | Select-Object -First 1
        if ($existingId) {
            $Candidate.decision = 'duplicate'; $Candidate.tweak_id = $existingFeatures[$existingId]; return
        }
        $Candidate.decision = 'accepted'; $Candidate.tweak_id = "experimental.$slug-$hash"; return
    }
    if ($type -eq 'documentation_feature') {
        if ($title -match '(?i)^(Bookmarks|Information|Reset App Defaults|Export/Import Tweaks|Get Classic Apps|Download)') {
            $Candidate.decision = 'rejected'; $Candidate.reason_code = 'not-an-independent-setting'; return
        }
        $Candidate.decision = 'accepted'; $Candidate.tweak_id = "behavior.winaero-$slug-$hash"; return
    }
    if ($type -eq 'file_script') {
        $observed = @($Candidate.details.observed_types) -join ','
        $Candidate.decision = 'rejected'
        $Candidate.reason_code = if ($observed -match 'cleanup') { 'data-cleanup' }
            elseif ($observed -match 'installation') { 'third-party-installation' }
            else { 'arbitrary-script' }
        return
    }
    if ($type -in @('scheduled_task', 'bcd_element', 'power_setting', 'windows_feature', 'windows_capability')) {
        $prefix = switch ($type) {
            'scheduled_task' { 'behavior' }
            'bcd_element' { 'boot' }
            'power_setting' { 'power' }
            'windows_feature' { 'apps' }
            'windows_capability' { 'apps' }
        }
        $Candidate.decision = 'accepted'; $Candidate.tweak_id = "$prefix.$slug-$hash"; return
    }
    $Candidate.decision = 'rejected'; $Candidate.reason_code = 'unsupported-object-type'
}

Read-ExistingCatalog $ExistingContentRoot

$resolvedRoots = @()
foreach ($root in $SourceRoots) {
    if (!(Test-Path -LiteralPath $root -PathType Container)) {
        throw "Source root does not exist: $root"
    }
    $resolvedRoots += (Resolve-Path -LiteralPath $root).Path
}

foreach ($root in $resolvedRoots) {
    foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $relativePath = [IO.Path]::GetRelativePath($root, $file.FullName)
        if ($relativePath -match '(?i)(^|\\)(tests?|\.github)(\\|$)') { continue }
        switch ($file.Extension.ToLowerInvariant()) {
            '.reg' { Read-RegFile $root $file; break }
            '.ps1' { Read-ScriptFile $root $file; break }
            '.psm1' { Read-ScriptFile $root $file; break }
            '.cmd' { Read-ScriptFile $root $file; break }
            '.bat' { Read-ScriptFile $root $file; break }
            '.json' {
                if ($file.Name -eq 'Apps.json' -and $file.FullName -notmatch '\\Languages\\') {
                    Read-AppCatalog $root $file
                } elseif ($file.Name -eq 'Features.json' -and $file.FullName -notmatch '\\Languages\\') {
                    Read-FeatureCatalog $root $file
                }
                break
            }
        }
    }
}
Read-WinaeroFeatures $WinaeroFeatureFile

$outputCandidates = foreach ($candidate in $candidates.Values) {
    Set-CandidateDecision $candidate
    $details = [ordered]@{
        titles = @($candidate.details.titles | Sort-Object)
        sources = @($candidate.details.sources | Sort-Object)
        observed_types = @($candidate.details.observed_types | Sort-Object)
        observed_values = @($candidate.details.observed_values | Sort-Object)
        occurrence_count = $candidate.details.occurrence_count
    }
    [ordered]@{
        candidate_key = $candidate.candidate_key
        object_type = $candidate.object_type
        normalized_object = $candidate.normalized_object
        decision = $candidate.decision
        tweak_id = $candidate.tweak_id
        reason_code = $candidate.reason_code
        details = $details
    }
}

$outputCandidates = @($outputCandidates | Sort-Object candidate_key)
$decisionCounts = @{}
$typeCounts = @{}
foreach ($candidate in $outputCandidates) {
    $decisionCounts[$candidate.decision] = 1 + [int]($decisionCounts[$candidate.decision] ?? 0)
    $typeCounts[$candidate.object_type] = 1 + [int]($typeCounts[$candidate.object_type] ?? 0)
}

$baselineCatalogCount = @(Get-ChildItem -LiteralPath (Join-Path $ExistingContentRoot 'tweaks') `
    -Recurse -File -Filter '*.yaml' | Where-Object {
        -not $_.FullName.StartsWith(
            [IO.Path]::GetFullPath((Join-Path $ExistingContentRoot 'tweaks\imported-registry')),
            [StringComparison]::OrdinalIgnoreCase)
    }).Count

$manifest = [ordered]@{
    schema = 'tweakopedia.complete-import-audit/1'
    source_roots = @($resolvedRoots)
    existing_catalog_count = $baselineCatalogCount
    summary = [ordered]@{
        candidates = $outputCandidates.Count
        decisions = $decisionCounts
        object_types = $typeCounts
    }
    candidates = $outputCandidates
}

$parent = Split-Path $OutputPath -Parent
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$json = $manifest | ConvertTo-Json -Depth 12
[IO.File]::WriteAllText($OutputPath, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Output "Inventory manifest: $OutputPath"
Write-Output "Candidates: $($outputCandidates.Count)"
$decisionCounts.GetEnumerator() | Sort-Object Name | ForEach-Object { Write-Output "$($_.Name): $($_.Value)" }
