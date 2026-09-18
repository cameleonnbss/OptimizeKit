<#
.SYNOPSIS
  OptimizeKit - Windows Optimization Suite (PowerShell engine)

.DESCRIPTION
  Standalone PowerShell engine: gaming / privacy / debloat tweaks, driver helpers,
  network latency tests and junk cleanup. Everything is logged and every registry
  change is backed up to %LOCALAPPDATA%\OptimizeKit\ before being applied.

  Run WITHOUT admin  : powershell -File OptimizeKit.ps1 -User
  Run WITH admin kit : powershell -File OptimizeKit.ps1 -Full   (self-elevates)

  Tweaks curated from Chris Titus Tech's WinUtil (MIT), Microsoft/Valve docs and
  PC-gaming community guides. See docs/SOURCES.md in the repository.

.NOTES
  Version 1.0 - cameleonnbss - MIT license
#>
[CmdletBinding()]
param(
    [switch]$User,    # user menu (no admin needed)
    [switch]$Full,    # full kit (self-elevates when needed)
    [switch]$Silent,  # apply the gaming profile without menus
    [switch]$Restore  # restore Windows defaults for everything the kit touches
)

$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference    = 'SilentlyContinue'

# ----------------------------------------------------------------- logging
$Script:Dir  = Join-Path $env:LOCALAPPDATA 'OptimizeKit'
$Script:Log  = Join-Path $Script:Dir 'OptimizeKit-PowerShell.log'
New-Item -ItemType Directory -Force -Path $Script:Dir | Out-Null

function Write-Kit {
    param([string]$Msg, [ValidateSet('info','ok','warn','err')][string]$Kind = 'info')
    $stamp  = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $color  = @{ info = 'Gray'; ok = 'Green'; warn = 'Yellow'; err = 'Red' }[$Kind]
    $prefix = @{ info = '[INFO]'; ok = '[OK]  '; warn = '[WARN]'; err = '[FAIL]' }[$Kind]
    Write-Host ("  {0} {1}" -f $prefix, $Msg) -ForegroundColor $color
    Add-Content -Path $Script:Log -Value "[$stamp] $prefix $Msg" -Encoding UTF8
}
function Write-Section { param([string]$Title)
    Write-Host ""
    Write-Host ("  === {0} ===" -f $Title) -ForegroundColor Cyan
    Add-Content -Path $Script:Log -Value "=== $Title ===" -Encoding UTF8
}

# ----------------------------------------------------------------- helpers
function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Backup-Registry {
    Write-Section 'Registry backup'
    $stamp = Get-Date -Format 'yyyyMMdd_HHMMSS'
    $keys = @(
        'HKCU\Software\Microsoft\GameBar',
        'HKCU\System\GameConfigStore',
        'HKCU\Control Panel\Mouse',
        'HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager',
        'HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection',
        'HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters'
    )
    foreach ($k in $keys) {
        $safe = ($k -replace '[\\:]', '_')
        $file = Join-Path $Script:Dir ("backup_{0}_{1}.reg" -f $stamp, $safe)
        reg export $k $file /y | Out-Null
    }
    Write-Kit ("Backups written to {0}" -f $Script:Dir) 'ok'
}

function Set-RegDword {
    param([string]$Path, [string]$Name, [int]$Value)
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    New-ItemProperty -Path $Path -Name $Name -PropertyType DWord -Value $Value -Force | Out-Null
}
function Set-RegString {
    param([string]$Path, [string]$Name, [string]$Value)
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    New-ItemProperty -Path $Path -Name $Name -PropertyType String -Value $Value -Force | Out-Null
}
function Disable-KitService {
    param([string]$Name, [string]$Friendly)
    $svc = Get-Service -Name $Name -ErrorAction SilentlyContinue
    if (-not $svc) { Write-Kit ("service {0} not present, skipped" -f $Name) 'warn'; return }
    Stop-Service $Name -Force -ErrorAction SilentlyContinue
    Set-Service $Name -StartupType Disabled
    Write-Kit ("service {0} ({1}) disabled" -f $Name, $Friendly) 'ok'
}

