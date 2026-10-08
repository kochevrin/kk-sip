# Makes dist\kk-sip-<version>-windows-x64.msi from dist\stage (build-windows.sh).
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
