<#
.SYNOPSIS
    Installs (or removes) War Wind - Artificial Remaster into a War Wind game folder.

.DESCRIPTION
    Point it at the War Wind folder, or let it find the Steam install. It then:

      1. Checks the folder: WW.EXE present, not running, and the expected build (the bytes of
         the display-mode call at VA 0x4684C7 must read BB 08 00 00 00, found through the PE
         section table -- the same check the mod makes at runtime). WW.EXE is only ever READ.
      2. Backs up everything it will replace into <game>\_ArtificialRemaster_backup\
         (ddraw.dll, ddraw.ini, dsound.dll, WarWindHD.ini, WarWindHD\feature-hires_720.wwp,
         WarWindHD\fix-gdi-palette.wwp, WarWindHD\backdrop.png, WarWindHD\shaders\wwfx.hlsl), once, with a manifest, so -Uninstall can put the folder back
         exactly as it was.
      3. Installs cnc-ddraw (FunkyFr3sh, MIT) as ddraw.dll: this project's build, which adds a
         backdrop= image drawn around the 640x480 screens instead of black bars and the Direct3D 9
         effect passes behind Modern Graphics (source patches in third_party/cnc-ddraw). If that build cannot be had, the official cnc-ddraw release is
         used instead and the 640x480 screens get black bars. Then it writes the War Wind profile
         into ddraw.ini (other keys already in the file are kept): borderless window, integer
         scaling, the stone backdrop, Windows 95 version spoof, and no display-mode change.
      4. Installs the mod from this project's GitHub release: dsound.dll (a proxy that
         forwards to the system dsound.dll), WarWindHD.ini, WarWindHD\feature-hires_720.wwp,
         WarWindHD\fix-gdi-palette.wwp, WarWindHD\backdrop.png (the AI-upscaled stone art cnc-ddraw draws around 640x480 screens)
         and WarWindHD\shaders\wwfx.hlsl (the Modern Graphics shaders, compiled at start-up).
      5. Downloads the AI-upscaled cutscene archives and unpacks them into Data\VIDS_HD
         (about 700 MB unpacked; skip with -SkipCutscenes).

    Everything downloaded is cached (default: %LOCALAPPDATA%\WarWind-ArtificialRemaster\downloads);
    interrupted downloads resume. The large cutscene archives are deleted from the cache once
    unpacked unless -KeepDownloads is given.

    Nothing here needs administrator rights, unless the game lives under Program Files.

.PARAMETER GamePath
    The War Wind folder (the one holding WW.EXE), or WW.EXE itself. If omitted: the folder this
    script sits in if it holds WW.EXE, then the Steam install (app 1741140, found through the
    registry and libraryfolders.vdf), then a prompt.

.PARAMETER Uninstall
    Restore the files saved in _ArtificialRemaster_backup\ and remove what the installer added.
    The HD cutscenes are kept unless you say otherwise (or pass -RemoveCutscenes).

.PARAMETER RemoveCutscenes
    With -Uninstall: also delete Data\VIDS_HD without asking.

.PARAMETER SkipCutscenes
    Install the mod and cnc-ddraw only; do not download the HD cutscenes.

.PARAMETER Disable
    Mod features to switch off in WarWindHD.ini. Any of:
      GridHotkeys      command buttons use the key at their on-screen slot (QWERT/ASDFG/ZXCVB)
      CtrlGroups       Ctrl+digit assigns a control group
      EdgeScroll       moving the mouse to a screen edge scrolls the map
      WheelScroll      mouse wheel scrolls the map (Shift+wheel sideways)
      MiddleDrag       middle-button drag pans the map
      DoubleClickType  double-click a unit selects all visible units of that type
      ModernClicks     right click moves/attacks, left click only selects (modern RTS clicks)
      HiRes            missions run at 1280x720: full-width map over a bottom HUD
      PreciseClock     game ticks at the designed 16 per second instead of about 12
      SmoothMotion     units glide between game frames at 60 fps instead of stepping
      CommandGrid      the bottom HUD shows the selection's commands as a 5x3 hotkey grid
      InGameSaveLoad   save/load in an in-game screen instead of the Windows file dialogs
      WideMenus        widescreen menus (main menu, race options, briefing, victory/defeat, Esc menu)
      Modern           Modern Graphics: soft shadows, lights, animated water, soft fog of war, colour grade (F9)
      HDVideos         play the Data\VIDS_HD remasters instead of the original AVIs
    Everything not listed is switched on.

.PARAMETER Tag
    The mod release to install (e.g. v1.0.0-beta). Default: the newest published release,
    pre-releases included.

.PARAMETER LocalFiles
    A folder holding any of the pieces already downloaded; each is used instead of a download
    when found there by name: dsound.dll, ddraw.dll (this project's cnc-ddraw build, or any
    cnc-ddraw), WarWindHD.ini, feature-hires_720.wwp, fix-gdi-palette.wwp, backdrop.png, wwfx.hlsl, cnc-ddraw.zip (the official
    release, the fallback), WarWind-HD-Cutscenes-*.zip.

.PARAMETER Downloads
    Cache folder. Default: %LOCALAPPDATA%\WarWind-ArtificialRemaster\downloads.

.PARAMETER KeepDownloads
    Keep the cutscene archives in the cache after unpacking them.

.PARAMETER Force
    Overwrite existing HD cutscenes, rewrite ddraw.ini from the profile alone (dropping any
    other keys in it), and start WarWindHD.ini from the defaults instead of keeping your
    current settings. When only the official cnc-ddraw is available, also replace an installed
    cnc-ddraw that is new enough.

.PARAMETER Yes
    Answer yes to every confirmation. For unattended use only.

.PARAMETER NoPause
    Skip the "Press Enter to exit" pause at the end.

.EXAMPLE
    .\Install-WarWindRemaster.ps1

.EXAMPLE
    .\Install-WarWindRemaster.ps1 "D:\Games\War Wind" -SkipCutscenes -Disable HiRes

.EXAMPLE
    .\Install-WarWindRemaster.ps1 -Uninstall

.NOTES
    Windows PowerShell 5.1 compatible. Never launches the game and never modifies WW.EXE.
    If Windows refuses to run it, use:
      powershell -ExecutionPolicy Bypass -File .\Install-WarWindRemaster.ps1
    The mod release can be private while in testing: set $env:GITHUB_TOKEN to a token that can
    read the repository and the installer authenticates its GitHub requests with it.
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $GamePath,

    [switch] $Uninstall,
    [switch] $RemoveCutscenes,
    [switch] $SkipCutscenes,

    [ValidateSet('GridHotkeys', 'CtrlGroups', 'EdgeScroll', 'WheelScroll', 'MiddleDrag', 'DoubleClickType', 'ModernClicks', 'HiRes', 'PreciseClock', 'SmoothMotion', 'CommandGrid', 'InGameSaveLoad', 'WideMenus', 'Modern', 'HDVideos')]
    [string[]] $Disable = @(),

    [string] $Tag,
    [string] $LocalFiles,
    [string] $Downloads,

    [switch] $KeepDownloads,
    [switch] $Force,
    [switch] $Yes,
    [switch] $NoPause
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# ---------------------------------------------------------------------------------------
# Where things come from. Edit here when a link moves.
#
# cnc-ddraw normally comes from this project's release (ddraw.dll: upstream 279a057 = 7.1.0.1
# plus third_party/cnc-ddraw/cnc-ddraw-backdrop.patch). The official release is the fallback,
# pinned to the version this mod was tested with; the "latest" lookup is only used when the
# pinned asset cannot be fetched. Every ini key the profile below uses exists in 7.1.0.0
# (win_version, the newest of them, arrived in September 2024) except backdrop, which only our
# build reads; the official build ignores it and draws black bars.
# ---------------------------------------------------------------------------------------

$Sources = @{
    ModReleases     = 'https://api.github.com/repos/jlrouzies-fr/WarWind-ArtificialRemaster/releases?per_page=30'
    ModHome         = 'https://github.com/jlrouzies-fr/WarWind-ArtificialRemaster/releases'
    ModDdrawSource  = 'https://github.com/jlrouzies-fr/WarWind-ArtificialRemaster/tree/main/third_party/cnc-ddraw'
    CncDdrawPinned  = 'https://github.com/FunkyFr3sh/cnc-ddraw/releases/download/v7.1.0.0/cnc-ddraw.zip'
    CncDdrawLatest  = 'https://api.github.com/repos/FunkyFr3sh/cnc-ddraw/releases/latest'
    CncDdrawHome    = 'https://github.com/FunkyFr3sh/cnc-ddraw'
}

$SteamAppId       = '1741140'
$CncMinVersion    = [version] '7.1'
$VersionCheckVa   = 0x4684C7                                   # mov ebx, 8 before SetVideoMode(640, 480, 8)
$VersionCheckData = [byte[]] @(0xBB, 0x08, 0x00, 0x00, 0x00)
$KnownExeSha256   = @{ 'D0C051F219E0B5BB8BB351D306ECAED62690E4A17D214D63F04B41C9AD5C7795' = 'Steam release (1.2, DirectX 3 build)' }
$Races            = @('EA', 'OB', 'OPEN', 'SH', 'TH')
$CutsceneAsset    = '(?i)^WarWind-HD-Cutscenes-(EA|OB|OPEN|SH|TH)(-\d+of\d+)?\.zip$'
$BackupFolderName = '_ArtificialRemaster_backup'

# Files the installer may replace, relative to the game folder. Each is backed up (or recorded
# as absent) before the first install, and restored (or deleted) by -Uninstall.
$ManagedFiles = @('ddraw.dll', 'ddraw.ini', 'dsound.dll', 'WarWindHD.ini', 'WarWindHD\feature-hires_720.wwp', 'WarWindHD\fix-gdi-palette.wwp', 'WarWindHD\backdrop.png', 'WarWindHD\shaders\wwfx.hlsl')