# ----------------------------------------------------------------- GAMING
function Enable-GameMode {
    Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1
    Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode'  1
    Write-Kit 'Game Mode enabled'
}
function Disable-GameDVR {
    Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 0
    Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_FSEBehaviorMode' 2
    Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_HonorUserFSEBehaviorMode' 1
    Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_EFSEFeatureFlags' 0
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 0
    Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'UseNexusForGameBarEnabled' 0
    Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'ShowStartupPanel' 0
    Write-Kit 'Game DVR / Game Bar background capture disabled (source: WinUtil)'
}
function Enable-HAGS {
    Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode' 2
    Write-Kit 'Hardware-accelerated GPU scheduling ON (reboot required)'
}
function Disable-MPO {
    Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'OverlayTestMode' 5
    Write-Kit 'Multi-Plane Overlay disabled - fixes stutter/flicker on 24H2'
}
function Enable-HighTimerResolution {
    Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\kernel' 'GlobalTimerResolutionRequests' 1
    bcdedit /set disabledynamictick yes | Out-Null
    bcdedit /set useplatformclock false  | Out-Null
    Write-Kit 'Global timer resolution 0.5ms requests enabled (source: TimerResolution-Optimization)'
}
function Set-NetworkGaming {
    $mm = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile'
    Set-RegDword $mm 'NetworkThrottlingIndex' 0xFFFFFFFF
    Set-RegDword $mm 'SystemResponsiveness' 0
    $g = "$mm\Tasks\Games"
    Set-RegDword $g 'GPU Priority' 8
    Set-RegDword $g 'Priority' 6
    Set-RegString $g 'Scheduling Category' 'High'
    Set-RegString $g 'SFIO Priority' 'High'
    Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces' -ErrorAction SilentlyContinue |
        ForEach-Object {
            Set-RegDword $_.PSPath 'TcpAckFrequency' 1
            Set-RegDword $_.PSPath 'TCPNoDelay' 1
        }
    Write-Kit 'Gaming network stack: no Nagle, no throttling, MMCSS games priority'
}
function Set-MousePrecision {
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '0'
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '0'
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '0'
    Write-Kit 'Mouse acceleration disabled (raw 1:1 input)'
}
function Set-VisualPerformance {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 2
    Set-RegString 'HKCU:\Control Panel\Desktop\WindowMetrics' 'MinAnimate' '0'
    Set-RegString 'HKCU:\Control Panel\Desktop' 'MenuShowDelay' '0'
    Write-Kit 'Visual effects set to best performance + instant menus'
}
function Set-UltimatePowerPlan {
    powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 2>$null | Out-Null
    powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c 2>$null | Out-Null }
    Write-Kit ('Power plan: ' + (powercfg /getactivescheme))
}
function Set-Win32Priority {
    Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\PriorityControl' 'Win32PrioritySeparation' 0x26
    Write-Kit 'Win32PrioritySeparation = 0x26 (foreground quantum boost)'
}
function Set-GpuPreference {
    Set-RegString 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' 'SwapEffectUpgradeEnable=1;'
    Write-Kit 'Global GPU preference: high performance'
}
function Disable-XboxServices {
    foreach ($s in 'XblAuthManager','XblGameSave','XboxNetApiSvc','XboxGipSvc') {
        Disable-KitService $s 'Xbox live'
    }
    Write-Kit 'Xbox services disabled (Game Pass on PC will stop working)' 'warn'
}
function Disable-FullscreenOptimizations {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' '~ DISABLEDXMAXIMIZEDWINDOWEDMODE' 1
    Write-Kit 'Fullscreen optimizations disabled globally'
}

