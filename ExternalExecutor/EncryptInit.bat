@echo off
setlocal enabledelayedexpansion

echo === Init.lua Protection ===
echo.

set "SCRIPT_DIR=%~dp0"
set "INPUT_FILE=%SCRIPT_DIR%Init.lua"
set "OUTPUT_FILE=%SCRIPT_DIR%Init.lua.enc"
set "DECOY_FILE=%SCRIPT_DIR%Init.lua.decoy"

if not exist "%INPUT_FILE%" (
    echo Error: Init.lua not found!
    pause
    exit /b 1
)

echo Creating decoy file...
echo Hello CraveX skiddies :3> "%DECOY_FILE%"

echo Encrypting Init.lua with XOR...

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$key = [byte[]]@(0x4A, 0x75, 0x6C, 0x65, 0x73, 0x45, 0x78, 0x65); ^
$content = [System.IO.File]::ReadAllBytes('%INPUT_FILE%'); ^
$encrypted = New-Object byte[] $content.Length; ^
for ($i = 0; $i -lt $content.Length; $i++) { ^
    $encrypted[$i] = $content[$i] -bxor $key[$i %% $key.Length]; ^
}; ^
[System.IO.File]::WriteAllBytes('%OUTPUT_FILE%', $encrypted); ^
Write-Host 'Encryption complete!'"

echo.
echo === Files Created ===
echo  - Init.lua.decoy  (Resource Hacker sees this)
echo  - Init.lua.enc    (Encrypted real code)
echo.
echo Done! You can now rebuild the project.
echo.
pause