# Keep in sync with tools/ddraw.user.ini. Written as is when there is no cnc-ddraw ddraw.ini yet;
# otherwise every key below is set in the existing file and its other keys are left alone.
$DdrawIni = @'
; cnc-ddraw - https://github.com/FunkyFr3sh/cnc-ddraw
; War Wind - Artificial Remaster profile: borderless window covering the screen, integer scaling,
; no display-mode change. Written by Install-WarWindRemaster.ps1.
[ddraw]
; image shown around 640x480 screens instead of black bars (this project's cnc-ddraw build)
backdrop=WarWindHD\backdrop.png
windowed=true
fullscreen=true
toggle_borderless=true
boxing=true
maintas=true
renderer=auto
d3d9_filter=0
adjmouse=true
savesettings=0
nonexclusive=true
singlecpu=true
; Spoof Windows 95 so the game skips its "Windows NT is not supported" dialog
win_version=95

[WW]
minfps=-1
'@

# ---------------------------------------------------------------------------------------
# Output plumbing
# ---------------------------------------------------------------------------------------

$script:CountDone = 0
$script:CountWarn = 0
$script:CountFail = 0
$script:Manual    = New-Object System.Collections.ArrayList   # steps the user must do
$script:Changed   = New-Object System.Collections.ArrayList   # files written, for Unblock-File
$script:Release      = $null
$script:ReleaseCache = $null
$script:LastWebError = ''

$script:UseColour = $true
try {
    if ($null -eq $Host -or $null -eq $Host.UI -or $null -eq $Host.UI.RawUI) { $script:UseColour = $false }
    else { $null = $Host.UI.RawUI.ForegroundColor }
}
catch { $script:UseColour = $false }

function Write-Chunk
{
    param([string] $Text, [string] $Colour, [switch] $NoNewline)
    try {
        if ($script:UseColour -and $Colour) { Write-Host $Text -ForegroundColor $Colour -NoNewline:$NoNewline }
        else { Write-Host $Text -NoNewline:$NoNewline }
    }
    catch {
        $script:UseColour = $false
        Write-Host $Text -NoNewline:$NoNewline
    }
}

# The emblem: a two-armed wind swirl, gold outer arm around a green inner arm, with a gold eye.
# Written with placeholders so this file stays plain ASCII: B = full block, u = upper half,
# l = lower half, o = the eye.
function ConvertTo-Glyphs
{
    param([string] $S)
    return $S.Replace('B', [string][char]0x2588).Replace('u', [string][char]0x2580).Replace('l', [string][char]0x2584).Replace('o', [string][char]0x25CF)
}

function Write-Banner
{
    $box  = 'DarkGray'
    $w    = 70
    $mark = 18
    $gold = 'Yellow'
    $green = 'Green'

    $rows = @(
        @{ Mark = @(,@('      llllll', $gold));                                                   Text = '';                                                   TextColour = '' },
        @{ Mark = @(,@('   lBuu    uul', $gold));                                                 Text = 'War Wind - Artificial Remaster';                     TextColour = 'White' },
        @{ Mark = @(@('  Bu  ', $gold), @('luuul', $green));                                      Text = 'AI-upscaled cutscenes, 1280x720 missions,';           TextColour = 'Gray' },
        @{ Mark = @(@('  B  ', $gold), @('B  ', $green), @('o', $gold), @('  B', $green));          Text = 'grid hotkeys and modern mouse controls';            TextColour = 'Gray' },
        @{ Mark = @(@('  ul  ', $gold), @('ulllu', $green), @('  lB', $gold));                      Text = 'a fan mod -- needs your own copy of War Wind';       TextColour = 'DarkGray' },
        @{ Mark = @(,@('    uulllllBu', $gold));                                                  Text = '';                                                   TextColour = '' }
    )

    Write-Host ''
    Write-Chunk ('  ' + [char]0x2554 + ([string][char]0x2550) * $w + [char]0x2557) $box
    foreach ($r in $rows) {
        Write-Chunk ('  ' + [char]0x2551) $box -NoNewline
        $used = 0
        foreach ($seg in $r.Mark) {
            $t = ConvertTo-Glyphs $seg[0]
            Write-Chunk $t $seg[1] -NoNewline
            $used += $t.Length
        }
        if ($used -lt $mark) { Write-Chunk ((' ') * ($mark - $used)) $null -NoNewline; $used = $mark }
        if ($r.Text) { Write-Chunk $r.Text $r.TextColour -NoNewline; $used += $r.Text.Length }
        if ($used -lt $w) { Write-Chunk ((' ') * ($w - $used)) $null -NoNewline }
        Write-Chunk ([string][char]0x2551) $box
    }
    Write-Chunk ('  ' + [char]0x255A + ([string][char]0x2550) * $w + [char]0x255D) $box
    Write-Host ''
}

function Write-Section
{
    param([string] $Title)
    Write-Host ''
    Write-Chunk ('  ' + [char]0x2500 + [char]0x2500 + ' ') 'Yellow' -NoNewline
    Write-Chunk $Title 'White' -NoNewline
    $pad = 64 - $Title.Length
    if ($pad -lt 1) { $pad = 1 }
    Write-Chunk (' ' + ([string][char]0x2500) * $pad) 'Yellow'
}

# $Status: Done (something was installed/written), Ok (already right, nothing to do),
# Skip (not applicable), Warn, Fail, Info.
function Report
{
    param(
        [ValidateSet('Done', 'Ok', 'Skip', 'Warn', 'Fail', 'Info')]
        [string] $Status,
        [string] $Text,
        [string] $Detail,
        [string] $Manual
    )

    switch ($Status) {
        'Done' { $glyph = '[DONE]'; $colour = 'Green';    $script:CountDone++ }
        'Ok'   { $glyph = '[ OK ]'; $colour = 'Green' }
        'Skip' { $glyph = '[ -- ]'; $colour = 'DarkGray' }
        'Warn' { $glyph = '[WARN]'; $colour = 'Yellow';   $script:CountWarn++ }
        'Fail' { $glyph = '[FAIL]'; $colour = 'Red';      $script:CountFail++ }
        'Info' { $glyph = '[ .. ]'; $colour = 'DarkGray' }
    }

    if ($Manual) { $null = $script:Manual.Add($Manual) }

    Write-Chunk ('  ' + $glyph + ' ') $colour -NoNewline
    if ($Status -eq 'Skip' -or $Status -eq 'Info') { Write-Chunk $Text 'DarkGray' } else { Write-Host $Text }
    if ($Detail) {
        foreach ($line in ($Detail -split "`n")) {
            if ($line.Trim()) { Write-Chunk ('         ' + $line.Trim()) 'DarkGray' }
        }
    }
    if ($Manual) {
        $first = $true
        foreach ($line in ($Manual -split "`n")) {
            if (-not $line.Trim()) { continue }
            if ($first) { Write-Chunk ('         ' + [char]0x2192 + ' ' + $line.Trim()) 'DarkYellow'; $first = $false }
            else { Write-Chunk ('           ' + $line.Trim()) 'DarkYellow' }
        }
    }
}

function Exit-Installer
{
    param([int] $Code)
    if (-not $NoPause) {
        Write-Host ''
        try { [void](Read-Host '  Press Enter to exit') } catch { }
    }
    exit $Code
}

function Stop-Install
{
    param([string] $Text, [string] $Detail, [string] $Manual)
    Report -Status 'Fail' -Text $Text -Detail $Detail -Manual $Manual
    Write-Host ''
    Write-Chunk '  Stopped: nothing further was changed.' 'Red'
    Exit-Installer 1
}

function Confirm-Step
{
    param([string] $Question, [switch] $DefaultYes)
    if ($Yes) { return $true }
    Write-Host ''
    $hint = '[y/N]'
    if ($DefaultYes) { $hint = '[Y/n]' }
    Write-Chunk ('  ' + $Question + ' ' + $hint + ' ') 'Cyan' -NoNewline
    try { $a = Read-Host } catch { return [bool]$DefaultYes }
    if (-not $a.Trim()) { return [bool]$DefaultYes }
    return ($a -match '^(?i)y(es)?$')
}

# ---------------------------------------------------------------------------------------
# Small, defensive helpers
# ---------------------------------------------------------------------------------------

function Join-Safe
{
    param([string] $Parent, [string] $Child)
    try { return [IO.Path]::Combine($Parent, $Child) } catch { return $null }
}

function Test-FileHere
{
    param([string] $Path)
    if (-not $Path) { return $false }
    try { return (Test-Path -LiteralPath $Path -PathType Leaf) } catch { return $false }
}

function Test-DirHere
{
    param([string] $Path)
    if (-not $Path) { return $false }
    try { return (Test-Path -LiteralPath $Path -PathType Container) } catch { return $false }
}

function New-DirSafe
{
    param([string] $Path)
    if (-not (Test-DirHere $Path)) { $null = New-Item -ItemType Directory -Path $Path -Force }
}

function Format-Size
{
    param([long] $Bytes)
    if ($Bytes -ge 1073741824) { return ('{0:N1} GB' -f ($Bytes / 1073741824)) }
    if ($Bytes -ge 1048576)    { return ('{0:N1} MB' -f ($Bytes / 1048576)) }
    if ($Bytes -ge 1024)       { return ('{0:N0} KB' -f ($Bytes / 1024)) }
    return ('{0} B' -f $Bytes)
}

function Get-Sha256
{
    param([string] $Path)
    try { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash.ToUpperInvariant() } catch { return $null }
}

function Get-FileVersionSafe
{
    param([string] $Path)
    if (-not (Test-FileHere $Path)) { return $null }
    try {
        $vi = (Get-Item -LiteralPath $Path -ErrorAction Stop).VersionInfo
        return $vi
    }
    catch { return $null }
}

# Scan a small binary for an ASCII marker.
function Test-BinaryMarker
{
    param([string] $Path, [string] $Pattern, [int] $MaxBytes = 16777216)
    if (-not (Test-FileHere $Path)) { return $false }
    try {
        $fi = Get-Item -LiteralPath $Path -ErrorAction Stop
        if ($fi.Length -gt $MaxBytes -or $fi.Length -eq 0) { return $false }
        $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Path))
        return [regex]::IsMatch($text, $Pattern)
    }
    catch { return $false }
}