# ----------------------------------------------------------------- PRIVACY
function Disable-Telemetry {
    Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection' 'AllowTelemetry' 0
    Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection' 'AllowTelemetry' 0
    Disable-KitService 'DiagTrack' 'connected user experiences'
    Disable-KitService 'dmwappushservice' 'telemetry'
    Write-Kit 'Telemetry disabled (AllowTelemetry=0, source: WinUtil)'
}
function Disable-AdvertisingId {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo' 'Enabled' 0
    Write-Kit 'Advertising ID off'
}
function Disable-ActivityHistory {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'PublishUserActivities' 0
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'UploadUserActivities' 0
    Write-Kit 'Activity history / timeline off'
}
function Remove-BingFromSearch {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BingSearchEnabled' 0
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'CortanaConsent' 0
    Write-Kit 'Bing removed from start menu search'
}
function Disable-TailoredExperiences {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'TailoredExperiencesWithDiagnosticDataEnabled' 0
    Write-Kit 'Tailored experiences off'
}
function Disable-TelemetryTasks {
    $tasks = @(
        '\Microsoft\Windows\Customer Experience Improvement Program\Consolidator',
        '\Microsoft\Windows\Customer Experience Improvement Program\UsbCeip',
        '\Microsoft\Windows\Customer Experience Improvement Program\Uploader',
        '\Microsoft\Windows\Application Experience\Microsoft Compatibility Appraiser',
        '\Microsoft\Windows\Application Experience\ProgramDataUpdater',
        '\Microsoft\Windows\Autochk\Proxy',
        '\Microsoft\Windows\DiskDiagnostic\Microsoft-Windows-DiskDiagnosticDataCollector',
        '\Microsoft\Windows\Feedback\Siuf\DmClient',
        '\Microsoft\Windows\Feedback\Siuf\DmClientOnScenarioDownload'
    )
    $n = 0
    foreach ($t in $tasks) { if (schtasks /Change /TN $t /Disable 2>$null) { $n++ } }
    Write-Kit ("{0} telemetry scheduled tasks disabled" -f $n) 'ok'
}
function Remove-Copilot {
    Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsCopilot' 'TurnOffWindowsCopilot' 1
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowCopilotButton' 0
    Write-Kit 'Windows Copilot removed'
}
function Disable-EdgeBackground {
    Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Microsoft Edge' 'BackgroundModeEnabled' 0
    Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Microsoft Edge' 'StartupBoostEnabled' 0
    Write-Kit 'Edge background mode + startup boost disabled'
}

# ----------------------------------------------------------------- DEBLOAT
function Remove-BloatApps {
    $pkgs = @(
        'Microsoft.549981C3F5F10','Microsoft.BingFinance','Microsoft.BingNews',
        'Microsoft.BingSports','Microsoft.BingWeather','Microsoft.BingSearch',
        'Microsoft.Clipchamp','Microsoft.GamingApp','Microsoft.GetHelp','Microsoft.Getstarted',
        'Microsoft.MicrosoftOfficeHub','Microsoft.MicrosoftSolitaireCollection','Microsoft.MixedReality.Portal',
        'Microsoft.News','Microsoft.PowerAutomateDesktop','Microsoft.SkypeApp','Microsoft.Todos',
        'Microsoft.Whiteboard','Microsoft.WindowsFeedbackHub','Microsoft.WindowsMaps',
        'Microsoft.Xbox.TCUI','Microsoft.XboxApp','Microsoft.XboxGameOverlay','Microsoft.XboxGamingOverlay',
        'Microsoft.XboxSpeechToTextOverlay','Microsoft.YourPhone','Microsoft.ZuneMusic','Microsoft.ZuneVideo',
        'MicrosoftTeams','Clipchamp.Clipchamp','Microsoft.Copilot'
    )
    $n = 0
    foreach ($p in $pkgs) {
        Get-AppxPackage -Name $p -AllUsers -ErrorAction SilentlyContinue |
            Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue
        if (-not (Get-AppxPackage -Name $p -ErrorAction SilentlyContinue)) { $n++ }
    }
    Write-Kit ("bloat apps processed ({0} gone)" -f $n) 'ok'
}
function Disable-OneDrive {
    Get-Process onedrive -ErrorAction SilentlyContinue | Stop-Process -Force
    foreach ($setup in "$env:WinDir\SysWOW64\OneDriveSetup.exe", "$env:WinDir\System32\OneDriveSetup.exe") {
        if (Test-Path $setup) { Start-Process $setup '/uninstall' -Wait }
    }
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer' 'NoOneDriveSidebar' 1
    Write-Kit 'OneDrive uninstalled (files stay in your local OneDrive folder)'
}
function Disable-SysMain { Disable-KitService 'SysMain' 'superfetch' }
function Disable-SearchIndexing { Disable-KitService 'WSearch' 'search indexing' }
function Disable-BackgroundApps {
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications' 'GlobalUserDisabled' 1
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BackgroundAppGlobalToggle' 0
    Write-Kit 'Background apps disabled'
}

