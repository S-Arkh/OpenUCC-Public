param(
    [ValidatePattern('^[A-Za-z_][A-Za-z_0-9]*$')]
    [string]$Package = 'Interface',
    [string]$SourceDir = '',
    [switch]$Objects
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = [Guid]::NewGuid().ToString('N')
$logdir = Join-Path $root "Logs\$build"
$work = Join-Path $root "Work\$build"
$utf8 = New-Object System.Text.UTF8Encoding($false)
$pendingOutput = $null
$backupOutput = $null
$replacementSucceeded = $false

function Quote-NativeArgument([string]$value) {
    # Start-Process joins ArgumentList into one Windows command line.
    $escaped = $value -replace '(\\*)"', '$1$1\"'
    $escaped = $escaped -replace '(\\+)$', '$1$1'
    return '"' + $escaped + '"'
}

function Get-Sha256([string]$path) {
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    $stream = $null
    try {
        $stream = [System.IO.File]::OpenRead($path)
        return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '')
    } finally {
        if ($stream) { $stream.Dispose() }
        $hasher.Dispose()
    }
}

try {
    if ($Package -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$') { throw 'Reserved package name.' }
    New-Item -ItemType Directory -Path $logdir, $work -Force | Out-Null
    $entry = $null
    $manifestPath = Join-Path $root 'packages.json'
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $manifest = Get-Content -LiteralPath $manifestPath -Encoding UTF8 -Raw | ConvertFrom-Json
        $entry = $manifest.PSObject.Properties[$Package]
    }
    $mode = 'compile'
    if ($entry) { $entry = $entry.Value; $mode = ([string]$entry.mode).ToLowerInvariant() }
    if ($Objects) { $mode = 'compile-objects' }
    if ($mode -notin @('compile', 'compile-objects')) { throw "Invalid package mode: $mode" }
    if (!$SourceDir) {
        $packageSources = Join-Path $root "Packages\$Package\Classes"
        if ($entry -or (Test-Path -LiteralPath $packageSources -PathType Container)) {
            $SourceDir = $packageSources
        } else {
            $SourceDir = Join-Path $root 'Classes'
        }
    }
    if (!(Test-Path -LiteralPath $SourceDir -PathType Container)) { throw "Source directory not found: $SourceDir" }
    $SourceDir = (Resolve-Path -LiteralPath $SourceDir).Path
    $extension = '*.uc'
    if ($mode -eq 'compile-objects') { $extension = '*.uobj' }
    $sources = @(Get-ChildItem -LiteralPath $SourceDir -Recurse -File -Filter $extension)
    if (!$sources.Count) { throw "No $extension source files." }
    $core = Join-Path $root 'Dependencies\Core.u'
    $reference = Join-Path $root "References\$Package.u"
    if ($Package -ieq 'Core') { $core = $reference }
    if (!(Test-Path -LiteralPath $core -PathType Leaf)) { throw "Missing target Core.u: $core" }
    if ($mode -eq 'compile-objects' -and !(Test-Path -LiteralPath $reference -PathType Leaf)) {
        throw "Object package requires its reference: $reference"
    }
    $artifact = Join-Path $work "$Package.u"
    $arguments = @($mode, $core, $artifact, '--source-dir', $SourceDir, '--trace-progress')
    if (Test-Path -LiteralPath $reference -PathType Leaf) { $arguments += @('--reference', $reference) }
    $dependencyDir = Join-Path $root 'Dependencies'
    if ($entry) {
        $dependencies = @($entry.dependencies)
    } else {
        $dependencies = @(Get-ChildItem -LiteralPath $dependencyDir -Filter '*.u' -File |
            Where-Object { $_.BaseName -ine $Package -and $_.BaseName -ine 'Core' } |
            Sort-Object Name | ForEach-Object { $_.BaseName })
    }
    $seen = @{}
    foreach ($name in $dependencies) {
        if ($name -notmatch '^[A-Za-z_][A-Za-z_0-9]*$') { throw "Invalid dependency name: $name" }
        if ($name -ieq $Package -or $name -ieq 'Core') { continue }
        if ($seen.ContainsKey($name)) { throw "Duplicate dependency: $name" }
        $seen[$name] = $true
        $dependency = Join-Path $dependencyDir "$name.u"
        if (!(Test-Path -LiteralPath $dependency -PathType Leaf)) { throw "Missing dependency: $dependency" }
        $arguments += @('--dependency', $dependency)
    }
    $quoted = @($arguments | ForEach-Object { Quote-NativeArgument $_ })
    $process = Start-Process -FilePath (Join-Path $root 'bin\openucc.exe') -ArgumentList $quoted -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $logdir 'compiler.log') -RedirectStandardError (Join-Path $logdir 'stderr.log')
    if ($process.ExitCode -ne 0) { throw "Compiler failed ($($process.ExitCode)); see stderr.log." }
    if (!(Test-Path -LiteralPath $artifact -PathType Leaf)) { throw 'Compiler did not produce a package.' }
    $output = Join-Path $root 'Output'
    $archive = Join-Path $output $build
    New-Item -ItemType Directory -Path $archive -Force | Out-Null
    Copy-Item -LiteralPath $artifact -Destination $archive
    $candidateOutput = Join-Path $output ".$Package.$build.tmp"
    if (Test-Path -LiteralPath $candidateOutput) { throw "Temporary path already exists: $candidateOutput" }
    $pendingOutput = $candidateOutput
    Copy-Item -LiteralPath $artifact -Destination $pendingOutput
    $record = @{status='PASS'; package=$Package; mode=$mode; sources=$sources.Count; sha256=(Get-Sha256 $artifact)}
    [IO.File]::WriteAllText((Join-Path $logdir 'result.json'), ($record | ConvertTo-Json), $utf8)
    $latest = Join-Path $output "$Package.u"
    if (Test-Path -LiteralPath $latest -PathType Leaf) {
        # PowerShell 5.1 can turn a null backup path into an invalid empty string.
        $backupOutput = Join-Path $output ".$Package.$build.bak"
        if (Test-Path -LiteralPath $backupOutput) { throw "Backup path already exists: $backupOutput" }
        [IO.File]::Replace($pendingOutput, $latest, $backupOutput)
        $replacementSucceeded = $true
    } else {
        [IO.File]::Move($pendingOutput, $latest)
    }
    Write-Host "PASS: $Package ($($sources.Count) sources)"
    exit 0
} catch {
    New-Item -ItemType Directory -Path $logdir -Force | Out-Null
    $record = @{status='FAIL'; package=$Package; error=$_.Exception.Message}
    [IO.File]::WriteAllText((Join-Path $logdir 'result.json'), ($record | ConvertTo-Json), $utf8)
    Write-Host "FAIL: $($_.Exception.Message)"
    Write-Host 'Any existing Output package belongs to a previous successful build.'
    exit 1
} finally {
    if ($pendingOutput -and (Test-Path -LiteralPath $pendingOutput -PathType Leaf)) {
        Remove-Item -LiteralPath $pendingOutput -ErrorAction SilentlyContinue
    }
    if ($replacementSucceeded -and (Test-Path -LiteralPath $backupOutput -PathType Leaf)) {
        Remove-Item -LiteralPath $backupOutput -ErrorAction SilentlyContinue
    }
}