function Copy-Tracked
{
    param([string] $From, [string] $To)
    New-DirSafe ([IO.Path]::GetDirectoryName($To))
    Copy-Item -LiteralPath $From -Destination $To -Force
    $null = $script:Changed.Add($To)
}

function Write-TextTracked
{
    param([string] $Path, [string] $Text)
    New-DirSafe ([IO.Path]::GetDirectoryName($Path))
    [IO.File]::WriteAllText($Path, $Text, (New-Object Text.UTF8Encoding $false))
    $null = $script:Changed.Add($Path)
}

function Read-TextSafe
{
    param([string] $Path)
    if (-not (Test-FileHere $Path)) { return $null }
    try {
        $t = [IO.File]::ReadAllText($Path, [Text.Encoding]::UTF8)
        if ($t.Length -gt 0 -and [int]$t[0] -eq 0xFEFF) { $t = $t.Substring(1) }
        return $t
    }
    catch { return $null }
}

function Get-FreeSpace
{
    param([string] $Path)
    try { return (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Path))).AvailableFreeSpace } catch { return [long]-1 }
}

# ---------------------------------------------------------------------------------------
# Ini editing that keeps everything else in the file intact.
# ---------------------------------------------------------------------------------------

function Set-IniKey
{
    param([string] $Text, [string] $Section, [string] $Key, [string] $Value)

    if ($null -eq $Text) { $Text = '' }
    $nl = "`r`n"
    $lines = New-Object System.Collections.ArrayList
    foreach ($l in ($Text -split "`r?`n")) { $null = $lines.Add($l) }
    if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }

    $cur = ''
    $secStart = -1
    $secEnd = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $t = $lines[$i].Trim()
        if ($t -match '^\[(.+)\]$') {
            if ($cur -ieq $Section -and $secStart -ge 0 -and $secEnd -lt 0) { $secEnd = $i }
            $cur = $Matches[1]
            if ($cur -ieq $Section) { $secStart = $i + 1 }
            continue
        }
        if ($cur -ieq $Section -and $t -match ('(?i)^' + [regex]::Escape($Key) + '\s*=')) {
            $lines[$i] = $Key + '=' + $Value
            return (($lines -join $nl) + $nl)
        }
    }
    if ($secStart -ge 0) {
        if ($secEnd -lt 0) { $secEnd = $lines.Count }
        $at = $secEnd
        while ($at -gt $secStart -and $lines[$at - 1].Trim() -eq '') { $at-- }
        $lines.Insert($at, ($Key + '=' + $Value))
    }
    else {
        if ($lines.Count -gt 0) { $null = $lines.Add('') }
        $null = $lines.Add('[' + $Section + ']')
        $null = $lines.Add($Key + '=' + $Value)
    }
    return (($lines -join $nl) + $nl)
}

# Every (section, key, value) in an ini text, in order.
function Get-IniEntries
{
    param([string] $Text)
    $out = @()
    if ($null -eq $Text) { return $out }
    $cur = ''
    foreach ($l in ($Text -split "`r?`n")) {
        $t = $l.Trim()
        if ($t -match '^\[(.+)\]$') { $cur = $Matches[1]; continue }
        if ($t -match '^[;#]' -or -not $t) { continue }
        if ($t -match '^([^=]+?)\s*=\s*(.*)$') { $out += ,@($cur, $Matches[1], $Matches[2]) }
    }
    return ,$out
}

# ---------------------------------------------------------------------------------------
# WW.EXE checks. Read-only: the file is opened for reading and shared, never written.
# ---------------------------------------------------------------------------------------

# Reads $Count bytes at virtual address $Va, mapping it through the section table.
# Watcom leaves VirtualSize at 0 in every section, so the raw size stands in for it.
function Read-PeBytesAtVa
{
    param([string] $Path, [long] $Va, [int] $Count)
    $fs = $null
    $br = $null
    try {
        $fs = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $br = New-Object IO.BinaryReader($fs)
        if ($fs.Length -lt 0x40 -or $br.ReadUInt16() -ne 0x5A4D) { return $null }        # 'MZ'
        $fs.Position = 0x3C
        $peOff = $br.ReadInt32()
        if ($peOff -le 0 -or ($peOff + 24) -ge $fs.Length) { return $null }
        $fs.Position = $peOff
        if ($br.ReadUInt32() -ne 0x00004550) { return $null }                           # 'PE\0\0'
        $machine   = $br.ReadUInt16()
        $nSections = $br.ReadUInt16()
        $fs.Position = $peOff + 20
        $optSize   = $br.ReadUInt16()
        $optOff    = [long]$peOff + 24
        $fs.Position = $optOff
        if ($br.ReadUInt16() -ne 0x10B -or $machine -ne 0x014C) { return $null }        # PE32, x86
        $fs.Position = $optOff + 28
        $imageBase = [long]$br.ReadUInt32()
        $rva = $Va - $imageBase
        $secOff = $optOff + $optSize
        for ($i = 0; $i -lt $nSections; $i++) {
            $fs.Position = $secOff + ([long]$i * 40) + 8
            $vsize = [long]$br.ReadUInt32()
            $vaddr = [long]$br.ReadUInt32()
            $rsize = [long]$br.ReadUInt32()
            $raw   = [long]$br.ReadUInt32()
            $span  = $rsize
            if ($vsize -gt 0 -and $vsize -lt $rsize) { $span = $vsize }
            if ($rva -ge $vaddr -and ($rva + $Count) -le ($vaddr + $span)) {
                $off = $raw + ($rva - $vaddr)
                if (($off + $Count) -gt $fs.Length) { return $null }
                $fs.Position = $off
                return New-Object psobject -Property @{ Offset = $off; Bytes = $br.ReadBytes($Count) }
            }
        }
        return $null
    }
    catch { return $null }
    finally {
        if ($br) { try { $br.Close() } catch { } }
        if ($fs) { try { $fs.Dispose() } catch { } }
    }
}

