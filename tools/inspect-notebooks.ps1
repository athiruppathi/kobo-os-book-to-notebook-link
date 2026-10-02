param(
    [Parameter(Mandatory = $true)][string]$Mount
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Host-side, read-only schema inspection. Never emit property values, file
# names, notebook titles, page identifiers, ink, or archive contents.
function Get-Fields($Object) {
    if ($null -eq $Object) { return @() }
    return @($Object.PSObject.Properties | ForEach-Object {
        [ordered]@{
            name = $_.Name
            type = if ($null -eq $_.Value) { 'null' } else { $_.Value.GetType().Name }
        }
    })
}

try {
    $root = (Resolve-Path -LiteralPath $Mount).Path
    if (-not (Test-Path -LiteralPath (Join-Path $root '.kobo') -PathType Container)) {
        throw 'Not a Kobo mount: .kobo directory is missing'
    }
    $directory = Join-Path $root 'My Notebooks'
    $results = @()
    if (Test-Path -LiteralPath $directory -PathType Container) {
        foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File -Filter '*.nebo') {
            $archive = $null
            $result = [ordered]@{ sample = $results.Count + 1; status = 'ok'; metadataFields = @(); koboFields = @() }
            try {
                $archive = [IO.Compression.ZipFile]::OpenRead($file.FullName)
                $entries = @($archive.Entries | Where-Object FullName -eq 'meta.json')
                if ($entries.Count -ne 1) { throw 'Expected one metadata entry' }
                if ($entries[0].Length -gt 1048576) { throw 'Metadata exceeds inspection limit' }
                $reader = [IO.StreamReader]::new($entries[0].Open())
                try { $metadata = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
                if ($metadata -isnot [PSCustomObject]) { throw 'Invalid metadata object' }
                $result.metadataFields = @(Get-Fields $metadata)
                $result.koboFields = @(Get-Fields $metadata.'iink-user-metadata'.kobo)
            } catch {
                # Exception messages can include personal filenames or raw JSON.
                $result.status = 'unreadable-or-unsupported'
            } finally {
                if ($null -ne $archive) { $archive.Dispose() }
            }
            $results += $result
        }
    }
    [ordered]@{ notebookCount = $results.Count; notebooks = $results } | ConvertTo-Json -Depth 8
    if (@($results | Where-Object status -ne 'ok').Count) { exit 2 }
} catch {
    [Console]::Error.WriteLine('Inspection failed. Check the mount path and read permissions.')
    exit 1
}
