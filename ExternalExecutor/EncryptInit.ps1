# XOR Encryption Tool for Init.lua Protection
# Run this script to generate the encrypted Init.lua resource

$XOR_KEY = [byte[]]@(0x4A, 0x75, 0x6C, 0x65, 0x73, 0x45, 0x78, 0x65)  # "JulesExe"

$scriptPath = Join-Path $PSScriptRoot "Init.lua"
$encryptedPath = Join-Path $PSScriptRoot "Init.lua.enc"
$decoyPath = Join-Path $PSScriptRoot "Init.lua.decoy"

if (-not (Test-Path $scriptPath)) {
    Write-Host "Error: Init.lua not found at $scriptPath" -ForegroundColor Red
    exit 1
}

$content = [System.IO.File]::ReadAllBytes($scriptPath)

$encrypted = New-Object byte[] $content.Length
for ($i = 0; $i -lt $content.Length; $i++) {
    $encrypted[$i] = $content[$i] -bxor $XOR_KEY[$i % $XOR_KEY.Length]
}

[System.IO.File]::WriteAllBytes($encryptedPath, $encrypted)

$decoyMessage = "Hello CraveX skiddies :3"
Set-Content -Path $decoyPath -Value $decoyMessage -NoNewline

Write-Host "=== Init.lua Protection ===" -ForegroundColor Cyan
Write-Host "1. Encrypted file created: $encryptedPath" -ForegroundColor Green
Write-Host "2. Decoy file created: $decoyPath" -ForegroundColor Green
Write-Host ""
Write-Host "=== Resource Setup ===" -ForegroundColor Yellow
Write-Host "In your .rc resource file:"
Write-Host "  1  RCDATA  Init.lua.decoy    (visible to Resource Hacker)"
Write-Host "  101 RCDATA Init.lua.enc      (encrypted real code)"
Write-Host ""
Write-Host "Done!" -ForegroundColor Green