# ----------------------------------------------------------------- DRIVERS
function Show-DriverInfo {
    Write-Section 'GPU / driver info'
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
    if ($gpu) {
        Write-Host ("  GPU     : {0}" -f $gpu.Name)
        Write-Host ("  Driver  : {0}" -f $gpu.DriverVersion)
        Write-Host ("  Date    : {0}" -f $gpu.DriverDate)
        Write-Host ("  RAM     : {0} MB" -f [int]($gpu.AdapterRAM / 1MB))
    }
}
function Open-VendorPage {
    $gpu = (Get-CimInstance Win32_VideoController | Select-Object -First 1).Name
    $url = 'https://www.nvidia.com/Download/index.aspx'
    if     ($gpu -match 'Radeon|AMD')   { $url = 'https://www.amd.com/en/support' }
    elseif ($gpu -match 'Intel|Iris|Arc') { $url = 'https://www.intel.com/content/www/us/en/download-center/home.html' }
    Write-Kit ("opening {0}" -f $url) 'info'
    Start-Process $url
}
function Invoke-DriverScan {
    Write-Kit 'asking Windows Update to scan for driver updates'
    Start-Process 'UsoClient' 'StartInteractiveScan' -WindowStyle Hidden
}

# ----------------------------------------------------------------- NETWORK
$Script:PingTargets = @(
    @{ Name = 'Cloudflare'; Host = '1.1.1.1' },
    @{ Name = 'Google';     Host = '8.8.8.8' },
    @{ Name = 'Gateway';    Host = ((Get-NetRoute -DestinationPrefix 0.0.0.0/0 -ErrorAction SilentlyContinue |
        Sort-Object RouteMetric | Select-Object -First 1).NextHop) }
)
function Test-Latency {
    Write-Section 'Network latency'
    foreach ($t in $Script:PingTargets) {
        if (-not $t.Host) { continue }
        $p = Test-Connection -ComputerName $t.Host -Count 4 -ErrorAction SilentlyContinue
        if ($p) {
            $avg = [int](($p | Measure-Object -Property ResponseTime -Average).Average)
            $color = if ($avg -lt 30) { 'Green' } elseif ($avg -lt 80) { 'Yellow' } else { 'Red' }
            Write-Host ("  {0,-12} {1,-16} {2} ms" -f $t.Name, $t.Host, $avg) -ForegroundColor $color
        } else {
            Write-Host ("  {0,-12} {1,-16} timeout" -f $t.Name, $t.Host) -ForegroundColor Red
        }
    }
}

