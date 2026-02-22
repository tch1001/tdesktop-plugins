$luaDir = 'C:\Users\tanch\Documents\tdesktop\Telegram\ThirdParty\lua'
$tempFile = "$env:TEMP\lua54.zip"
$tempExtract = "$env:TEMP\lua54_extract"

# Download Lua 5.4 as zip from GitHub mirror or luabinaries
Write-Host "Downloading Lua 5.4.7 source zip..."
# Use the GitHub-hosted version
Invoke-WebRequest -Uri 'https://github.com/lua/lua/archive/refs/tags/v5.4.7.zip' -OutFile $tempFile -UseBasicParsing
Write-Host "Downloaded: $((Get-Item $tempFile).Length) bytes"

if (Test-Path $tempExtract) { Remove-Item $tempExtract -Recurse -Force }
New-Item -ItemType Directory -Path $tempExtract | Out-Null

Write-Host "Expanding zip..."
Expand-Archive -Path $tempFile -DestinationPath $tempExtract -Force
Write-Host "Done."

$children = Get-ChildItem $tempExtract
Write-Host "Children: $($children.Count)"
foreach ($c in $children) { Write-Host "  $($c.Name)" }

if ($children.Count -gt 0) {
    $srcDir = $children[0].FullName
    if (Test-Path $luaDir) { Remove-Item $luaDir -Recurse -Force }
    Move-Item $srcDir $luaDir
    Write-Host "Lua source ready at: $luaDir"
    Write-Host "Files:"
    Get-ChildItem $luaDir | Format-Table Name
}