function Format-Hex
{
    param([byte[]] $Bytes)
    if ($null -eq $Bytes) { return '(unreadable)' }
    return (($Bytes | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')
}

# ---------------------------------------------------------------------------------------
# Finding the game
# ---------------------------------------------------------------------------------------

function Get-SteamLibraries
{
    $roots = New-Object System.Collections.ArrayList
    foreach ($k in @(@('HKCU:\Software\Valve\Steam', 'SteamPath'),
                     @('HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath'),
                     @('HKLM:\SOFTWARE\Valve\Steam', 'InstallPath'))) {
        try {
            $v = (Get-ItemProperty -LiteralPath $k[0] -Name $k[1] -ErrorAction Stop).($k[1])
            if ($v) { $null = $roots.Add(($v -replace '/', '\')) }
        }
        catch { }
    }

    $libs = New-Object System.Collections.ArrayList
    foreach ($r in $roots) {
        if (-not (Test-DirHere $r)) { continue }
        if (-not ($libs -contains $r)) { $null = $libs.Add($r) }
        $vdf = Read-TextSafe (Join-Safe $r 'steamapps\libraryfolders.vdf')
        if (-not $vdf) { continue }
        foreach ($m in [regex]::Matches($vdf, '"path"\s+"([^"]+)"')) {
            $p = $m.Groups[1].Value -replace '\\\\', '\'
            if ((Test-DirHere $p) -and -not ($libs -contains $p)) { $null = $libs.Add($p) }
        }
    }
    return $libs
}

function Find-SteamGame
{
    foreach ($lib in (Get-SteamLibraries)) {
        $acf = Join-Safe $lib ('steamapps\appmanifest_' + $SteamAppId + '.acf')
        $dir = $null
        $t = Read-TextSafe $acf
        if ($t) {
            $m = [regex]::Match($t, '"installdir"\s+"([^"]+)"')
            if ($m.Success) { $dir = Join-Safe $lib ('steamapps\common\' + $m.Groups[1].Value) }
        }
        if (-not $dir) { $dir = Join-Safe $lib 'steamapps\common\War Wind' }
        if (Test-FileHere (Join-Safe $dir 'WW.EXE')) { return $dir }
    }
    return $null
}

# ---------------------------------------------------------------------------------------
# Downloads
# ---------------------------------------------------------------------------------------

try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 -bor 0x3000 } catch { }

$script:Token = $env:GITHUB_TOKEN
$UserAgent = 'WarWind-ArtificialRemaster-Installer (PowerShell)'

function New-Request
{
    param([string] $Url, [string] $Accept)
    $req = [Net.HttpWebRequest]::Create($Url)
    $req.UserAgent = $UserAgent
    $req.Timeout = 30000
    $req.ReadWriteTimeout = 120000
    if ($Accept) { $req.Accept = $Accept }
    # Only GitHub gets the token; HttpWebRequest drops it on the redirect to the asset CDN.
    if ($script:Token -and $Url -match '^https://(api\.)?github\.com/') { $req.Headers.Add('Authorization', 'token ' + $script:Token) }
    return $req
}

function Get-WebJson
{
    param([string] $Url)
    $resp = $null
    try {
        $resp = (New-Request $Url 'application/vnd.github+json').GetResponse()
        $sr = New-Object IO.StreamReader($resp.GetResponseStream())
        try { $json = $sr.ReadToEnd() } finally { $sr.Dispose() }
        # Windows PowerShell 5.1 hands a JSON array down the pipeline as ONE object; assigning
        # it first and piping the variable enumerates it.
        $parsed = $json | ConvertFrom-Json
        return $parsed
    }
    catch {
        $script:LastWebError = $_.Exception.Message
        if ($_.Exception.InnerException) { $script:LastWebError = $_.Exception.InnerException.Message }
        return $null
    }
    finally { if ($resp) { $resp.Close() } }
}

# Downloads $Url to $Dest with a progress line, resuming a previous .part file.
# $ExpectedSize > 0 makes a cached file of that exact size count as done. Never throws.
function Get-Download
{
    param([string] $Url, [string] $Dest, [string] $Label, [long] $ExpectedSize = 0, [string] $Accept)

    if (Test-FileHere $Dest) {
        $have = (Get-Item -LiteralPath $Dest).Length
        if ($ExpectedSize -le 0 -or $have -eq $ExpectedSize) {
            Report -Status 'Ok' -Text ($Label + ': cached (' + (Format-Size $have) + ')')
            return $true
        }
        Remove-Item -LiteralPath $Dest -Force
    }

    New-DirSafe ([IO.Path]::GetDirectoryName($Dest))
    $tmp = $Dest + '.part'
    $start = [long]0
    if (Test-FileHere $tmp) {
        $start = (Get-Item -LiteralPath $tmp).Length
        if ($ExpectedSize -gt 0 -and $start -ge $ExpectedSize) { Remove-Item -LiteralPath $tmp -Force; $start = 0 }
    }

    $resp = $null
    $in = $null
    $out = $null
    try {
        $req = New-Request $Url $Accept
        if ($start -gt 0) { $req.AddRange($start) }
        $resp = $req.GetResponse()
        if ($start -gt 0 -and [int]$resp.StatusCode -ne 206) { $start = 0 }   # no resume offered: start over
        $total = $ExpectedSize
        if ($resp.ContentLength -gt 0) { $total = $start + $resp.ContentLength }

        $mode = [IO.FileMode]::Create
        if ($start -gt 0) { $mode = [IO.FileMode]::Append }
        $out = New-Object IO.FileStream($tmp, $mode, [IO.FileAccess]::Write)
        $in  = $resp.GetResponseStream()
        $buf = New-Object byte[] 1048576
        $done = $start
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $lastShown = -1000
        $sinceStart = [long]0
        if ($start -gt 0) { Write-Chunk ('  [ .. ] ' + $Label + ': resuming at ' + (Format-Size $start)) 'DarkGray' }
        while ($true) {
            $n = $in.Read($buf, 0, $buf.Length)
            if ($n -le 0) { break }
            $out.Write($buf, 0, $n)
            $done += $n
            $sinceStart += $n
            $ms = $clock.ElapsedMilliseconds
            if ($ms - $lastShown -ge 250) {
                $lastShown = $ms
                $rate = 0
                if ($ms -gt 0) { $rate = $sinceStart / ($ms / 1000.0) }
                $line = '  [ .. ] ' + $Label + ': ' + (Format-Size $done)
                if ($total -gt 0) { $line += (' / ' + (Format-Size $total) + ('  {0,5:N1}%' -f (100.0 * $done / $total))) }
                $line += ('  ' + (Format-Size ([long]$rate)) + '/s')
                if ($total -gt 0 -and $rate -gt 0) {
                    $eta = [TimeSpan]::FromSeconds([math]::Ceiling(($total - $done) / $rate))
                    $line += ('  ' + $eta.ToString('hh\:mm\:ss') + ' left')
                }
                Write-Chunk ("`r" + $line.PadRight(100)) 'DarkGray' -NoNewline
            }
        }
        Write-Chunk ("`r" + (' ' * 102) + "`r") $null -NoNewline
        $out.Dispose(); $out = $null
        $len = (Get-Item -LiteralPath $tmp).Length
        if ($len -lt 16) { throw 'the download is empty' }
        if ($ExpectedSize -gt 0 -and $len -ne $ExpectedSize) { throw ('got ' + $len + ' bytes, expected ' + $ExpectedSize) }
        Move-Item -LiteralPath $tmp -Destination $Dest -Force
        Report -Status 'Done' -Text ($Label + ': downloaded (' + (Format-Size $len) + ')')
        return $true
    }
    catch {
        Write-Host ''
        $msg = $_.Exception.Message
        if ($_.Exception.InnerException) { $msg = $_.Exception.InnerException.Message }
        Report -Status 'Fail' -Text ($Label + ': download failed.') -Detail ('From: ' + $Url + "`n" + $msg + "`nRe-run the installer to resume where it stopped.")
        return $false
    }
    finally {
        if ($out) { try { $out.Dispose() } catch { } }
        if ($in) { try { $in.Dispose() } catch { } }
        if ($resp) { try { $resp.Close() } catch { } }
    }
}

# A file from -LocalFiles, if one of that name is there.
function Find-Local
{
    param([string] $Name)
    if (-not $LocalFiles -or -not (Test-DirHere $LocalFiles)) { return $null }
    $p = Join-Safe $LocalFiles $Name
    if (Test-FileHere $p) { return $p }
    return $null
}

# Resolves a mod release asset by name: -LocalFiles, then the release, then (for the text
# files only) a repository checkout next to this script. Returns a path or $null.
function Resolve-ModAsset
{
    param([string] $Name, [string] $CheckoutPath)
    $hit = Find-Local $Name
    if ($hit) { Report -Status 'Ok' -Text ($Name + ': using ' + $hit); return $hit }

    if ($script:Release) {
        $a = @($script:Release.assets | Where-Object { $_.name -ieq $Name })
        if ($a.Count -gt 0) {
            $dest = Join-Safe $script:ReleaseCache $Name
            $url = $a[0].browser_download_url
            $accept = $null
            if ($script:Token) { $url = $a[0].url; $accept = 'application/octet-stream' }
            if (Get-Download -Url $url -Dest $dest -Label $Name -ExpectedSize ([long]$a[0].size) -Accept $accept) { return $dest }
            return $null
        }
    }
    if ($CheckoutPath -and (Test-FileHere $CheckoutPath)) {
        Report -Status 'Warn' -Text ($Name + ': not in the release; using the repository copy next to this script.') -Detail $CheckoutPath
        return $CheckoutPath
    }
    return $null
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# ---------------------------------------------------------------------------------------
# Backup manifest: one line per managed file, "present<TAB>path" or "absent<TAB>path".
# Written once, before the first install touches anything; later runs keep it, so the
# backup always holds the folder as it was before this mod.
# ---------------------------------------------------------------------------------------

function Read-Manifest
{
    param([string] $BackupDir)
    $t = Read-TextSafe (Join-Safe $BackupDir 'manifest.txt')
    if (-not $t) { return $null }
    $map = [ordered]@{}
    foreach ($l in ($t -split "`r?`n")) {
        if ($l -match '^(present|absent)\t(.+)$') { $map[$Matches[2]] = $Matches[1] }
    }
    return $map
}

function Save-Manifest
{
    param([string] $BackupDir, $Map)
    $lines = @(('# War Wind - Artificial Remaster backup, ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')),
               '# present = the original is saved here; absent = the file did not exist before the install')
    foreach ($k in $Map.Keys) { $lines += ($Map[$k] + "`t" + $k) }
    [IO.File]::WriteAllText((Join-Safe $BackupDir 'manifest.txt'), (($lines -join "`r`n") + "`r`n"), (New-Object Text.UTF8Encoding $false))
}

# A dsound.dll that is this mod (from an earlier install or a dev build) is not "the original".
function Test-IsModDll
{
    param([string] $Path)
    return (Test-BinaryMarker $Path 'WarWindHD\.ini')
}

# ---------------------------------------------------------------------------------------
# 0. Resolve the target
# ---------------------------------------------------------------------------------------

Write-Banner

$here = $null
try { $here = $PSScriptRoot } catch { }
if (-not $here) { try { $here = Split-Path -Parent $MyInvocation.MyCommand.Path } catch { } }

if (-not $GamePath -or -not $GamePath.Trim()) {
    $candidates = @()
    if ($here) { $candidates += $here; $candidates += (Split-Path -Parent $here) }
    foreach ($c in $candidates) {
        if ($c -and (Test-FileHere (Join-Safe $c 'WW.EXE'))) { $GamePath = $c; break }
    }
    if (-not $GamePath) {
        $steam = Find-SteamGame
        if ($steam) {
            Write-Chunk ('  Found War Wind in Steam: ' + $steam) 'White'
            if (Confirm-Step 'Use this folder?' -DefaultYes) { $GamePath = $steam }
        }
    }
    if (-not $GamePath) {
        Write-Chunk '  Path to your War Wind folder (the one holding WW.EXE): ' 'Cyan' -NoNewline
        try { $GamePath = Read-Host } catch { $GamePath = '' }
        $GamePath = $GamePath.Trim().Trim('"')
    }
}
if (-not $GamePath) { Stop-Install -Text 'No game folder given.' -Manual 'Pass the War Wind folder: .\Install-WarWindRemaster.ps1 "D:\Games\War Wind"' }

try { $resolved = (Resolve-Path -LiteralPath $GamePath -ErrorAction Stop).ProviderPath } catch { $resolved = $null }
if (-not $resolved) { Stop-Install -Text ('Path not found: ' + $GamePath) }
if (Test-FileHere $resolved) { $resolved = [IO.Path]::GetDirectoryName($resolved) }
$gameDir = $resolved.TrimEnd('\')
$exePath = Join-Safe $gameDir 'WW.EXE'
$backupDir = Join-Safe $gameDir $BackupFolderName

if ($gameDir.StartsWith($env:WINDIR, [StringComparison]::OrdinalIgnoreCase)) {
    Stop-Install -Text 'Refusing to touch the Windows directory.'
}

Write-Chunk '  Game    ' 'DarkGray' -NoNewline
Write-Host $gameDir
Write-Chunk '  Mode    ' 'DarkGray' -NoNewline
if ($Uninstall) { Write-Host 'uninstall' } else { Write-Host 'install' }

# ---------------------------------------------------------------------------------------
# 1. Check the game folder
# ---------------------------------------------------------------------------------------

Write-Section 'Game folder'

if (-not (Test-FileHere $exePath)) {
    Stop-Install -Text ('WW.EXE is not in ' + $gameDir) -Manual 'Point the installer at the folder that holds WW.EXE.'
}
Report -Status 'Ok' -Text 'WW.EXE found.'

$running = @()
try {
    $running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ($_.Path -ieq $exePath) } catch { $false }
    })
}
catch { }
if ($running.Count -gt 0) {
    Stop-Install -Text 'War Wind is running.' -Manual 'Close the game, then run the installer again.'
}

if (-not $Uninstall) {
    $probe = Read-PeBytesAtVa -Path $exePath -Va $VersionCheckVa -Count $VersionCheckData.Length
    $exeHash = Get-Sha256 $exePath
    $label = $null
    if ($exeHash -and $KnownExeSha256.ContainsKey($exeHash)) { $label = $KnownExeSha256[$exeHash] }
    if ($null -eq $probe) {
        Stop-Install -Text 'WW.EXE could not be read as a 32-bit Windows executable.' -Detail $exePath
    }
    $match = $true
    for ($i = 0; $i -lt $VersionCheckData.Length; $i++) { if ($probe.Bytes[$i] -ne $VersionCheckData[$i]) { $match = $false } }
    $where = ('VA 0x{0:X} = file offset 0x{1:X}: {2}' -f $VersionCheckVa, $probe.Offset, (Format-Hex $probe.Bytes))
    if ($match) {
        $t = 'WW.EXE is the expected build.'
        if ($label) { $t = 'WW.EXE is the expected build: ' + $label + '.' }
        Report -Status 'Ok' -Text $t -Detail $where
    }
    else {
        Report -Status 'Fail' -Text 'WW.EXE is not the build this mod was made for.' `
               -Detail ($where + "`nexpected " + (Format-Hex $VersionCheckData) + '. The mod would disable itself at start-up.') `
               -Manual 'The mod targets War Wind 1.2 (the DirectX 3 build sold on Steam). Restore an unmodified WW.EXE (Steam: Properties > Installed Files > Verify integrity) and re-run.'
        Stop-Install -Text 'Install cancelled: wrong game build.'
    }
}

# Cache folder
if (-not $Downloads) { $Downloads = Join-Safe $env:LOCALAPPDATA 'WarWind-ArtificialRemaster\downloads' }
$script:Cache = $Downloads

# ---------------------------------------------------------------------------------------
# Uninstall
# ---------------------------------------------------------------------------------------

if ($Uninstall) {
    Write-Section 'Restore the original files'

    $manifest = Read-Manifest $backupDir
    if ($null -eq $manifest) {
        Report -Status 'Warn' -Text ('No backup manifest in ' + $backupDir) -Detail 'Only the mod''s own files can be removed; ddraw.dll and ddraw.ini are left alone.'
        $manifest = [ordered]@{ 'WarWindHD.ini' = 'absent'; 'WarWindHD\feature-hires_720.wwp' = 'absent'; 'WarWindHD\fix-gdi-palette.wwp' = 'absent'; 'WarWindHD\backdrop.png' = 'absent'; 'WarWindHD\shaders\wwfx.hlsl' = 'absent' }
        $ds = Join-Safe $gameDir 'dsound.dll'
        if (Test-IsModDll $ds) { $manifest['dsound.dll'] = 'absent' }
    }

    $restoreFailed = $false
    foreach ($rel in @($manifest.Keys)) {
        $target = Join-Safe $gameDir $rel
        if ($manifest[$rel] -eq 'present') {
            $saved = Join-Safe $backupDir $rel
            if (Test-FileHere $saved) {
                try { Copy-Tracked -From $saved -To $target; Report -Status 'Done' -Text ($rel + ': original restored.') }
                catch { $restoreFailed = $true; Report -Status 'Fail' -Text ($rel + ': could not restore.') -Detail $_.Exception.Message }
            }
            else { $restoreFailed = $true; Report -Status 'Fail' -Text ($rel + ': the backup copy is missing.') -Detail $saved }
        }
        elseif (Test-FileHere $target) {
            try { Remove-Item -LiteralPath $target -Force; Report -Status 'Done' -Text ($rel + ': removed (it was not there before the install).') }
            catch { $restoreFailed = $true; Report -Status 'Fail' -Text ($rel + ': could not remove.') -Detail $_.Exception.Message }
        }
        else { Report -Status 'Skip' -Text ($rel + ': not present.') }
    }

    foreach ($rel in @('WarWindHD.log', 'WarWindHD.cmd')) {
        $p = Join-Safe $gameDir $rel
        if (Test-FileHere $p) { try { Remove-Item -LiteralPath $p -Force; Report -Status 'Done' -Text ($rel + ': removed.') } catch { } }
    }
    $shaderDir = Join-Safe $gameDir 'WarWindHD\shaders'
    if ((Test-DirHere $shaderDir) -and @(Get-ChildItem -LiteralPath $shaderDir -Force).Count -eq 0) {
        Remove-Item -LiteralPath $shaderDir -Force
    }
    $patchDir = Join-Safe $gameDir 'WarWindHD'
    if ((Test-DirHere $patchDir) -and @(Get-ChildItem -LiteralPath $patchDir -Force).Count -eq 0) {
        Remove-Item -LiteralPath $patchDir -Force
        Report -Status 'Done' -Text 'WarWindHD\: removed (empty).'
    }

    Write-Section 'HD cutscenes'
    $vidsHd = Join-Safe $gameDir 'Data\VIDS_HD'
    if (Test-DirHere $vidsHd) {
        $size = [long]0
        try { $size = [long](Get-ChildItem -LiteralPath $vidsHd -File -Recurse | Measure-Object -Property Length -Sum).Sum } catch { }
        $drop = $RemoveCutscenes
        if (-not $drop -and -not $Yes) { $drop = Confirm-Step ('Also delete the HD cutscenes in Data\VIDS_HD (' + (Format-Size $size) + ')?') }
        if ($drop) {
            try { Remove-Item -LiteralPath $vidsHd -Recurse -Force; Report -Status 'Done' -Text ('Data\VIDS_HD removed (' + (Format-Size $size) + ' freed).') }
            catch { Report -Status 'Fail' -Text 'Could not remove Data\VIDS_HD.' -Detail $_.Exception.Message }
        }
        else { Report -Status 'Skip' -Text 'Data\VIDS_HD kept (the original game ignores it).' }
    }
    else { Report -Status 'Skip' -Text 'No Data\VIDS_HD folder.' }

    if (-not $restoreFailed -and (Test-DirHere $backupDir)) {
        try { Remove-Item -LiteralPath $backupDir -Recurse -Force; Report -Status 'Done' -Text ($BackupFolderName + '\ removed: everything in it is back in place.') }
        catch { Report -Status 'Warn' -Text ('Could not remove ' + $backupDir) -Detail $_.Exception.Message }
    }
    elseif ($restoreFailed) {
        Report -Status 'Warn' -Text ($BackupFolderName + '\ kept, because something could not be restored.') -Manual ('Copy the files you need back by hand from ' + $backupDir)
    }

    Write-Host ''
    Write-Chunk ('  ' + ([string][char]0x2500) * 70) 'Yellow'
    Write-Chunk '  ' $null -NoNewline
    Write-Chunk ($script:CountDone.ToString() + ' changed') 'Green' -NoNewline
    Write-Chunk '   ' $null -NoNewline
    Write-Chunk ($script:CountWarn.ToString() + ' warning' + $(if ($script:CountWarn -eq 1) { '' } else { 's' })) 'Yellow' -NoNewline
    Write-Chunk '   ' $null -NoNewline
    Write-Chunk ($script:CountFail.ToString() + ' failure' + $(if ($script:CountFail -eq 1) { '' } else { 's' })) 'Red'
    if ($script:CountFail -gt 0) { Exit-Installer 1 }
    Write-Host ''
    Write-Chunk '  War Wind is back to how it was before the remaster.' 'Gray'
    Exit-Installer 0
}

# ---------------------------------------------------------------------------------------
# 2. Back up what will be replaced
# ---------------------------------------------------------------------------------------

Write-Section 'Backup'

$manifest = Read-Manifest $backupDir
if ($null -ne $manifest) {
    Report -Status 'Ok' -Text ('Backup from an earlier install kept: ' + $backupDir) -Detail 'It holds the folder as it was before the first install; -Uninstall restores it.'
    $added = $false
    foreach ($rel in $ManagedFiles) {
        if (-not $manifest.Contains($rel)) {
            # A file a newer installer manages: record its pre-install state now.
            $src = Join-Safe $gameDir $rel
            if (Test-FileHere $src) {
                $dst = Join-Safe $backupDir $rel
                New-DirSafe ([IO.Path]::GetDirectoryName($dst))
                Copy-Item -LiteralPath $src -Destination $dst -Force
                $manifest[$rel] = 'present'
            }
            else { $manifest[$rel] = 'absent' }
            $added = $true
        }
    }
    if ($added) { Save-Manifest $backupDir $manifest }
}
else {
    try {
        New-DirSafe $backupDir
        $manifest = [ordered]@{}
        foreach ($rel in $ManagedFiles) {
            $src = Join-Safe $gameDir $rel
            # Everything present is saved as it is -- even an earlier, hand-installed copy of this
            # mod -- so -Uninstall returns the folder to exactly this state.
            if (Test-FileHere $src) {
                $dst = Join-Safe $backupDir $rel
                New-DirSafe ([IO.Path]::GetDirectoryName($dst))
                Copy-Item -LiteralPath $src -Destination $dst -Force
                $manifest[$rel] = 'present'
                $det = ''
                if ($rel -ieq 'dsound.dll' -and (Test-IsModDll $src)) { $det = 'this is an earlier build of the mod itself' }
                Report -Status 'Done' -Text ($rel + ': backed up.') -Detail $det
            }
            else { $manifest[$rel] = 'absent' }
        }
        Save-Manifest $backupDir $manifest
        Report -Status 'Done' -Text ('Backup manifest written: ' + $backupDir)
    }
    catch {
        Stop-Install -Text 'Could not create the backup.' -Detail $_.Exception.Message -Manual 'Check that the game folder is writable (games under Program Files need an administrator PowerShell).'
    }
}

# ---------------------------------------------------------------------------------------
# 3. Find the mod release
# ---------------------------------------------------------------------------------------

Write-Section 'Mod release'
New-DirSafe $script:Cache
Write-Chunk '  Cache   ' 'DarkGray' -NoNewline
Write-Host $script:Cache

$releases = Get-WebJson $Sources.ModReleases
if ($null -eq $releases) {
    $d = 'GitHub said: ' + $script:LastWebError
    if (-not $script:Token) { $d += "`nIf the repository is still private, set `$env:GITHUB_TOKEN to a token that can read it." }
    Report -Status 'Warn' -Text 'Could not list the mod releases.' -Detail $d
}
else {
    $list = @($releases | ForEach-Object { $_ })
    if ($Tag) { $pick = @($list | Where-Object { $_.tag_name -eq $Tag }) }
    else { $pick = @($list | Where-Object { -not $_.draft } | Sort-Object { [DateTime] $_.published_at } -Descending) }
    if ($pick.Count -gt 0) {
        $script:Release = $pick[0]
        $kind = ''
        if ($script:Release.draft) { $kind = ' (draft)' } elseif ($script:Release.prerelease) { $kind = ' (pre-release)' }
        Report -Status 'Ok' -Text ('Release ' + $script:Release.tag_name + $kind + ', ' + @($script:Release.assets).Count + ' files.')
        $script:ReleaseCache = Join-Safe $script:Cache $script:Release.tag_name
    }
    elseif ($Tag) { Report -Status 'Warn' -Text ('No release tagged ' + $Tag + '.') -Detail $Sources.ModHome }
    else { Report -Status 'Warn' -Text 'No published release yet.' -Detail $Sources.ModHome }
}
if (-not $script:ReleaseCache) { $script:ReleaseCache = Join-Safe $script:Cache 'local' }

$repoRoot = $null
if ($here) { $repoRoot = Split-Path -Parent $here }

# No published release reachable (private repo, offline) and no -LocalFiles: use the built
# release folder of a repository checkout next to this script (dist\<tag>\, else dist\).
if (-not $script:Release -and -not $LocalFiles -and $repoRoot) {
    $distRoot = Join-Safe $repoRoot 'dist'
    $distPick = $null
    if (Test-DirHere $distRoot) {
        $tagDirs = @(Get-ChildItem -LiteralPath $distRoot -Directory -ErrorAction SilentlyContinue |
                     Where-Object { Test-FileHere (Join-Safe $_.FullName 'dsound.dll') } |
                     Sort-Object LastWriteTime -Descending)
        if ($Tag) { $tagDirs = @($tagDirs | Where-Object { $_.Name -eq $Tag }) }
        if ($tagDirs.Count -gt 0) { $distPick = $tagDirs[0].FullName }
        elseif (-not $Tag -and (Test-FileHere (Join-Safe $distRoot 'dsound.dll'))) { $distPick = $distRoot }
    }
    if ($distPick) {
        $LocalFiles = $distPick
        Report -Status 'Info' -Text 'Using the built release files of this repository checkout.' -Detail $distPick
    }
}

$modDll = Resolve-ModAsset -Name 'dsound.dll'
$modIni = Resolve-ModAsset -Name 'WarWindHD.ini' -CheckoutPath (Join-Safe $repoRoot 'src\mod\WarWindHD.ini')
$modWwp = Resolve-ModAsset -Name 'feature-hires_720.wwp' -CheckoutPath (Join-Safe $repoRoot 'src\patchsets\feature-hires_720.wwp')
$modPalWwp = Resolve-ModAsset -Name 'fix-gdi-palette.wwp' -CheckoutPath (Join-Safe $repoRoot 'src\patchsets\fix-gdi-palette.wwp')
$modBackdrop = Resolve-ModAsset -Name 'backdrop.png' -CheckoutPath (Join-Safe $repoRoot 'assets\backdrop.png')
$modShader = Resolve-ModAsset -Name 'wwfx.hlsl' -CheckoutPath (Join-Safe $repoRoot 'src\mod\shaders\wwfx.hlsl')
$modDdraw = Resolve-ModAsset -Name 'ddraw.dll'

if (-not $modDll) {
    Stop-Install -Text 'The mod DLL (dsound.dll) could not be found or downloaded.' `
                 -Manual ('Download dsound.dll from ' + $Sources.ModHome + "`nand re-run with -LocalFiles <folder holding it>.")
}
if (-not (Test-IsModDll $modDll)) {
    Stop-Install -Text ('This dsound.dll is not the War Wind mod: ' + $modDll)
}
if (-not $modIni -or -not $modWwp -or -not $modPalWwp) {
    Stop-Install -Text 'WarWindHD.ini, feature-hires_720.wwp or fix-gdi-palette.wwp could not be found or downloaded.' -Manual ('Get them from ' + $Sources.ModHome + ' and pass -LocalFiles.')
}

# ---------------------------------------------------------------------------------------
# 4. cnc-ddraw
# ---------------------------------------------------------------------------------------

Write-Section 'cnc-ddraw (DirectDraw replacement)'

$ddrawPath = Join-Safe $gameDir 'ddraw.dll'
$vi = Get-FileVersionSafe $ddrawPath
$haveCnc = $false
$haveVer = $null
if ($vi -and $vi.ProductName -and $vi.ProductName -match '(?i)cnc-ddraw') {
    $haveCnc = $true
    $m = [regex]::Match([string]$vi.FileVersion, '^\s*(\d+(\.\d+){1,3})')
    if ($m.Success) { try { $haveVer = [version]$m.Groups[1].Value } catch { } }
}

# This project's build is cnc-ddraw that reads the backdrop= key.
function Test-IsBackdropDdraw
{
    param([string] $Path)
    $v = Get-FileVersionSafe $Path
    if (-not $v -or -not $v.ProductName -or $v.ProductName -notmatch '(?i)cnc-ddraw') { return $false }
    return (Test-BinaryMarker $Path 'backdrop')
}

$ourDdraw = $null
if ($modDdraw) {
    if (Test-IsBackdropDdraw $modDdraw) { $ourDdraw = $modDdraw }
    else { Report -Status 'Info' -Text ($modDdraw + ' is not this project''s cnc-ddraw build (no backdrop support).') }
}

if ($ourDdraw) {
    $ourHash = Get-Sha256 $ourDdraw
    if ((Test-FileHere $ddrawPath) -and $ourHash -and (Get-Sha256 $ddrawPath) -eq $ourHash) {
        Report -Status 'Ok' -Text 'This project''s cnc-ddraw build is already installed as ddraw.dll.'
    }
    else {
        if (Test-FileHere $ddrawPath) {
            if ($haveCnc) { Report -Status 'Info' -Text ('Replacing cnc-ddraw ' + $haveVer + ' with this project''s build, which adds the backdrop; the old one is in the backup.') }
            else { Report -Status 'Info' -Text 'Replacing the ddraw.dll that came with the game (a Direct3D 9 wrapper that changes the display mode); the original is in the backup.' }
        }
        try {
            Copy-Tracked -From $ourDdraw -To $ddrawPath
            $nv = Get-FileVersionSafe $ddrawPath
            $ver = ''
            if ($nv) { $ver = ' ' + ([string]$nv.FileVersion).Split(' ')[0] }
            Report -Status 'Done' -Text ('cnc-ddraw' + $ver + ' with the backdrop patch installed as ddraw.dll.') `
                   -Detail ('MIT licence, by FunkyFr3sh: ' + $Sources.CncDdrawHome + "`nOur patch and rebuild notes: " + $Sources.ModDdrawSource)
        }
        catch { Stop-Install -Text 'Could not write ddraw.dll.' -Detail $_.Exception.Message }
    }
}
elseif ($haveCnc -and $haveVer -and $haveVer -ge $CncMinVersion -and -not $Force) {
    Report -Status 'Warn' -Text ('This project''s cnc-ddraw build is not available; the installed cnc-ddraw ' + $haveVer + ' is kept (-Force replaces it).') `
           -Detail 'Without the backdrop patch the 640x480 screens show black bars instead of the stone backdrop.'
}
else {
    Report -Status 'Warn' -Text 'This project''s cnc-ddraw build is not available; using the official cnc-ddraw release.' `
           -Detail 'Without the backdrop patch the 640x480 screens show black bars instead of the stone backdrop.'
    if (Test-FileHere $ddrawPath) {
        if ($haveCnc) { Report -Status 'Info' -Text ('Replacing cnc-ddraw ' + $haveVer + '.') }
        else { Report -Status 'Info' -Text 'Replacing the ddraw.dll that came with the game (a Direct3D 9 wrapper that changes the display mode); the original is in the backup.' }
    }

    $cncDll = $null
    $localDll = Find-Local 'ddraw.dll'
    $localZip = Find-Local 'cnc-ddraw.zip'
    if ($localDll) {
        $lv = Get-FileVersionSafe $localDll
        if ($lv -and $lv.ProductName -match '(?i)cnc-ddraw') { $cncDll = $localDll; Report -Status 'Ok' -Text ('cnc-ddraw: using ' + $localDll) }
        else { Report -Status 'Warn' -Text ('Ignoring ' + $localDll + ': not cnc-ddraw.') }
    }
    if (-not $cncDll) {
        $zip = $localZip
        if (-not $zip) {
            $zip = Join-Safe $script:Cache 'cnc-ddraw-v7.1.0.0.zip'
            if (-not (Get-Download -Url $Sources.CncDdrawPinned -Dest $zip -Label 'cnc-ddraw 7.1.0.0')) {
                $zip = $null
                $rel = Get-WebJson $Sources.CncDdrawLatest
                if ($rel) {
                    $a = @($rel.assets | Where-Object { $_.name -ieq 'cnc-ddraw.zip' })
                    if ($a.Count -gt 0) {
                        $zip = Join-Safe $script:Cache ('cnc-ddraw-' + $rel.tag_name + '.zip')
                        if (-not (Get-Download -Url $a[0].browser_download_url -Dest $zip -Label ('cnc-ddraw ' + $rel.tag_name) -ExpectedSize ([long]$a[0].size))) { $zip = $null }
                    }
                }
            }
        }
        if ($zip) {
            try {
                $z = [IO.Compression.ZipFile]::OpenRead($zip)
                try {
                    $e = @($z.Entries | Where-Object { $_.FullName -ieq 'ddraw.dll' })
                    if ($e.Count -eq 0) { throw 'ddraw.dll is not in the archive' }
                    $cncDll = Join-Safe $script:Cache 'cnc-ddraw\ddraw.dll'
                    New-DirSafe ([IO.Path]::GetDirectoryName($cncDll))
                    [IO.Compression.ZipFileExtensions]::ExtractToFile($e[0], $cncDll, $true)
                }
                finally { $z.Dispose() }
            }
            catch { $cncDll = $null; Report -Status 'Fail' -Text 'Could not unpack cnc-ddraw.' -Detail $_.Exception.Message }
        }
    }

    if ($cncDll) {
        try {
            Copy-Tracked -From $cncDll -To $ddrawPath
            $nv = Get-FileVersionSafe $ddrawPath
            $ver = ''
            if ($nv) { $ver = ' ' + ([string]$nv.FileVersion).Split(' ')[0] }
            Report -Status 'Done' -Text ('cnc-ddraw' + $ver + ' installed as ddraw.dll.') -Detail ('MIT licence, by FunkyFr3sh: ' + $Sources.CncDdrawHome)
        }
        catch { Stop-Install -Text 'Could not write ddraw.dll.' -Detail $_.Exception.Message }
    }
    else {
        Stop-Install -Text 'cnc-ddraw is required and could not be obtained.' `
                     -Manual ('Download cnc-ddraw.zip from ' + $Sources.CncDdrawHome + "/releases`nand re-run with -LocalFiles <folder holding it>.")
    }
}

# ddraw.ini: the profile as is when there is no cnc-ddraw ini yet (or with -Force); otherwise the
# profile keys are set in the existing file and whatever else it holds is kept.
$iniPath = Join-Safe $gameDir 'ddraw.ini'
$haveIni = Read-TextSafe $iniPath
$want = $DdrawIni -replace "`r?`n", "`r`n"
$merged = $false
if ($haveIni -and -not $Force -and $haveIni -match '(?im)^\s*\[ddraw\]\s*$') {
    $want = $haveIni
    foreach ($e in (Get-IniEntries $DdrawIni)) { $want = Set-IniKey -Text $want -Section $e[0] -Key $e[1] -Value $e[2] }
    $want = $want -replace "`r?`n", "`r`n"
    $merged = $true
}
if ($haveIni -and ($haveIni -replace "`r?`n", "`r`n") -eq $want) {
    Report -Status 'Ok' -Text 'ddraw.ini already holds the War Wind profile.'
}
else {
    Write-TextTracked -Path $iniPath -Text $want
    $how = 'written'
    if ($merged) { $how = 'updated (your other settings in it are kept)' }
    Report -Status 'Done' -Text ('ddraw.ini ' + $how + ': borderless, integer scaling, stone backdrop, Windows 95 spoof, no display-mode change.')
}

# ---------------------------------------------------------------------------------------
# 5. The mod
# ---------------------------------------------------------------------------------------

Write-Section 'War Wind HD mod'

try {
    Copy-Tracked -From $modDll -To (Join-Safe $gameDir 'dsound.dll')
    Report -Status 'Done' -Text ('dsound.dll installed (' + (Format-Size (Get-Item -LiteralPath $modDll).Length) + ').') -Detail 'A proxy: every DirectSound call is forwarded to Windows'' own dsound.dll.'
    Copy-Tracked -From $modWwp -To (Join-Safe $gameDir 'WarWindHD\feature-hires_720.wwp')
    Report -Status 'Done' -Text 'WarWindHD\feature-hires_720.wwp installed (applied in memory at start-up; WW.EXE is never changed).'
    Copy-Tracked -From $modPalWwp -To (Join-Safe $gameDir 'WarWindHD\fix-gdi-palette.wwp')
    Report -Status 'Done' -Text 'WarWindHD\fix-gdi-palette.wwp installed (keeps the game colours after Windows dialogs such as save or load).'
}
catch { Stop-Install -Text 'Could not install the mod files.' -Detail $_.Exception.Message }

$backdropPath = Join-Safe $gameDir 'WarWindHD\backdrop.png'
if ($modBackdrop) {
    try {
        if ((Test-FileHere $backdropPath) -and (Get-Sha256 $backdropPath) -eq (Get-Sha256 $modBackdrop)) {
            Report -Status 'Ok' -Text 'WarWindHD\backdrop.png is already in place.'
        }
        else {
            Copy-Tracked -From $modBackdrop -To $backdropPath
            Report -Status 'Done' -Text ('WarWindHD\backdrop.png installed (' + (Format-Size (Get-Item -LiteralPath $modBackdrop).Length) + ').') -Detail 'The stone art cnc-ddraw draws around the 640x480 screens.'
        }
    }
    catch { Report -Status 'Warn' -Text 'Could not install WarWindHD\backdrop.png; the 640x480 screens show black bars.' -Detail $_.Exception.Message }
}
else {
    Report -Status 'Warn' -Text 'backdrop.png could not be found or downloaded; the 640x480 screens show black bars.' `
           -Manual ('Download backdrop.png from ' + $Sources.ModHome + ' and put it in ' + (Join-Safe $gameDir 'WarWindHD') + '.')
}

if ($modShader) {
    try {
        Copy-Tracked -From $modShader -To (Join-Safe $gameDir 'WarWindHD\shaders\wwfx.hlsl')
        Report -Status 'Done' -Text 'WarWindHD\shaders\wwfx.hlsl installed.' -Detail 'The Modern Graphics shaders (Direct3D 9 renderer; F9 switches them on and off in a mission).'
    }
    catch { Report -Status 'Warn' -Text 'Could not install WarWindHD\shaders\wwfx.hlsl; Modern Graphics stays off.' -Detail $_.Exception.Message }
}
else {
    Report -Status 'Warn' -Text 'wwfx.hlsl could not be found or downloaded; Modern Graphics stays off.' `
           -Manual ('Download wwfx.hlsl from ' + $Sources.ModHome + ' and put it in ' + (Join-Safe $gameDir 'WarWindHD\shaders') + '.')
}

# WarWindHD.ini: the release defaults, with the user's current values carried over (unless -Force)
# and the -Disable switches applied last.
$iniOut = Read-TextSafe $modIni
$userIniPath = Join-Safe $gameDir 'WarWindHD.ini'
$existing = Read-TextSafe $userIniPath
$kept = 0
if ($existing -and -not $Force) {
    $defaults = Get-IniEntries $iniOut
    foreach ($e in (Get-IniEntries $existing)) {
        $known = @($defaults | Where-Object { $_[0] -ieq $e[0] -and $_[1] -ieq $e[1] })
        if ($known.Count -gt 0 -and $known[0][2] -ne $e[2]) { $iniOut = Set-IniKey -Text $iniOut -Section $e[0] -Key $e[1] -Value $e[2]; $kept++ }
    }
}
$featureKeys = @{
    GridHotkeys = 'Controls'; CtrlGroups = 'Controls'; EdgeScroll = 'Controls'; WheelScroll = 'Controls'; MiddleDrag = 'Controls'
    DoubleClickType = 'Controls'; ModernClicks = 'Controls'; HiRes = 'Video'; PreciseClock = 'Video'; SmoothMotion = 'Video'
    CommandGrid = 'UI'; InGameSaveLoad = 'UI'; WideMenus = 'UI'; Modern = 'Graphics'; HDVideos = 'Cutscenes'
}
foreach ($f in $Disable) { $iniOut = Set-IniKey -Text $iniOut -Section $featureKeys[$f] -Key $f -Value '0' }
$iniOut = Set-IniKey -Text $iniOut -Section 'Dev' -Key 'Commands' -Value '0'
Write-TextTracked -Path $userIniPath -Text ($iniOut -replace "`r?`n", "`r`n")
$d = @()
if ($kept -gt 0) { $d += ($kept.ToString() + ' of your existing settings kept') }
if ($Disable.Count -gt 0) { $d += ('switched off: ' + ($Disable -join ', ')) }
Report -Status 'Done' -Text 'WarWindHD.ini written.' -Detail ($d -join "`n")

$on = @($featureKeys.Keys | Where-Object { $Disable -notcontains $_ } | Sort-Object)
Report -Status 'Info' -Text ('Features on: ' + ($on -join ', '))

# ---------------------------------------------------------------------------------------
# 6. HD cutscenes
# ---------------------------------------------------------------------------------------

Write-Section 'HD cutscenes'

$vidsHd = Join-Safe $gameDir 'Data\VIDS_HD'
$vidsSrc = Join-Safe $gameDir 'Data\VIDS'

function Get-CutsceneCoverage
{
    $total = 0
    $have = 0
    $missing = @()
    foreach ($race in $Races) {
        $dir = Join-Safe $vidsSrc $race
        if (-not (Test-DirHere $dir)) { continue }
        foreach ($avi in @(Get-ChildItem -LiteralPath $dir -File -Filter '*.AVI' -ErrorAction SilentlyContinue)) {
            $total++
            $mp4 = Join-Safe $vidsHd ($race + '\' + [IO.Path]::GetFileNameWithoutExtension($avi.Name).ToUpperInvariant() + '.mp4')
            if (Test-FileHere $mp4) { $have++ } else { $missing += ($race + '\' + $avi.Name) }
        }
    }
    return New-Object psobject -Property @{ Total = $total; Have = $have; Missing = $missing }
}

if ($SkipCutscenes) {
    Report -Status 'Skip' -Text 'Skipped (-SkipCutscenes): the original cutscenes play.'
}
elseif ($Disable -contains 'HDVideos') {
    Report -Status 'Skip' -Text 'Skipped: HDVideos is switched off.'
}
else {
    # What to fetch: local archives first, else every cutscene asset of the release.
    $parts = @()
    if ($LocalFiles -and (Test-DirHere $LocalFiles)) {
        foreach ($f in @(Get-ChildItem -LiteralPath $LocalFiles -File -Filter 'WarWind-HD-Cutscenes-*.zip' -ErrorAction SilentlyContinue)) {
            if ($f.Name -match $CutsceneAsset) { $parts += New-Object psobject -Property @{ Name = $f.Name; Size = $f.Length; Local = $f.FullName; Url = $null; ApiUrl = $null } }
        }
    }
    if ($parts.Count -eq 0 -and $script:Release) {
        foreach ($a in @($script:Release.assets)) {
            if ($a.name -match $CutsceneAsset) { $parts += New-Object psobject -Property @{ Name = $a.name; Size = [long]$a.size; Local = $null; Url = $a.browser_download_url; ApiUrl = $a.url } }
        }
    }
    $parts = @($parts | Sort-Object Name)

    $before = Get-CutsceneCoverage
    if ($before.Total -gt 0 -and $before.Have -eq $before.Total -and -not $Force) {
        Report -Status 'Ok' -Text ('All ' + $before.Total + ' cutscenes already have an HD version in Data\VIDS_HD; nothing to download (-Force re-fetches).')
    }
    elseif ($parts.Count -eq 0) {
        Report -Status 'Warn' -Text 'No cutscene archives in this release (or in -LocalFiles).' `
               -Manual ('Download the WarWind-HD-Cutscenes-*.zip files from ' + $Sources.ModHome + "`nand re-run with -LocalFiles <folder>, or unpack them into " + $vidsHd + ' yourself.')
    }
    else {
        $sum = [long]0
        $largest = [long]0
        foreach ($p in $parts) { $sum += $p.Size; if ($p.Size -gt $largest) { $largest = $p.Size } }
        $need = $sum
        if (-not @($parts | Where-Object { $_.Local }).Count) { $need += $largest }   # one archive in the cache at a time
        Report -Status 'Info' -Text ($parts.Count.ToString() + ' archives, ' + (Format-Size $sum) + '.')

        $free = Get-FreeSpace $gameDir
        if ($free -ge 0 -and $free -lt $need) {
            Report -Status 'Fail' -Text ('Not enough free space: ' + (Format-Size $need) + ' needed, ' + (Format-Size $free) + ' free on ' + [IO.Path]::GetPathRoot($gameDir)) `
                   -Manual 'Free some space and re-run, or install without cutscenes (-SkipCutscenes).'
        }
        elseif (Confirm-Step ('Download and unpack the HD cutscenes (' + (Format-Size $sum) + ')?') -DefaultYes) {
            New-DirSafe $vidsHd
            $safeEntry = '(?i)^(EA|OB|OPEN|SH|TH)/[A-Z0-9_]+\.mp4$'
            foreach ($p in $parts) {
                $zip = $p.Local
                $fromCache = $false
                if (-not $zip) {
                    $zip = Join-Safe $script:ReleaseCache $p.Name
                    $url = $p.Url
                    $accept = $null
                    if ($script:Token) { $url = $p.ApiUrl; $accept = 'application/octet-stream' }
                    if (-not (Get-Download -Url $url -Dest $zip -Label $p.Name -ExpectedSize $p.Size -Accept $accept)) { continue }
                    $fromCache = $true
                }
                try {
                    $z = [IO.Compression.ZipFile]::OpenRead($zip)
                    $n = 0
                    $skipped = 0
                    try {
                        $entries = @($z.Entries | Where-Object { $_.Length -gt 0 })
                        $i = 0
                        foreach ($e in $entries) {
                            $i++
                            $name = $e.FullName -replace '\\', '/'
                            if ($name -notmatch $safeEntry) { $skipped++; continue }
                            $dest = Join-Safe $vidsHd ($name -replace '/', '\')
                            if ((Test-FileHere $dest) -and (Get-Item -LiteralPath $dest).Length -eq $e.Length -and -not $Force) { $n++; continue }
                            Write-Chunk ("`r" + ('  [ .. ] ' + $p.Name + ': unpacking ' + $i + '/' + $entries.Count + '  ' + $name).PadRight(100)) 'DarkGray' -NoNewline
                            New-DirSafe ([IO.Path]::GetDirectoryName($dest))
                            [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest, $true)
                            $n++
                        }
                    }
                    finally { $z.Dispose() }
                    Write-Chunk ("`r" + (' ' * 102) + "`r") $null -NoNewline
                    $det = ''
                    if ($skipped -gt 0) { $det = $skipped.ToString() + ' unexpected entries ignored' }
                    Report -Status 'Done' -Text ($p.Name + ': ' + $n + ' videos in place.') -Detail $det
                    if ($fromCache -and -not $KeepDownloads) { try { Remove-Item -LiteralPath $zip -Force } catch { } }
                }
                catch {
                    Write-Host ''
                    Report -Status 'Fail' -Text ($p.Name + ': could not unpack.') -Detail ($_.Exception.Message + "`nThe archive may be damaged; delete it from " + $script:ReleaseCache + ' and re-run.')
                }
            }
        }
        else { Report -Status 'Skip' -Text 'Cutscenes not downloaded.' }
    }

    $after = Get-CutsceneCoverage
    if ($after.Total -gt 0) {
        if ($after.Have -eq $after.Total) { Report -Status 'Ok' -Text ('All ' + $after.Total + ' cutscenes have an HD version.') }
        elseif ($after.Have -gt 0) {
            Report -Status 'Warn' -Text ($after.Have.ToString() + ' of ' + $after.Total + ' cutscenes have an HD version; the others play in their original form.') `
                   -Detail ('missing: ' + (($after.Missing | Select-Object -First 8) -join ', ') + $(if ($after.Missing.Count -gt 8) { ', ...' } else { '' }))
        }
    }
}

# ---------------------------------------------------------------------------------------
# 7. Mark-of-the-web off everything we wrote
# ---------------------------------------------------------------------------------------

foreach ($p in $script:Changed) {
    try { if (Test-FileHere $p) { Unblock-File -LiteralPath $p -ErrorAction SilentlyContinue } } catch { }
}

# ---------------------------------------------------------------------------------------
# Summary and next steps
# ---------------------------------------------------------------------------------------

Write-Host ''
Write-Chunk ('  ' + ([string][char]0x2500) * 70) 'Yellow'
Write-Chunk '  ' $null -NoNewline
Write-Chunk ($script:CountDone.ToString() + ' installed') 'Green' -NoNewline
Write-Chunk '   ' $null -NoNewline
Write-Chunk ($script:CountWarn.ToString() + ' warning' + $(if ($script:CountWarn -eq 1) { '' } else { 's' })) 'Yellow' -NoNewline
Write-Chunk '   ' $null -NoNewline
Write-Chunk ($script:CountFail.ToString() + ' failure' + $(if ($script:CountFail -eq 1) { '' } else { 's' })) 'Red'

if ($script:Manual.Count -gt 0) {
    Write-Host ''
    Write-Chunk '  Still to do by hand:' 'White'
    $i = 1
    foreach ($a in $script:Manual) {
        $first = $true
        foreach ($line in ($a -split "`n")) {
            if (-not $line.Trim()) { continue }
            if ($first) { Write-Chunk ('   ' + $i + '. ' + $line.Trim()) 'DarkYellow'; $first = $false }
            else { Write-Chunk ('      ' + $line.Trim()) 'DarkYellow' }
        }
        $i++
    }
}

Write-Host ''
Write-Chunk '  Next:' 'White'
$steps = @()
$steps += 'Start War Wind from Steam as usual. It opens as a borderless window over the whole screen.'
$steps += 'WarWindHD.log next to WW.EXE should say "War Wind HD loaded" and list the hooks it installed.'
if ($Disable -notcontains 'GridHotkeys') { $steps += 'Command buttons now answer to the key at their slot: Q W E R T / A S D F G / Z X C V B.' }
if ($Disable -notcontains 'CtrlGroups') { $steps += 'Ctrl+1..0 assigns a control group; 1..0 recalls it.' }
if ($Disable -notcontains 'ModernClicks') { $steps += 'Right click moves and attacks with the selection; left click selects.' }
if ($Disable -notcontains 'PreciseClock') { $steps += 'The game clock now follows the system timer; GameSpeedPercent in WarWindHD.ini sets its pace (100 = the designed speed; the default, 70, is close to how the unmodified game plays).' }
if ($Disable -notcontains 'Modern') { $steps += 'Modern Graphics (soft shadows, lights, animated water, soft fog of war) is on; F9 switches it off and on in a mission, and the Options screen has a row for it.' }
$steps += 'Settings live in WarWindHD.ini; to undo everything: .\Install-WarWindRemaster.ps1 -Uninstall'
$i = 1
foreach ($s in $steps) { Write-Chunk ('   ' + $i + '. ' + $s) 'Gray'; $i++ }

Write-Host ''
if ($script:CountFail -gt 0) { Exit-Installer 1 }
Exit-Installer 0