# ----------------------------------------------------------------- CLEAN
function Invoke-JunkCleanup {
    Write-Section 'Junk cleanup'
    $targets = @(
        $env:TEMP,
        (Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\INetCache'),
        (Join-Path $env:LOCALAPPDATA 'CrashDumps'),
        (Join-Path $env:LOCALAPPDATA 'NVIDIA\DXCache'),
        (Join-Path $env:LOCALAPPDATA 'NVIDIA\GLCache'),
        (Join-Path $env:LOCALAPPDATA 'AMD\DxCache'),
        (Join-Path $env:LOCALAPPDATA 'D3DSCache'),
        (Join-Path $env:WINDIR 'Temp')
    ) | Where-Object { $_ -and (Test-Path $_) }
    $before = (Get-PSDrive C).Free
    foreach ($t in $targets) {
        Get-ChildItem $t -Recurse -Force -ErrorAction SilentlyContinue |
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
    Clear-RecycleBin -Force -ErrorAction SilentlyContinue
    $after = (Get-PSDrive C).Free
    Write-Kit ("freed {0:N1} MB" -f (($after - $before) / 1MB)) 'ok'
}
function Invoke-ComponentCleanup {
    Write-Kit 'DISM component cleanup (can take several minutes)...'
    Dism.exe /Online /Cleanup-Image /StartComponentCleanup /Quiet
    Write-Kit 'DISM component cleanup done' 'ok'
}

# ----------------------------------------------------------------- profiles
function Invoke-GamingProfile {
    Write-Section 'GAMING profile'
    Enable-GameMode; Disable-GameDVR; Set-NetworkGaming
    if (Test-Admin) {
        Enable-HAGS; Disable-MPO; Enable-HighTimerResolution
        Set-UltimatePowerPlan; Set-Win32Priority; Disable-XboxServices
    } else {
        Write-Kit 'admin-only tweaks skipped (HAGS, MPO, timer, power plan, xbox svc)' 'warn'
    }
    Set-MousePrecision; Set-VisualPerformance; Set-GpuPreference
    Invoke-JunkCleanup
}
function Invoke-PrivacyProfile {
    Write-Section 'PRIVACY profile'
    Disable-AdvertisingId; Disable-ActivityHistory; Remove-BingFromSearch
    Disable-TailoredExperiences; Disable-EdgeBackground
    if (Test-Admin) {
        Disable-Telemetry; Disable-TelemetryTasks; Remove-Copilot
    } else {
        Write-Kit 'admin-only tweaks skipped (telemetry svc, CEIP tasks, copilot policy)' 'warn'
    }
}
function Invoke-DebloatProfile {
    Write-Section 'DEBLOAT profile'
    Disable-BackgroundApps
    if (Test-Admin) {
        Remove-BloatApps; Disable-OneDrive
        Disable-SysMain; Disable-SearchIndexing
    } else {
        Write-Kit 'admin-only debloat skipped (appx removal, services)' 'warn'
    }
}
function Invoke-FullKit {
    Invoke-GamingProfile
    Invoke-PrivacyProfile
    Invoke-DebloatProfile
    if (Test-Admin) { Invoke-ComponentCleanup }
    Write-Section 'FULL KIT COMPLETE'
    Write-Kit 'reboot recommended to activate HAGS / timer / power plan' 'warn'
}

function Invoke-RestoreDefaults {
    Write-Section 'Restore Windows defaults'
    Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1
    Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 1
    Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1
    Remove-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'OverlayTestMode' -ErrorAction SilentlyContinue
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '1'
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '6'
    Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '10'
    Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile' 'NetworkThrottlingIndex' 10
    Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile' 'SystemResponsiveness' 20
    Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\PriorityControl' 'Win32PrioritySeparation' 2
    powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e 2>$null
    Write-Kit 'defaults restored (games/MMCSS/mouse/DVR/power)' 'ok'
}

# ----------------------------------------------------------------- system info
function Show-SystemInfo {
    Write-Section 'System information'
    $os  = Get-CimInstance Win32_OperatingSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
    $admin = Test-Admin
    Write-Host ("  Computer : {0}  ({1})" -f $env:COMPUTERNAME, $env:USERNAME)
    Write-Host ("  OS       : {0} build {1}" -f $os.Caption, $os.BuildNumber)
    Write-Host ("  CPU      : {0} [{1} threads]" -f $cpu.Name.Trim(), $cpu.NumberOfLogicalProcessors)
    Write-Host ("  RAM      : {0:N1} GB total / {1:N1} GB free" -f ($os.TotalVisibleMemorySize/1MB), ($os.FreePhysicalMemory/1MB))
    Write-Host ("  GPU      : {0}  (driver {1})" -f $gpu.Name, $gpu.DriverVersion)
    Write-Host ("  Admin    : {0}" -f $(if ($admin) { 'YES - full kit available' } else { 'no - user kit' }))
    $up = (Get-Date) - $os.LastBootUpTime
    Write-Host ("  Uptime   : {0}d {1:00}h {2:00}m" -f $up.Days, $up.Hours, $up.Minutes)
}

# ----------------------------------------------------------------- menus
function Show-Banner {
    Clear-Host
    Write-Host ""
    Write-Host "  /$$$$$$  /$$   /$$ /$$$$$$$$ /$$   /$$ /$$$$$$$$ /$$$$$$$$" -ForegroundColor Cyan
    Write-Host " | $$__  $$| $$  | $$|__  $$__/| $$  /$$/| $$_____/| $$_____/" -ForegroundColor Cyan
    Write-Host " | $$  \ $$| $$  | $$   | $$   | $$ /$$/ | $$      | $$      " -ForegroundColor Cyan
    Write-Host " | $$  | $$| $$  | $$   | $$   | $$$$$$/  | $$$$$   | $$$$$   " -ForegroundColor Cyan
    Write-Host " | $$  | $$| $$  | $$   | $$   | $$  $$   | $$__/   | $$__/   " -ForegroundColor Cyan
    Write-Host " | $$  | $$| $$  | $$   | $$   | $$\  $$  | $$      | $$      " -ForegroundColor Cyan
    Write-Host " | $$$$$$$/|  $$$$$$/   | $$   | $$ \  $$ | $$$$$$$$| $$$$$$$$" -ForegroundColor Cyan
    Write-Host " |_______/  \______/    |__/   |__/  \__/ |________/|________/" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Windows Optimization Suite - PowerShell engine v1.0" -ForegroundColor DarkGray
    Write-Host ("  log: {0}" -f $Script:Log) -ForegroundColor DarkGray
    Write-Host ("  mode: {0}" -f $(if (Test-Admin) { 'ADMINISTRATOR' } else { 'user' })) -ForegroundColor $(if (Test-Admin) { 'Green' } else { 'Yellow' })
    Write-Host ""
}

function Pause-Kit {
    Write-Host ""
    Read-Host  "  Press ENTER to continue" | Out-Null
}

function Show-MainMenu {
    $isAdmin = Test-Admin
    while ($true) {
        Show-Banner
        Write-Host "  $(if ($isAdmin) { '-- ADMIN MENU --' } else { '-- USER MENU (run OptimizeKit-admin.bat for the full kit) --' })" -ForegroundColor Magenta
        Write-Host ""
        if ($isAdmin) {
            Write-Host "   1. System information" 
            Write-Host "   2. GAMING profile     (DVR off, network, timer, HAGS, MPO, power)"
            Write-Host "   3. PRIVACY profile    (telemetry, ads, tracking)"
            Write-Host "   4. DEBLOAT profile    (bloat apps, OneDrive, services)"
            Write-Host "   5. FULL KIT           (everything + DISM cleanup)"
            Write-Host "   6. Individual tweaks  (numbered list)"
            Write-Host "   7. Game boost         (priority for a running process)"
            Write-Host "   8. Network latency    (ping targets)"
            Write-Host "   9. Junk cleanup       (temp, caches, recycle bin)"
            Write-Host "  10. Drivers            (info, vendor page, WU scan)"
            Write-Host "  11. Registry backup"
            Write-Host "  12. Restore Windows defaults"
            Write-Host "   0. Exit"
        } else {
            Write-Host "   1. System information"
            Write-Host "   2. USER GAMING tweaks (GameMode, DVR, mouse, visuals)"
            Write-Host "   3. USER PRIVACY tweaks (ads, activity, bing)"
            Write-Host "   4. Network latency    (ping targets)"
            Write-Host "   5. Junk cleanup       (user temp + recycle bin)"
            Write-Host "   6. Drivers            (info, vendor page)"
            Write-Host "   7. Registry backup"
            Write-Host "   0. Exit"
        }
        Write-Host ""
        $c = Read-Host "  Choice"
        if ($isAdmin) {
            switch ($c) {
                '0'  { return }
                '1'  { Show-SystemInfo; Pause-Kit }
                '2'  { Invoke-GamingProfile;  Pause-Kit }
                '3'  { Invoke-PrivacyProfile; Pause-Kit }
                '4'  { Invoke-DebloatProfile; Pause-Kit }
                '5'  { Invoke-FullKit;        Pause-Kit }
                '6'  { Show-TweakMenu; Pause-Kit }
                '7'  { Show-GameBoost; Pause-Kit }
                '8'  { Test-Latency;   Pause-Kit }
                '9'  { Invoke-JunkCleanup; Pause-Kit }
                '10' { Show-DriverInfo; Open-VendorPage; Pause-Kit }
                '11' { Backup-Registry; Pause-Kit }
                '12' { Invoke-RestoreDefaults; Pause-Kit }
                default { }
            }
        } else {
            switch ($c) {
                '0' { return }
                '1' { Show-SystemInfo; Pause-Kit }
                '2' { Enable-GameMode; Disable-GameDVR; Set-MousePrecision; Set-VisualPerformance
                      Write-Kit 'user gaming tweaks applied' 'ok'; Pause-Kit }
                '3' { Disable-AdvertisingId; Disable-ActivityHistory; Remove-BingFromSearch
                      Write-Kit 'user privacy tweaks applied' 'ok'; Pause-Kit }
                '4' { Test-Latency; Pause-Kit }
                '5' { Invoke-JunkCleanup; Pause-Kit }
                '6' { Show-DriverInfo; Open-VendorPage; Pause-Kit }
                '7' { Backup-Registry; Pause-Kit }
                default { }
            }
        }
    }
}

function Show-TweakMenu {
    Write-Section 'Individual tweaks'
    Write-Host "   1. Enable Game Mode            7. Ultimate power plan"
    Write-Host "   2. Disable Game DVR/Game Bar   8. Win32PrioritySeparation"
    Write-Host "   3. HAGS on                     9. GPU preference high perf"
    Write-Host "   4. MPO off                    10. Disable fullscreen opts"
    Write-Host "   5. High timer resolution      11. Disable Xbox services"
    Write-Host "   6. Gaming network stack       12. Disable SysMain"
    Write-Host "   0. Back"
    $c = Read-Host "  Choice"
    switch ($c) {
        '1'  { Enable-GameMode }
        '2'  { Disable-GameDVR }
        '3'  { Enable-HAGS }
        '4'  { Disable-MPO }
        '5'  { Enable-HighTimerResolution }
        '6'  { Set-NetworkGaming }
        '7'  { Set-UltimatePowerPlan }
        '8'  { Set-Win32Priority }
        '9'  { Set-GpuPreference }
        '10' { Disable-FullscreenOptimizations }
        '11' { Disable-XboxServices }
        '12' { Disable-SysMain }
        default { }
    }
}

function Show-GameBoost {
    Write-Section 'Game boost'
    $procs = Get-Process | Where-Object { $_.MainWindowTitle } | Sort-Object ProcessName -Unique
    $i = 1
    foreach ($p in $procs) {
        Write-Host ("   {0,2}. {1,-28} (PID {2})" -f $i, $p.ProcessName, $p.Id)
        $i++
    }
    $c = Read-Host "  Boost which number (0=cancel)"
    $n = 0
    if ([int]::TryParse($c, [ref]$n) -and $n -ge 1 -and $n -lt $i) {
        $target = $procs[$n - 1]
        foreach ($p in (Get-Process -Name $target.ProcessName -ErrorAction SilentlyContinue)) {
            $p.PriorityClass = 'High'
        }
        Write-Kit ("{0} boosted to High priority" -f $target.ProcessName) 'ok'
    }
}

# ----------------------------------------------------------------- entry
if ($Restore) { Invoke-RestoreDefaults; exit 0 }

if (-not (Test-Admin) -and -not $User) {
    Write-Host "  Admin rights are needed for the full kit - relaunching elevated..." -ForegroundColor Yellow
    Start-Process powershell "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Full" -Verb RunAs
    exit 0
}

Write-Kit ("OptimizeKit PowerShell engine started (admin={0})" -f (Test-Admin)) 'info'
Backup-Registry

if ($Silent) { Invoke-FullKit; exit 0 }

Show-MainMenu
Write-Kit 'session ended' 'info'
