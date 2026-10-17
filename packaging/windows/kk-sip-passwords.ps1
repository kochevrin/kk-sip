<#
.SYNOPSIS
    Puts each user's SIP password into a file only that user can read, for the
    "Password file" field of the kk-sip account policy (docs/provisioning.md).

.DESCRIPTION
    Run it on the file server, as an administrator. The first run creates the folder and
    shares it; every run writes one file per user, <login>.txt, readable by that user,
    the administrators and SYSTEM only. Run it again for new users or changed passwords;
    to take a password away, delete the user's file.

    In the account policy, set the password file to \\<this server>\kk-sip$\%USERNAME%.txt.

.PARAMETER Folder
    Local folder for the files, for example D:\kk-sip.

.PARAMETER Csv
    Logins and passwords, a header line and then one user per line:
        Login,Password
        ivan.petrov,s3cret
    Excel's ";" works too. Delete the file afterwards: the passwords are in it.

.PARAMETER Login
    One user instead of a CSV file; the script asks for the password.

.PARAMETER Domain
    The users' domain (NetBIOS name). Default: the domain of the one running the script.

.PARAMETER Share
    Name of the share. Default: kk-sip$ (the $ hides it from network browsing).

.EXAMPLE
    .\kk-sip-passwords.ps1 -Folder D:\kk-sip -Csv .\passwords.csv

.EXAMPLE
    .\kk-sip-passwords.ps1 -Folder D:\kk-sip -Login ivan.petrov
#>
param(
    [Parameter(Mandatory = $true)] [string] $Folder,
    [string] $Csv,
    [string] $Login,
    [string] $Domain = $env:USERDOMAIN,
    [string] $Share = 'kk-sip$'
)
$ErrorActionPreference = 'Stop'

if (-not $Csv -and -not $Login) { throw 'Give -Csv <file> or -Login <name>.' }

function Get-AccountName([string] $sid) {
    (New-Object Security.Principal.SecurityIdentifier $sid).Translate([Security.Principal.NTAccount]).Value
}

# The folder: administrators and SYSTEM only. Users may open a file in it (traverse) but
# not list it; each file then lets in its own user.
if (-not (Test-Path -LiteralPath $Folder)) { New-Item -ItemType Directory -Path $Folder | Out-Null }
$Folder = (Resolve-Path -LiteralPath $Folder).ProviderPath # .NET below does not know PowerShell's current folder
icacls $Folder /inheritance:r /grant:r '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-18:(OI)(CI)F' '*S-1-5-11:(X,RA)' | Out-Null
if ($LASTEXITCODE) { throw "icacls could not set the permissions of $Folder" }

# Readable over the network by every domain user; the files' own permissions decide who
# reads what.
if ($Share -and -not (Get-SmbShare -Name $Share -ErrorAction SilentlyContinue)) {
    New-SmbShare -Name $Share -Path $Folder -FolderEnumerationMode AccessBased `
        -ReadAccess (Get-AccountName 'S-1-5-11') -FullAccess (Get-AccountName 'S-1-5-32-544') | Out-Null
    Write-Host "Shared $Folder as \\$env:COMPUTERNAME\$Share"
}

function Set-Password([string] $user, [string] $password) {
    if ($user -notmatch '^[^\\/:*?"<>|]+$' -or $user -match '^\.+$') {
        Write-Warning "Skipped '$user': not a login"
        return
    }
    if (-not $password) {
        Write-Warning "Skipped ${user}: no password"
        return
    }
    $file = Join-Path $Folder "$user.txt"
    # A new file gets the folder's permissions (administrators and SYSTEM) before it holds
    # anything; then its user is let in. UTF-8 without a byte order mark.
    [IO.File]::WriteAllText($file, $password)
    icacls $file /grant "${Domain}\${user}:R" | Out-Null
    if ($LASTEXITCODE) {
        Remove-Item -LiteralPath $file
        Write-Warning "Skipped ${user}: no such user in $Domain"
        return
    }
    Write-Host "$user.txt"
}

if ($Login) {
    $secure = Read-Host "SIP password for $Login" -AsSecureString
    Set-Password $Login (New-Object Net.NetworkCredential '', $secure).Password
}
if ($Csv) {
    $first = Get-Content -LiteralPath $Csv -TotalCount 1
    $delimiter = if ($first -match ';' -and $first -notmatch ',') { ';' } else { ',' }
    foreach ($row in Import-Csv -LiteralPath $Csv -Delimiter $delimiter) {
        Set-Password ([string] $row.Login).Trim() ([string] $row.Password)
    }
    Write-Host "Done. Delete $Csv now: the passwords are in it."
}
