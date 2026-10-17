# Makes dist\kk-sip-<version>-windows-x64.msi from dist\stage (build-windows.sh)
# and dist\kk-sip-<version>-policy-templates.zip with the ADMX/ADML files, plus
# dist\kk-sip-passwords.ps1 for password files (docs/provisioning.md).
# Needs the .NET SDK; installs the WiX 5 command line tool if missing.
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\..\.."
$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(kk-sip VERSION ([0-9.]+)').Matches[0].Groups[1].Value

if (-not (Get-Command wix -ErrorAction SilentlyContinue)) {
    dotnet tool install --global wix --version 5.0.2
    $env:PATH += ";$env:USERPROFILE\.dotnet\tools"
}

$msi = "$root\dist\kk-sip-$version-windows-x64.msi"
wix build "$root\packaging\windows\kk-sip.wxs" -arch x64 `
    -d "Version=$version" -d "IconFile=$root\resources\icons\kk-sip.ico" `
    -bindpath "Stage=$root\dist\stage" -o $msi
if ($LASTEXITCODE -ne 0) { throw "wix build failed" }
Remove-Item "$root\dist\*.wixpdb" -ErrorAction SilentlyContinue
Write-Host "MSI: $msi"

# Group Policy templates (docs/provisioning.md), released next to the MSI.
$admx = "$root\dist\kk-sip-$version-policy-templates.zip"
Compress-Archive -Path "$root\packaging\windows\admx\*" -DestinationPath $admx -Force
Write-Host "ADMX: $admx"

# Released as it is; it never runs here, so at least make sure it parses.
$passwords = "$root\packaging\windows\kk-sip-passwords.ps1"
$errors = $null
[Management.Automation.Language.Parser]::ParseFile($passwords, [ref]$null, [ref]$errors) | Out-Null
if ($errors) { throw "kk-sip-passwords.ps1: $($errors[0])" }
Copy-Item $passwords "$root\dist\kk-sip-passwords.ps1"
