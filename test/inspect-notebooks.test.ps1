$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('kobo-schema-' + [guid]::NewGuid())
$inspector = Join-Path $PSScriptRoot '../tools/inspect-notebooks.ps1'
$pwsh = (Get-Process -Id $PID).Path
try {
    New-Item -ItemType Directory -Path (Join-Path $fixture '.kobo') -Force | Out-Null
    $notebooks = New-Item -ItemType Directory -Path (Join-Path $fixture 'My Notebooks/nested') -Force
    $archive = [IO.Compression.ZipFile]::Open((Join-Path $notebooks.FullName 'PRIVATE_TITLE.nebo'), 'Create')
    try {
        $writer = [IO.StreamWriter]::new($archive.CreateEntry('meta.json').Open())
        try { $writer.Write('{"Application":"PRIVATE_VALUE","iink-user-metadata":{"kobo":{"device":"PRIVATE_DEVICE"}}}') } finally { $writer.Dispose() }
    } finally { $archive.Dispose() }
    $output = & $pwsh -NoProfile -File $inspector -Mount $fixture
    if ($LASTEXITCODE -ne 0) { throw 'Valid archive failed' }
    if (($output -join "`n") -match 'PRIVATE_') { throw 'Private metadata leaked' }
    $report = $output | ConvertFrom-Json
    if ($report.notebookCount -ne 1 -or $report.notebooks[0].koboFields[0].name -ne 'device') { throw 'Wrong schema' }
    [IO.File]::WriteAllText((Join-Path $notebooks.FullName 'PRIVATE_CORRUPT.nebo'), 'broken zip')
    $output = & $pwsh -NoProfile -File $inspector -Mount $fixture
    if ($LASTEXITCODE -ne 2) { throw 'Corrupt archive did not produce partial-failure status' }
    if (($output -join "`n") -match 'PRIVATE_') { throw 'Corrupt archive filename leaked' }
    $report = $output | ConvertFrom-Json
    if ($report.notebookCount -ne 2 -or @($report.notebooks | Where-Object status -eq 'ok').Count -ne 1) { throw 'Valid archive lost after partial failure' }
    Write-Output 'PASS: nested discovery, schema extraction, value privacy, corrupt archive isolation'
} finally {
    # Only remove the exact fixture created above, inside the OS temporary path.
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path $resolved -Leaf).StartsWith('kobo-schema-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
