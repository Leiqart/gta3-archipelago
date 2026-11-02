[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sourceRoot = $PSScriptRoot
$worldSource = Join-Path $sourceRoot 'gta3'
$manifestSource = Join-Path $worldSource 'archipelago.json'
$outputPath = Join-Path (Split-Path -Parent $sourceRoot) 'dist\gta3.apworld'
$stagingRoot = Join-Path ([IO.Path]::GetTempPath()) ('gta3_apworld_' + [guid]::NewGuid().ToString('N'))
$zipPath = Join-Path $stagingRoot 'gta3.zip'

try {
    New-Item -ItemType Directory -Path (Split-Path -Parent $outputPath) -Force | Out-Null
    New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::Open(
        $zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in '__init__.py', 'data.py', 'Options.py', 'Client.py', 'protocol.py') {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive,
                (Join-Path $worldSource $name),
                ('gta3/{0}' -f $name),
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive,
            $manifestSource,
            'gta3/archipelago.json',
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    } finally {
        $archive.Dispose()
    }
    Copy-Item -LiteralPath $zipPath -Destination $outputPath -Force

    Write-Host ('Built: {0}' -f $outputPath)
} finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        $resolvedStaging = [IO.Path]::GetFullPath($stagingRoot)
        $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if (-not $resolvedStaging.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove a staging directory outside the temporary directory.'
        }
        Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
    }
}
