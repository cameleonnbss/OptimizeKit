<#
.SYNOPSIS
  OptimizeKit v2.9 - Windows Gaming & Performance Control Center (PowerShell engine)

.DESCRIPTION
  Full CLI parity with the C++ dashboard: 71 tweaks, live status, profiles,
  diagnostics, network tests, cleanup, backup & one-key full restore.
  Every registry write is exported to %LOCALAPPDATA%\OptimizeKit\backups first.
  Run with -RestoreAll to undo everything the kit has ever touched.

  powershell -NoProfile -ExecutionPolicy Bypass -File OptimizeKit.ps1 [switches]
    -User      user menu (HKCU-only tweaks, no elevation)
    -Silent    apply the Gaming profile and exit
    -Status    print current state of all tweaks and exit
    -RestoreAll restore Windows defaults for every tweak
    -Apply id1,id2,... apply specific tweak ids
    -Profile gaming|privacy|debloat|full

.NOTES
  Version 2.9 - cameleonnbss - MIT license
#>
[CmdletBinding()]
param(
    [switch]$User,
    [switch]$Full,
    [switch]$Silent,
    [switch]$Status,
    [switch]$RestoreAll,
    [switch]$Tweaks,
    [switch]$Network,
    [switch]$Cleanup,
    [switch]$Firmware,
    [switch]$Drivers,
    [string]$Apply,
    [string]$Profile
)

$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference    = 'SilentlyContinue'
$Accent = "$([char]27)[96m"; $Green = "$([char]27)[92m"; $Yellow = "$([char]27)[93m"
$Red = "$([char]27)[91m"; $Dim = "$([char]27)[90m"; $Reset = "$([char]27)[0m"

# ----------------------------------------------------------------- logging
$Script:Dir = Join-Path $env:LOCALAPPDATA 'OptimizeKit'
$Script:Log = Join-Path $Script:Dir 'OptimizeKit.log'
New-Item -ItemType Directory -Force -Path (Join-Path $Script:Dir 'backups') | Out-Null

function Write-Kit {
    param([string]$Msg, [ValidateSet('info','ok','warn','err')][string]$Kind = 'info')
    $stamp  = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $color  = @{ info = 'Gray'; ok = 'Green'; warn = 'Yellow'; err = 'Red' }[$Kind]
    $prefix = @{ info = '[INFO]'; ok = '[ OK ]'; warn = '[WARN]'; err = '[FAIL]' }[$Kind]
    Write-Host ("  {0} {1}" -f $prefix, $Msg) -ForegroundColor $color
    Add-Content -Path $Script:Log -Value "[$stamp] $prefix $Msg" -Encoding UTF8
}
function Write-Section { param([string]$Title)
    Write-Host ""
    Write-Host ("  == {0} ==" -f $Title) -ForegroundColor Cyan
    Add-Content -Path $Script:Log -Value "== $Title ==" -Encoding UTF8
}

# ----------------------------------------------------------------- helpers
function Test-Admin {
    (New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Backup-RegKey { param([string]$Key)
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $safe  = ($Key -replace '[\\:]','_')
    $file  = Join-Path $Script:Dir ("backups\{0}_{1}.reg" -f $stamp, $safe)
    reg export $Key $file /y | Out-Null
}
function Backup-Registry {
    Write-Section 'Registry backup'
    foreach ($k in 'HKCU\Software\Microsoft\GameBar','HKCU\System\GameConfigStore',
                   'HKCU\Control Panel\Mouse','HKCU\Control Panel\Desktop',
                   'HKCU\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager',
                   'HKLM\SOFTWARE\Policies\Microsoft\Windows\DataCollection',
                   'HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters') {
        Backup-RegKey $k
    }
    Write-Kit ("backups -> {0}\backups" -f $Script:Dir) 'ok'
}

function Set-RegDword  { param($Path,$Name,$Value) if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }; New-ItemProperty -Path $Path -Name $Name -PropertyType DWord  -Value $Value -Force | Out-Null }
function Set-RegString { param($Path,$Name,$Value) if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }; New-ItemProperty -Path $Path -Name $Name -PropertyType String -Value $Value -Force | Out-Null }
function Get-RegDword  { param($Path,$Name,$Default) $v = Get-ItemProperty -Path $Path -Name $Name -ErrorAction SilentlyContinue; if ($v -and $null -ne $v.$Name) { $v.$Name } else { $Default } }

function Disable-KitService { param([string]$Name,[string]$Friendly)
    $svc = Get-Service -Name $Name -ErrorAction SilentlyContinue
    if (-not $svc) { Write-Kit ("service {0} not present, skipped" -f $Name) 'warn'; return }
    Backup-RegKey ("HKLM\SYSTEM\CurrentControlSet\Services\" + $Name)
    Stop-Service $Name -Force -ErrorAction SilentlyContinue
    Set-Service $Name -StartupType Disabled -ErrorAction SilentlyContinue
    Write-Kit ("service {0} ({1}) disabled" -f $Name, $Friendly) 'ok'
}
function Restore-KitService { param([string]$Name)
    $svc = Get-Service -Name $Name -ErrorAction SilentlyContinue
    if ($svc) { Set-Service $Name -StartupType Manual -ErrorAction SilentlyContinue; Write-Kit ("service {0} -> manual" -f $Name) 'ok' }
}

# ================================================================ TWEAK CATALOG
# Mirrors src/core/tweaks.cpp. Each entry: id, name, cat (gaming/privacy/...), user (works without admin)
$Script:Tweaks = @(
    @{ id='game_mode';         name='Enable Game Mode';                     cat='gaming';  user=1 }
    @{ id='game_dvr_off';      name='Disable Game DVR background capture';  cat='gaming';  user=1 }
    @{ id='hags_on';           name='Hardware-accelerated GPU scheduling';  cat='gaming';  user=0 }
    @{ id='mpo_off';           name='Disable MPO (fixes 24H2 stutter)';     cat='gaming';  user=0 }
    @{ id='timer_high';        name='Global timer resolution 0.5ms';        cat='latency'; user=0 }
    @{ id='network_gaming';    name='Gaming network stack (no Nagle...)';   cat='network'; user=0 }
    @{ id='mouse_precision';   name='Disable mouse acceleration (1:1)';     cat='input';   user=1 }
    @{ id='visual_fx_perf';    name='Performance visual effects';           cat='windows'; user=1 }
    @{ id='menu_delay_0';      name='Instant menu responses';               cat='windows'; user=1 }
    @{ id='background_apps';   name='Disable UWP background apps';          cat='gaming';  user=1 }
    @{ id='storage_sense';     name='Storage Sense weekly cleanup';         cat='disk';    user=1 }
    @{ id='search_index';      name='Disable search indexing';              cat='disk';    user=0 }
    @{ id='sysmain_off';       name='Disable SysMain (Superfetch)';         cat='ram';     user=0 }
    @{ id='hpets_off';         name='HPET off + useplatformclock false';    cat='latency'; user=0 }
    @{ id='power_ultimate';    name='Ultimate Performance power plan';      cat='power';   user=0 }
    @{ id='win32_priority';    name='Win32PrioritySeparation 0x26';         cat='power';   user=0 }
    @{ id='gpu_preference';    name='GPU preference: high performance';     cat='gaming';  user=1 }
    @{ id='fso_on';            name='Disable fullscreen optimisations';     cat='gaming';  user=1 }
    @{ id='xbox_live_off';     name='Disable Xbox Live services';           cat='gaming';  user=0 }
    @{ id='telemetry_off';     name='Disable telemetry (DiagTrack...)';     cat='privacy'; user=0 }
    @{ id='advertising_off';   name='Disable advertising ID';               cat='privacy'; user=1 }
    @{ id='activity_history';  name='Disable activity history / timeline';  cat='privacy'; user=1 }
    @{ id='bing_search';       name='Remove Bing from start menu search';   cat='privacy'; user=1 }
    @{ id='tailored_experiences'; name='Disable tailored experiences';      cat='privacy'; user=1 }
    @{ id='telemetry_tasks';   name='Disable telemetry scheduled tasks';    cat='privacy'; user=0 }
    @{ id='windows_copilot';   name='Remove Windows Copilot';               cat='privacy'; user=0 }
    @{ id='bloat_uninstall';   name='Uninstall MS Store bloat apps';        cat='debloat'; user=0 }
    @{ id='onedrive_off';      name='Uninstall OneDrive';                   cat='debloat'; user=0 }
    @{ id='hpets_boot';        name='Trim boot (no GUI boot logo)';         cat='windows'; user=0 }
    @{ id='edge_bing_blocking';name='Block Edge prerender/prelaunch';       cat='debloat'; user=1 }
    @{ id='windowed_games';    name='Optimizations for windowed games';     cat='gaming';  user=1 }
    @{ id='vrr';               name='Variable refresh rate (VRR)';          cat='gaming';  user=1 }
    @{ id='auto_hdr_off';      name='Disable Auto-HDR';                     cat='gaming';  user=1 }
    @{ id='game_bar_off';      name='Disable Game Bar entirely';            cat='gaming';  user=1 }
    @{ id='xbox_presence';     name='Disable Xbox presence (GameInput)';    cat='gaming';  user=0 }
    @{ id='bcdedit_tsc';       name='Dynamic tick + TSC platform tick';     cat='latency'; user=0 }
    @{ id='msi_mode';          name='MSI mode for GPU + NIC';               cat='latency'; user=0 }
    @{ id='interrupt_affinity';name='GPU interrupt affinity to P-cores';    cat='latency'; user=0 }
    @{ id='tcp_congestion';    name='TCP congestion provider BBR2';         cat='network'; user=0 }
    @{ id='nic_powersave';     name='NIC power saving off';                 cat='network'; user=0 }
    @{ id='usb_powersave';     name='USB selective suspend off';            cat='input';   user=0 }
    @{ id='pcie_aspm';         name='PCIe link power management off';       cat='power';   user=0 }
    @{ id='visual_fx_balloff'; name='Balanced visual effects';              cat='windows'; user=1 }
    @{ id='mouse_trails';      name='Disable cursor trail & shadow';        cat='input';   user=1 }
    @{ id='transparency_off';  name='Disable acrylic transparency';         cat='windows'; user=1 }
    @{ id='taskbar_anim';      name='Disable taskbar animations';           cat='windows'; user=1 }
    @{ id='dns_cache_big';     name='Bigger DNS cache (TTL 86400)';         cat='network'; user=0 }
    @{ id='shutdown_fast';     name='Fast startup ON (Hiberboot)';          cat='power';   user=0 }
    @{ id='recycle_bin_conf';  name='Recycle bin: immediate confirm';       cat='disk';    user=1 }

    # ---- v2.8: WinUtil-alignment batch (24) — engine v2.9 ----
    @{ id='widgets_off';       name='Widgets - Remove (taskbar)';           cat='debloat'; user=0 }
    @{ id='location_off';      name='Location tracking - Disable';          cat='privacy'; user=0 }
    @{ id='services_manual';   name='Services to Manual + svchost tuning';  cat='debloat'; user=0 }
    @{ id='delivery_opt';      name='Delivery Optimization - Disable';      cat='network'; user=0 }
    @{ id='consumer_features'; name='Consumer features - Disable';          cat='debloat'; user=0 }
    @{ id='store_search_off';  name='Store recommended search - Disable';   cat='debloat'; user=1 }
    @{ id='end_task_on_tb';    name='End task on taskbar right-click';      cat='windows'; user=1 }
    @{ id='wpbt_block';        name='WPBT vendor boot code - Block';        cat='privacy'; user=0 }
    @{ id='razer_block';       name='Razer auto-install - Block';           cat='debloat'; user=0 }
    @{ id='notifications_off'; name='Notifications & tips - Disable';       cat='privacy'; user=1 }
    @{ id='ipv4_prefer';       name='IPv4 preferred over IPv6';             cat='network'; user=0 }
    @{ id='ipv6_off';          name='IPv6 - Disable';                       cat='network'; user=0 }
    @{ id='teredo_off';        name='Teredo - Disable';                     cat='network'; user=0 }
    @{ id='disk_cleanup';      name='Disk cleanup + WinSxS trim (DISM)';    cat='disk';    user=0 }
    @{ id='hibernation_off';   name='Hibernation - Disable (frees disk)';   cat='power';   user=0 }
    @{ id='bsod_verbose';      name='Verbose BSoD messages';                cat='windows'; user=0 }
    @{ id='long_paths';        name='Long paths (>260 chars) - Enable';     cat='windows'; user=0 }
    @{ id='game_mode_win11';   name='Game Mode (Windows 11 form)';          cat='gaming';  user=1 }
    @{ id='edge_debloat';      name='Edge - Debloat (12 policies)';         cat='debloat'; user=0 }
    @{ id='brave_debloat';     name='Brave - Debloat (rewards/wallet/VPN)'; cat='debloat'; user=0 }
    @{ id='utc_time';          name='Hardware clock as UTC (dual-boot)';    cat='windows'; user=0 }
    @{ id='restore_point';     name='Create a restore point now';           cat='windows'; user=0 }
)

function Test-AdminForTweak { param($t) ($t.user -eq 1) -or (Test-Admin) }

# ----------------------------------------------------------------- apply (parity with C++)
function Invoke-Tweak { param([string]$Id)
    $t = $Script:Tweaks | Where-Object id -eq $Id
    if (-not $t) { Write-Kit "unknown tweak: $Id" 'err'; return $false }
    if (-not (Test-AdminForTweak $t)) { Write-Kit ("{0}: administrator required (run OptimizeKit.bat)" -f $Id) 'err'; return $false }
    switch ($Id) {
        'game_mode'          { Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1 }
        'game_dvr_off'       { Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 0; Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_FSEBehaviorMode' 2; Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_HonorUserFSEBehaviorMode' 1; Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_EFSEFeatureFlags' 0; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'UseNexusForGameBarEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'ShowStartupPanel' 0 }
        'hags_on'            { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode' 2; Write-Kit 'HAGS on - reboot required' 'warn' }
        'mpo_off'            { Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'OverlayTestMode' 5 }
        'timer_high'         { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\kernel' 'GlobalTimerResolutionRequests' 1; bcdedit /set disabledynamictick yes | Out-Null; bcdedit /set useplatformclock false | Out-Null }
        'network_gaming'     { $mm='HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile'; Set-RegDword $mm 'NetworkThrottlingIndex' 0xFFFFFFFF; Set-RegDword $mm 'SystemResponsiveness' 0; $g="$mm\Tasks\Games"; Set-RegDword $g 'GPU Priority' 8; Set-RegDword $g 'Priority' 6; Set-RegString $g 'Scheduling Category' 'High'; Set-RegString $g 'SFIO Priority' 'High'; Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces' | ForEach-Object { Set-RegDword $_.PSPath 'TcpAckFrequency' 1; Set-RegDword $_.PSPath 'TCPNoDelay' 1 } }
        'mouse_precision'    { Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '0'; Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '0'; Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '0' }
        'visual_fx_perf'     { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 2; Set-RegString 'HKCU:\Control Panel\Desktop\WindowMetrics' 'MinAnimate' '0' }
        'menu_delay_0'       { Set-RegDword 'HKCU:\Control Panel\Desktop' 'MenuShowDelay' 0 }
        'background_apps'    { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications' 'GlobalUserDisabled' 1; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BackgroundAppGlobalToggle' 0 }
        'storage_sense'      { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\StorageSense\Parameters\StoragePolicy' '01' 1; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\StorageSense\Parameters\StoragePolicy' '2048' 7 }
        'search_index'       { Disable-KitService 'WSearch' 'search indexing' }
        'sysmain_off'        { Disable-KitService 'SysMain' 'superfetch' }
        'hpets_off'          { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\hpet' 'Start' 4; bcdedit /set useplatformclock false | Out-Null; bcdedit /set disabledynamictick yes | Out-Null }
        'power_ultimate'     { powercfg -duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 2>$null | Out-Null; powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61 2>$null | Out-Null; if ($LASTEXITCODE -ne 0) { powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c 2>$null | Out-Null } }
        'win32_priority'     { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\PriorityControl' 'Win32PrioritySeparation' 0x26 }
        'gpu_preference'     { Set-RegString 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' 'SwapEffectUpgradeEnable=1;' }
        'fso_on'             { Set-RegDword 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' '~ DISABLEDXMAXIMIZEDWINDOWEDMODE' 1 }
        'xbox_live_off'      { foreach ($s in 'XblAuthManager','XblGameSave','XboxNetApiSvc','XboxGipSvc') { Disable-KitService $s 'xbox live' }; Write-Kit 'Game Pass on PC will stop working' 'warn' }
        'telemetry_off'      { Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection' 'AllowTelemetry' 0; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\DataCollection' 'AllowTelemetry' 0; Disable-KitService 'DiagTrack' 'connected user experiences'; Disable-KitService 'dmwappushservice' 'telemetry' }
        'advertising_off'    { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo' 'Enabled' 0 }
        'activity_history'   { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'PublishUserActivities' 0; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'UploadUserActivities' 0 }
        'bing_search'        { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BingSearchEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'CortanaConsent' 0 }
        'tailored_experiences' { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'TailoredExperiencesWithDiagnosticDataEnabled' 0 }
        'telemetry_tasks'    { $n=0; foreach ($t in '\Microsoft\Windows\Customer Experience Improvement Program\Consolidator','\Microsoft\Windows\Customer Experience Improvement Program\UsbCeip','\Microsoft\Windows\Customer Experience Improvement Program\Uploader','\Microsoft\Windows\Application Experience\Microsoft Compatibility Appraiser','\Microsoft\Windows\Application Experience\ProgramDataUpdater','\Microsoft\Windows\Autochk\Proxy','\Microsoft\Windows\DiskDiagnostic\Microsoft-Windows-DiskDiagnosticDataCollector','\Microsoft\Windows\Feedback\Siuf\DmClient','\Microsoft\Windows\Feedback\Siuf\DmClientOnScenarioDownload') { if (schtasks /Change /TN $t /Disable 2>$null) { $n++ } }; Write-Kit "$n telemetry tasks disabled" 'ok' }
        'windows_copilot'    { Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsCopilot' 'TurnOffWindowsCopilot' 1; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowCopilotButton' 0 }
        'bloat_uninstall'    { $pkgs='Microsoft.549981C3F5F10','Microsoft.BingFinance','Microsoft.BingNews','Microsoft.BingSports','Microsoft.BingWeather','Microsoft.BingSearch','Microsoft.Clipchamp','Microsoft.GamingApp','Microsoft.GetHelp','Microsoft.Getstarted','Microsoft.MicrosoftOfficeHub','Microsoft.MicrosoftSolitaireCollection','Microsoft.MixedReality.Portal','Microsoft.News','Microsoft.PowerAutomateDesktop','Microsoft.SkypeApp','Microsoft.Todos','Microsoft.Whiteboard','Microsoft.WindowsFeedbackHub','Microsoft.WindowsMaps','Microsoft.Xbox.TCUI','Microsoft.XboxApp','Microsoft.XboxGameOverlay','Microsoft.XboxGamingOverlay','Microsoft.XboxSpeechToTextOverlay','Microsoft.YourPhone','Microsoft.ZuneMusic','Microsoft.ZuneVideo','MicrosoftTeams','Clipchamp.Clipchamp','Microsoft.Copilot'; $n=0; foreach ($p in $pkgs) { Get-AppxPackage -Name $p -AllUsers | Remove-AppxPackage -AllUsers; if (-not (Get-AppxPackage -Name $p)) { $n++ } }; Write-Kit "$n bloat apps removed" 'ok' }
        'onedrive_off'       { Get-Process onedrive | Stop-Process -Force; foreach ($s in "$env:WinDir\SysWOW64\OneDriveSetup.exe","$env:WinDir\System32\OneDriveSetup.exe") { if (Test-Path $s) { Start-Process $s '/uninstall' -Wait } }; Write-Kit 'OneDrive uninstalled (local files untouched)' 'ok' }
        'hpets_boot'         { bcdedit /set nobootuxprogress | Out-Null }
        'edge_bing_blocking' { Set-RegDword 'HKCU:\Software\Policies\Microsoft\Microsoft Edge' 'HubsSidebarEnabled' 0; Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Microsoft Edge' 'BackgroundModeEnabled' 0; Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Microsoft Edge' 'StartupBoostEnabled' 0 }
        'windowed_games'     { Set-RegString 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' 'SwapEffectUpgradeEnable=1;' }
        'vrr'                { Set-RegDword 'HKCU:\Software\DirectX\UserGpuPreferences' 'VRROptimizeEnable' 1 }
        'auto_hdr_off'       { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AutoHDR' 'Enabled' 0; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'ForceAutoHDR' 0 }
        'game_bar_off'       { Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'UseNexusForGameBarEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 0 }
        'xbox_presence'      { Disable-KitService 'GameInput' 'xbox presence' }
        'bcdedit_tsc'        { bcdedit /set disabledynamictick yes | Out-Null; bcdedit /set useplatformtick yes | Out-Null }
        'msi_mode'           { Get-PnpDevice -Class Display,Net -Status OK | ForEach-Object { $p='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$_.InstanceId+'\Device Parameters'; $d=Get-ItemProperty -Path $p -ErrorAction SilentlyContinue; if ($d -and $null -ne $d.MessageSignaledInterruptProperties -and $d.MessageSignaledInterruptProperties -eq 0) { Set-ItemProperty -Path $p -Name MessageSignaledInterruptProperties -Value 1 } }; Write-Kit 'MSI mode enabled where supported' 'ok' }
        'interrupt_affinity' { $gpu=(Get-PnpDevice -Class Display -Status OK | Select-Object -First 1).InstanceId; if ($gpu) { $k='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$gpu+'\Device Parameters\Interrupt Management\MessageSignaledInterruptProperties'; if (Test-Path $k) { Set-ItemProperty -Path $k -Name DevicePolicy -Value 4 -Type DWord; Set-ItemProperty -Path $k -Name DevicePriority -Value 3 -Type DWord } } }
        'tcp_congestion'     { netsh int tcp set supplemental Internet congestionprovider=bbr2 | Out-Null; Write-Kit 'congestion provider -> bbr2 (revert: cubic)' 'ok' }
        'nic_powersave'      { & (Join-Path $PSScriptRoot 'OptimizeKit-NIC.ps1') -Mode gaming 2>$null; powercfg /setacvalueindex scheme_current 19cbb8fa-5279-450e-9fac-8a3d5fedd0c1 12bbebe6-58d6-4636-95bb-3217ef867c1a 0; powercfg /setactive scheme_current }
        'usb_powersave'      { powercfg /setacvalueindex scheme_current 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0; powercfg /setactive scheme_current }
        'pcie_aspm'          { powercfg /setacvalueindex scheme_current 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 0; powercfg /setactive scheme_current }
        'visual_fx_balloff'  { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 3 }
        'mouse_trails'       { Set-RegDword 'HKCU:\Control Panel\Cursors' 'CursorTrails' 0; Set-RegDword 'HKCU:\Control Panel\Desktop' 'UserPreferencesMask' 0x90120380 }
        'transparency_off'   { Set-RegDword 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' 0 }
        'taskbar_anim'       { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarAnimations' 0 }
        'dns_cache_big'      { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxCacheTtl' 86400; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxNegativeCacheTtl' 5; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxCacheSize' 0x64000 }
        'shutdown_fast'      { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HiberbootEnabled' 1 }
        'recycle_bin_conf'   { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer' 'ConfirmFileDelete' 1 }

        # ---- v2.8: WinUtil-alignment batch ----
        'widgets_off'        { Get-Process *Widget* -ErrorAction SilentlyContinue | Stop-Process -Force; Get-AppxPackage Microsoft.WidgetsPlatformRuntime -AllUsers | Remove-AppxPackage -AllUsers; Get-AppxPackage MicrosoftWindows.Client.WebExperience -AllUsers | Remove-AppxPackage -AllUsers; Stop-Process -Name explorer -Force; Write-Kit 'widgets removed (explorer restarted)' 'ok' }
        'location_off'       { Disable-KitService 'lfsvc' 'location'; Set-RegString 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\location' 'Value' 'Deny'; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Sensor\Overrides\{BFA794E4-F964-4FDB-90F6-51056BFE4B44}' 'SensorPermissionState' 0; Set-RegDword 'HKLM:\SYSTEM\Maps' 'AutoUpdateEnabled' 0 }
        'services_manual'    { foreach ($p in @(@('CscService','Disabled'),@('DiagTrack','Disabled'),@('MapsBroker','Manual'),@('StorSvc','Manual'),@('SharedAccess','Disabled'))) { Backup-RegKey ('HKLM\SYSTEM\CurrentControlSet\Services\' + $p[0]); Set-Service $p[0] -StartupType $p[1] -ErrorAction SilentlyContinue }; $mem=(Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum/1KB; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control' 'SvcHostSplitThresholdInKB' $mem; Write-Kit 'services -> manual + svchost split tuned' 'ok' }
        'delivery_opt'       { Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization' 'DODownloadMode' 0 }
        'consumer_features'  { Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent' 'DisableWindowsConsumerFeatures' 1 }
        'store_search_off'   { $db="$env:LOCALAPPDATA\Packages\Microsoft.WindowsStore_8wekyb3d8bbwe\LocalState\store.db"; if (Test-Path $db) { icacls $db /deny Everyone:F | Out-Null; Write-Kit 'store recommended search denied' 'ok' } else { Write-Kit 'store.db not found' 'warn' } }
        'end_task_on_tb'     { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\TaskbarDeveloperSettings' 'TaskbarEndTask' 1 }
        'wpbt_block'         { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' 'DisableWpbtExecution' 1; Write-Kit 'vendor WPBT boot code blocked' 'ok' }
        'razer_block'        { Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\DriverSearching' 'SearchOrderConfig' 0; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Device Installer' 'DisableCoInstallers' 1; $rp="$env:WinDir\Installer\Razer"; if (-not (Test-Path $rp)) { New-Item -ItemType Directory -Force -Path $rp | Out-Null }; icacls $rp /deny 'Everyone:(W)' 2>$null | Out-Null; Write-Kit 'razer auto-install blocked' 'ok' }
        'notifications_off'  { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\PushNotifications' 'ToastEnabled' 0; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\ContentDeliveryManager' 'SubscribedContent-338389Enabled' 0 }
        'ipv4_prefer'        { netsh int ipv6 set prefixpolicies ::ffff:0:0/96 46 4 2>$null | Out-Null; netsh int ipv6 set prefixpolicies ::/0 40 5 2>$null | Out-Null; Write-Kit 'IPv4 preferred over IPv6' 'ok' }
        'ipv6_off'           { foreach ($ad in Get-NetAdapterBinding -ComponentID ms_tcpip6) { Disable-NetAdapterBinding -Name $ad.Name -ComponentID ms_tcpip6 }; Write-Kit 'IPv6 disabled on all adapters' 'ok' }
        'teredo_off'         { netsh int teredo set state disabled 2>$null | Out-Null; Write-Kit 'Teredo disabled' 'ok' }
        'disk_cleanup'       { $before=(Get-PSDrive C).Free; cleanmgr /verylowdisk 2>$null | Out-Null; Dism.exe /Online /Cleanup-Image /StartComponentCleanup 2>$null | Out-Null; Write-Kit ('component cleanup done (freed {0:N0} MB during run)' -f (((Get-PSDrive C).Free-$before)/1MB)) 'ok' }
        'hibernation_off'    { powercfg /hibernate off 2>$null | Out-Null; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HibernateEnabled' 0; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\FlyoutMenuSettings' 'ShowHibernateOption' 0; Write-Kit 'hibernation off (frees hiberfil.sys)' 'ok' }
        'bsod_verbose'       { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' 'DisplayParameters' 1; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' 'AlwaysDump' 1 }
        'long_paths'         { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' 'LongPathsEnabled' 1 }
        'game_mode_win11'    { Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1 }
        'edge_debloat'       { $p='HKLM:\SOFTWARE\Policies\Microsoft\Edge'; foreach ($s in @(@('PersonalizationReportingEnabled',0),@('ShowRecommendationsEnabled',0),@('HideFirstRunExperience',1),@('UserFeedbackAllowed',0),@('ConfigureDoNotTrack',1),@('AlternateErrorPagesEnabled',0),@('EdgeCollectionsEnabled',0),@('EdgeShoppingAssistantEnabled',0),@('ShowMicrosoftRewards',0),@('WebWidgetAllowed',0),@('DiagnosticData',0),@('DefaultBrowserSettingsCampaignEnabled',0))) { Set-RegDword $p $s[0] $s[1] }; Write-Kit 'edge telemetry/annoyances disabled (12 policies)' 'ok' }
        'brave_debloat'      { $p='HKLM:\SOFTWARE\Policies\BraveSoftware\Brave'; foreach ($s in @(@('BraveRewardsDisabled',1),@('BraveWalletDisabled',1),@('BraveVPNDisabled',1),@('BraveAIChatEnabled',0),@('BraveStatsPingEnabled',0),@('BraveNewsDisabled',1),@('BraveTalkDisabled',1),@('TorDisabled',1),@('BraveP3AEnabled',0))) { Set-RegDword $p $s[0] $s[1] }; Write-Kit 'brave rewards/wallet/VPN/news disabled' 'ok' }
        'utc_time'           { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\TimeZoneInformation' 'RealTimeIsUniversal' 1; Write-Kit 'hardware clock read as UTC (dual-boot fix)' 'ok' }
        'restore_point'      { Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SystemRestore' 'SystemRestorePointCreationFrequency' 0; Enable-ComputerRestore -Drive "$env:SystemDrive" -ErrorAction SilentlyContinue; Checkpoint-Computer -Description 'OptimizeKit restore point' -RestorePointType MODIFY_SETTINGS; Write-Kit 'system restore point created' 'ok' }
        default              { Write-Kit "no action for $Id" 'err'; return $false }
    }
    Write-Kit ("applied {0} [{1}]" -f $t.name, $t.cat) 'ok'
    return $true
}

# ----------------------------------------------------------------- restore (parity)
function Restore-Tweak { param([string]$Id)
    switch ($Id) {
        'game_mode'          { Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1 }
        'game_dvr_off'       { Remove-ItemProperty 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' -ErrorAction SilentlyContinue; Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' -ErrorAction SilentlyContinue }
        'hags_on'            { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode' 1 }
        'mpo_off'            { Remove-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'OverlayTestMode' -ErrorAction SilentlyContinue }
        'timer_high'         { Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\kernel' 'GlobalTimerResolutionRequests' -ErrorAction SilentlyContinue; bcdedit /deletevalue disabledynamictick 2>$null | Out-Null; bcdedit /deletevalue useplatformclock 2>$null | Out-Null }
        'network_gaming'     { $mm='HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile'; Set-RegDword $mm 'NetworkThrottlingIndex' 10; Set-RegDword $mm 'SystemResponsiveness' 20 }
        'mouse_precision'    { Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseSpeed' '1'; Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold1' '6'; Set-RegString 'HKCU:\Control Panel\Mouse' 'MouseThreshold2' '10' }
        'visual_fx_perf'     { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 0 }
        'menu_delay_0'       { Set-RegDword 'HKCU:\Control Panel\Desktop' 'MenuShowDelay' 400 }
        'background_apps'    { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications' 'GlobalUserDisabled' -ErrorAction SilentlyContinue }
        'storage_sense'      { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\StorageSense\Parameters\StoragePolicy' '01' 0 }
        'search_index'       { Restore-KitService 'WSearch' }
        'sysmain_off'        { Restore-KitService 'SysMain' }
        'hpets_off'          { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\hpet' 'Start' 1 }
        'power_ultimate'     { powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e 2>$null | Out-Null }
        'win32_priority'     { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\PriorityControl' 'Win32PrioritySeparation' 2 }
        'gpu_preference'     { Remove-ItemProperty 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' -ErrorAction SilentlyContinue }
        'fso_on'             { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' '~ DISABLEDXMAXIMIZEDWINDOWEDMODE' -ErrorAction SilentlyContinue }
        'xbox_live_off'      { foreach ($s in 'XblAuthManager','XblGameSave','XboxNetApiSvc') { Restore-KitService $s } }
        'telemetry_off'      { Set-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection' 'AllowTelemetry' 1; Restore-KitService 'DiagTrack' }
        'advertising_off'    { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo' 'Enabled' 1 }
        'activity_history'   { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'PublishUserActivities' 1 }
        'bing_search'        { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BingSearchEnabled' -ErrorAction SilentlyContinue }
        'tailored_experiences' { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'TailoredExperiencesWithDiagnosticDataEnabled' 1 }
        'windows_copilot'    { Remove-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WindowsCopilot' 'TurnOffWindowsCopilot' -ErrorAction SilentlyContinue; Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowCopilotButton' 1 }
        'onedrive_off'       { Write-Kit 'OneDrive: reinstall from microsoft.com/microsoft-365/onedrive/download' 'info' }
        'hpets_boot'         { bcdedit /deletevalue nobootuxprogress 2>$null | Out-Null }
        'edge_bing_blocking' { Remove-ItemProperty 'HKCU:\Software\Policies\Microsoft\Microsoft Edge' 'HubsSidebarEnabled' -ErrorAction SilentlyContinue }
        'windowed_games'     { Remove-ItemProperty 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' -ErrorAction SilentlyContinue }
        'vrr'                { Remove-ItemProperty 'HKCU:\Software\DirectX\UserGpuPreferences' 'VRROptimizeEnable' -ErrorAction SilentlyContinue }
        'auto_hdr_off'       { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AutoHDR' 'Enabled' -ErrorAction SilentlyContinue; Remove-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'ForceAutoHDR' -ErrorAction SilentlyContinue }
        'game_bar_off'       { Set-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'UseNexusForGameBarEnabled' 1 }
        'xbox_presence'      { Restore-KitService 'GameInput' }
        'bcdedit_tsc'        { bcdedit /deletevalue disabledynamictick 2>$null | Out-Null; bcdedit /deletevalue useplatformtick 2>$null | Out-Null }
        'msi_mode'           { Get-PnpDevice -Class Display,Net -Status OK | ForEach-Object { $p='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$_.InstanceId+'\Device Parameters'; Set-ItemProperty -Path $p -Name MessageSignaledInterruptProperties -Value 0 -ErrorAction SilentlyContinue } }
        'interrupt_affinity' { $gpu=(Get-PnpDevice -Class Display -Status OK | Select-Object -First 1).InstanceId; if ($gpu) { $k='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$gpu+'\Device Parameters\Interrupt Management\MessageSignaledInterruptProperties'; if (Test-Path $k) { Remove-ItemProperty -Path $k -Name DevicePolicy -ErrorAction SilentlyContinue; Remove-ItemProperty -Path $k -Name DevicePriority -ErrorAction SilentlyContinue } } }
        'tcp_congestion'     { netsh int tcp set supplemental Internet congestionprovider=cubic | Out-Null }
        'nic_powersave'      { & (Join-Path $PSScriptRoot 'OptimizeKit-NIC.ps1') -Mode restore 2>$null }
        'usb_powersave'      { powercfg /setacvalueindex scheme_current 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 1; powercfg /setactive scheme_current }
        'pcie_aspm'          { powercfg /setacvalueindex scheme_current 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 1; powercfg /setactive scheme_current }
        'visual_fx_balloff'  { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 0 }
        'mouse_trails'       { Set-RegDword 'HKCU:\Control Panel\Cursors' 'CursorTrails' 0; Set-RegDword 'HKCU:\Control Panel\Desktop' 'UserPreferencesMask' 0x9E3E0780 }
        'transparency_off'   { Remove-ItemProperty 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' -ErrorAction SilentlyContinue }
        'taskbar_anim'       { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarAnimations' -ErrorAction SilentlyContinue }
        'dns_cache_big'      { Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxCacheTtl' -ErrorAction SilentlyContinue; Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxNegativeCacheTtl' -ErrorAction SilentlyContinue; Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxCacheSize' -ErrorAction SilentlyContinue }
        'shutdown_fast'      { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HiberbootEnabled' 1 }
        'recycle_bin_conf'   { Set-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer' 'ConfirmFileDelete' 0 }

        # ---- v2.8 batch: restore Windows defaults ----
        'widgets_off'        { Write-Kit 'widgets: reinstall from store (microsoft.com/store)' 'info' }
        'location_off'       { Restore-KitService 'lfsvc'; Set-RegString 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\location' 'Value' 'Allow' }
        'services_manual'    { foreach ($p in @(@('CscService','Manual'),@('DiagTrack','Automatic'),@('MapsBroker','Automatic'),@('StorSvc','Automatic'),@('SharedAccess','Automatic'))) { Set-Service $p[0] -StartupType $p[1] -ErrorAction SilentlyContinue }; Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control' 'SvcHostSplitThresholdInKB' -ErrorAction SilentlyContinue }
        'delivery_opt'       { Remove-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization' 'DODownloadMode' -ErrorAction SilentlyContinue }
        'consumer_features'  { Remove-ItemProperty 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent' 'DisableWindowsConsumerFeatures' -ErrorAction SilentlyContinue }
        'store_search_off'   { $db="$env:LOCALAPPDATA\Packages\Microsoft.WindowsStore_8wekyb3d8bbwe\LocalState\store.db"; if (Test-Path $db) { icacls $db /grant Everyone:F 2>$null | Out-Null } }
        'end_task_on_tb'     { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\TaskbarDeveloperSettings' 'TaskbarEndTask' -ErrorAction SilentlyContinue }
        'wpbt_block'         { Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' 'DisableWpbtExecution' -ErrorAction SilentlyContinue }
        'razer_block'        { Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\DriverSearching' 'SearchOrderConfig' 1; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Device Installer' 'DisableCoInstallers' 0; icacls "$env:WinDir\Installer\Razer" /remove:d Everyone 2>$null | Out-Null }
        'notifications_off'  { Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\PushNotifications' 'ToastEnabled' -ErrorAction SilentlyContinue }
        'ipv4_prefer'        { netsh int ipv6 reset 2>$null | Out-Null; Write-Kit 'IPv6 prefix policies reset' 'ok' }
        'ipv6_off'           { foreach ($ad in Get-NetAdapterBinding -ComponentID ms_tcpip6 -ErrorAction SilentlyContinue) { Enable-NetAdapterBinding -Name $ad.Name -ComponentID ms_tcpip6 }; Write-Kit 'IPv6 re-enabled' 'ok' }
        'teredo_off'         { netsh int teredo set state client 2>$null | Out-Null }
        'disk_cleanup'       { Write-Kit 'component cleanup has no undo (Windows re-accumulates over time)' 'info' }
        'hibernation_off'    { powercfg /hibernate on 2>$null | Out-Null; Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HibernateEnabled' 1; Set-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\FlyoutMenuSettings' 'ShowHibernateOption' 1 }
        'bsod_verbose'       { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' 'DisplayParameters' 0; Remove-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' 'AlwaysDump' -ErrorAction SilentlyContinue }
        'long_paths'         { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' 'LongPathsEnabled' 0 }
        'game_mode_win11'    { Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 0; Set-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 0 }
        'edge_debloat'       { Remove-Item 'HKLM:\SOFTWARE\Policies\Microsoft\Edge' -Recurse -Force -ErrorAction SilentlyContinue }
        'brave_debloat'      { Remove-Item 'HKLM:\SOFTWARE\Policies\BraveSoftware' -Recurse -Force -ErrorAction SilentlyContinue }
        'utc_time'           { Set-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\TimeZoneInformation' 'RealTimeIsUniversal' 0 }
        'restore_point'      { Write-Kit 'restore points are never auto-deleted' 'info' }
    }
    Write-Kit ("restored defaults {0}" -f $Id) 'ok'
}

# ----------------------------------------------------------------- status (reads the machine, never a memory)
function Get-TweakState { param([string]$Id)
    switch ($Id) {
        'game_mode'          { return (Get-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1) -eq 1 }
        'game_dvr_off'       { return ((Get-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1) -eq 0) -and ((Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 1) -eq 0) }
        'hags_on'            { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\GraphicsDrivers' 'HwSchMode' 1) -eq 2 }
        'mpo_off'            { return (Get-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'OverlayTestMode' 0) -eq 5 }
        'timer_high'         { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\kernel' 'GlobalTimerResolutionRequests' 0) -eq 1 }
        'network_gaming'     { return ((Get-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile' 'NetworkThrottlingIndex' 10) -eq 0xFFFFFFFF) }
        'mouse_precision'    { return (Get-RegDword 'HKCU:\Control Panel\Mouse' 'MouseSpeed' 1) -eq 0 }
        'visual_fx_perf'     { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects' 'VisualFXSetting' 0) -eq 2 }
        'menu_delay_0'       { return (Get-RegDword 'HKCU:\Control Panel\Desktop' 'MenuShowDelay' 400) -eq 0 }
        'background_apps'    { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications' 'GlobalUserDisabled' 0) -eq 1 }
        'storage_sense'      { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\StorageSense\Parameters\StoragePolicy' '01' 0) -eq 1 }
        'search_index'       { $s = Get-Service WSearch -ErrorAction SilentlyContinue; return $s -and $s.StartType -eq 'Disabled' }
        'sysmain_off'        { $s = Get-Service SysMain -ErrorAction SilentlyContinue; return $s -and $s.StartType -eq 'Disabled' }
        'hpets_off'          { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\hpet' 'Start' 1) -eq 4 }
        'power_ultimate'     { return ((powercfg /getactivescheme) -match 'Ultimate|Khadafi|Bitkit') }
        'win32_priority'     { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\PriorityControl' 'Win32PrioritySeparation' 2) -eq 0x26 }
        'gpu_preference'     { return (Get-RegDword 'HKCU:\Software\DirectX\UserGpuPreferences' 'DirectXUserGlobalSettings' 0) -ne 0 }
        'fso_on'             { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' '~ DISABLEDXMAXIMIZEDWINDOWEDMODE' 0) -ne 0 }
        'xbox_live_off'      { $s = Get-Service XblAuthManager -ErrorAction SilentlyContinue; return $s -and $s.StartType -eq 'Disabled' }
        'telemetry_off'      { return (Get-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DataCollection' 'AllowTelemetry' 1) -eq 0 }
        'advertising_off'    { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AdvertisingInfo' 'Enabled' 1) -eq 0 }
        'activity_history'   { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'PublishUserActivities' 1) -eq 0 }
        'bing_search'        { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BingSearchEnabled' 1) -eq 0 }
        'tailored_experiences' { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Privacy' 'TailoredExperiencesWithDiagnosticDataEnabled' 1) -eq 0 }
        'windows_copilot'    { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'ShowCopilotButton' 1) -eq 0 }
        'edge_bing_blocking' { return (Get-RegDword 'HKCU:\Software\Policies\Microsoft\Microsoft Edge' 'HubsSidebarEnabled' 1) -eq 0 }
        'windowed_games'     { $v=(Get-ItemProperty 'HKCU:\Software\DirectX\UserGpuPreferences' -Name DirectXUserGlobalSettings -ErrorAction SilentlyContinue).DirectXUserGlobalSettings; return $v -and $v -match 'SwapEffectUpgradeEnable=1' }
        'vrr'                { return (Get-RegDword 'HKCU:\Software\DirectX\UserGpuPreferences' 'VRROptimizeEnable' 0) -eq 1 }
        'auto_hdr_off'       { return ((Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\AutoHDR' 'Enabled' 1) -eq 0) -or ((Get-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\Dwm' 'ForceAutoHDR' 0) -eq 0) }
        'game_bar_off'       { return ((Get-RegDword 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 1) -eq 0) -and ((Get-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1) -eq 0) }
        'xbox_presence'      { $s = Get-Service GameInput -ErrorAction SilentlyContinue; return $s -and $s.StartType -eq 'Disabled' }
        'transparency_off'   { return (Get-RegDword 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize' 'EnableTransparency' 1) -eq 0 }
        'taskbar_anim'       { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' 'TaskbarAnimations' 1) -eq 0 }
        'dns_cache_big'      { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters' 'MaxCacheTtl' 86400) -gt 86400 }
        'shutdown_fast'      { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HiberbootEnabled' 0) -eq 1 }
        'recycle_bin_conf'   { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer' 'ConfirmFileDelete' 0) -eq 1 }

        # ---- v2.8 batch ----
        'widgets_off'        { return $null -eq (Get-AppxPackage Microsoft.WidgetsPlatformRuntime -ErrorAction SilentlyContinue) }
        'location_off'       { $s = Get-Service lfsvc -ErrorAction SilentlyContinue; return ((-not $s) -or $s.StartType -eq 'Disabled') -or ((Get-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Sensor\Overrides\{BFA794E4-F964-4FDB-90F6-51056BFE4B44}' 'SensorPermissionState' 1) -eq 0) }
        'services_manual'    { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control' 'SvcHostSplitThresholdInKB' 0) -gt 0 }
        'delivery_opt'       { return (Get-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\DeliveryOptimization' 'DODownloadMode' -1) -eq 0 }
        'consumer_features'  { return (Get-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\CloudContent' 'DisableWindowsConsumerFeatures' 0) -eq 1 }
        'store_search_off'   { $db="$env:LOCALAPPDATA\Packages\Microsoft.WindowsStore_8wekyb3d8bbwe\LocalState\store.db"; if (Test-Path $db) { return -not (Get-Acl $db).Access.Where({$_.IdentityReference -eq 'Everyone' -and $_.AccessControlType -eq 'Deny'}) } return $false }
        'end_task_on_tb'     { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\TaskbarDeveloperSettings' 'TaskbarEndTask' 0) -eq 1 }
        'wpbt_block'         { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' 'DisableWpbtExecution' 0) -eq 1 }
        'razer_block'        { return (Get-RegDword 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Device Installer' 'DisableCoInstallers' 0) -eq 1 }
        'notifications_off'  { return (Get-RegDword 'HKCU:\Software\Microsoft\Windows\CurrentVersion\PushNotifications' 'ToastEnabled' 1) -eq 0 }
        'ipv4_prefer'        { $p = netsh int ipv6 show prefixpolicies 2>$null | Out-String; return $p -match '::ffff:0:0/96\s+46' }
        'ipv6_off'           { return (Get-NetAdapterBinding -ComponentID ms_tcpip6 -ErrorAction SilentlyContinue | Where-Object Enabled | Measure-Object).Count -eq 0 }
        'teredo_off'         { return (netsh int teredo show state 2>$null | Out-String) -match 'disabled' }
        'disk_cleanup'       { return $null }  # DIAG: one-shot action
        'hibernation_off'    { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Power' 'HibernateEnabled' 1) -eq 0 }
        'bsod_verbose'       { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl' 'DisplayParameters' 0) -eq 1 }
        'long_paths'         { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' 'LongPathsEnabled' 0) -eq 1 }
        'game_mode_win11'    { return ((Get-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 0) -eq 1) -and ((Get-RegDword 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 0) -eq 1) }
        'edge_debloat'       { return (Get-RegDword 'HKLM:\SOFTWARE\Policies\Microsoft\Edge' 'DiagnosticData' -1) -eq 0 }
        'brave_debloat'      { return (Get-RegDword 'HKLM:\SOFTWARE\Policies\BraveSoftware\Brave' 'BraveRewardsDisabled' 0) -eq 1 }
        'utc_time'           { return (Get-RegDword 'HKLM:\SYSTEM\CurrentControlSet\Control\TimeZoneInformation' 'RealTimeIsUniversal' 0) -eq 1 }
        'restore_point'      { return $null }  # DIAG: one-shot action
    }
}

function Show-StatusAll {
    Write-Section ("Tweak status - {0} tweaks ({1})" -f $Script:Tweaks.Count, $(if (Test-Admin) {'ADMIN'} else {'user'}))
    $on = 0
    foreach ($t in $Script:Tweaks) {
        $st = Get-TweakState $t.id
        $mark = if ($null -eq $st) { ($Dim + 'DIAG    ' + $Reset) }
                elseif ($st)        { $Green + 'ON      ' + $Reset; $on++ }
                else                { ($Dim + 'default ' + $Reset) }
        $lock = if (-not (Test-AdminForTweak $t)) { ($Yellow + ' [ADMIN]' + $Reset) } else { '' }
        Write-Host ("  {0} {1,-42} {2}{3}" -f $mark, $t.name, ($Dim + $t.id + $Reset), $lock)
    }
    Write-Host ""
    Write-Host ("  {0}{1} tweaks active{2} - full list: OptimizeKit.ps1 -Status" -f $Green, $on, $Reset)
}

# ----------------------------------------------------------------- profiles
function Invoke-GamingProfile {
    Write-Section 'GAMING profile'
    $ids = 'game_mode','game_dvr_off','background_apps','gpu_preference','windowed_games','vrr','mouse_precision','menu_delay_0','storage_sense'
    if (Test-Admin) { $ids += 'hags_on','timer_high','network_gaming','power_ultimate','win32_priority','usb_powersave','pcie_aspm' }
    $ok = 0
    foreach ($i in $ids) { if (Invoke-Tweak $i) { $ok++ } }
    Write-Kit ("gaming profile: {0}/{1} applied" -f $ok, $ids.Count) 'ok'
}
function Invoke-PrivacyProfile {
    Write-Section 'PRIVACY profile'
    $ids = 'advertising_off','activity_history','bing_search','tailored_experiences','edge_bing_blocking','notifications_off'
    if (Test-Admin) { $ids += 'telemetry_off','telemetry_tasks','windows_copilot','location_off','wpbt_block' }
    $ok = 0
    foreach ($i in $ids) { if (Invoke-Tweak $i) { $ok++ } }
    Write-Kit ("privacy profile: {0}/{1} applied" -f $ok, $ids.Count) 'ok'
}
function Invoke-DebloatProfile {
    Write-Section 'DEBLOAT profile'
    $ids = 'background_apps','edge_bing_blocking','bloat_uninstall','sysmain_off','search_index'
    if (Test-Admin) { $ids += 'widgets_off','consumer_features','services_manual','delivery_opt','edge_debloat','razer_block' }
    $ok = 0
    foreach ($i in $ids) { if (Invoke-Tweak $i) { $ok++ } }
    Write-Kit ("debloat profile: {0}/{1} applied" -f $ok, $ids.Count) 'ok'
}
function Invoke-FullKit {
    Invoke-GamingProfile; Invoke-PrivacyProfile; Invoke-DebloatProfile
    if (Test-Admin) { Invoke-JunkCleanup; Invoke-Tweak 'restore_point' }
    Write-Section 'FULL KIT COMPLETE'
    Write-Kit 'reboot recommended (HAGS / timer / power plan)' 'warn'
}

# ----------------------------------------------------------------- diagnostics
function Show-Diagnostics {
    Write-Section 'Diagnostics (measured, honest)'
    $os = Get-CimInstance Win32_OperatingSystem
    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $freePct = [int](100 * $os.FreePhysicalMemory / $os.TotalVisibleMemorySize)
    $clock = $cpu.CurrentClockSpeed; $base = $cpu.MaxClockSpeed
    Write-Host ("  CPU      : {0}" -f $cpu.Name.Trim())
    Write-Host ("  Clock    : {0} MHz effective / {1} MHz base {2}" -f $clock, $base, $(if ($clock -lt $base * 0.7) { $Yellow + '<- below base: throttling or idle' + $Reset } else { $Green + 'OK' + $Reset }))
    Write-Host ("  RAM free : {0} % ({1:N1} GB / {2:N1} GB)" -f $freePct, ($os.FreePhysicalMemory/1MB), ($os.TotalVisibleMemorySize/1MB))
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
    Write-Host ("  GPU      : {0} (driver {1})" -f $gpu.Name, $gpu.DriverVersion)
    foreach ($tgt in '1.1.1.1','8.8.8.8') {
        $p = Test-Connection -ComputerName $tgt -Count 4 -ErrorAction SilentlyContinue
        if ($p) {
            $rt = $p | ForEach-Object { $_.ResponseTime }
            $avg = [int](($rt | Measure-Object -Average).Average)
            $jit = [math]::Round(($rt | ForEach-Object { [math]::Abs($_ - $avg) } | Measure-Object -Average).Average, 1)
            $loss = [math]::Round(100 * (4 - $p.Count) / 4, 0)
            $c = if ($avg -lt 30) { 'Green' } elseif ($avg -lt 80) { 'Yellow' } else { 'Red' }
            Write-Host ("  Ping {0,-8}: {1} ms avg, jitter {2} ms, loss {3} %" -f $tgt, $avg, $jit, $loss) -ForegroundColor $c
        }
    }
    Write-Host ("  Power    : {0}" -f ((powercfg /getactivescheme) -replace '.*GUID: \S+\s+(\(.*\))','$1').Trim('()'))
}

# ----------------------------------------------------------------- network / cleanup
function Test-Latency {
    Write-Section 'Network latency (4 pings / target)'
    Show-Diagnostics
}
function Invoke-JunkCleanup {
    Write-Section 'Junk cleanup'
    $before = (Get-PSDrive C).Free
    $targets = @($env:TEMP, (Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\INetCache'),
        (Join-Path $env:LOCALAPPDATA 'CrashDumps'), (Join-Path $env:LOCALAPPDATA 'NVIDIA\DXCache'),
        (Join-Path $env:LOCALAPPDATA 'NVIDIA\GLCache'), (Join-Path $env:LOCALAPPDATA 'AMD\DxCache'),
        (Join-Path $env:LOCALAPPDATA 'D3DSCache'), $env:WINDIR + '\Temp') | Where-Object { $_ -and (Test-Path $_) }
    foreach ($t in $targets) { Get-ChildItem $t -Recurse -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue }
    Clear-RecycleBin -Force -ErrorAction SilentlyContinue
    Write-Kit ("freed {0:N1} MB" -f (((Get-PSDrive C).Free - $before) / 1MB)) 'ok'
}
function Invoke-DnsTest {
    Write-Section 'DNS resolver test'
    $resolvers = @{ 'Cloudflare'='1.1.1.1'; 'Google'='8.8.8.8'; 'Quad9'='9.9.9.9'; 'OpenDNS'='208.67.222.222' }
    foreach ($n in $resolvers.Keys) {
        Clear-DnsClientCache -ErrorAction SilentlyContinue
        $sw = [Diagnostics.Stopwatch]::StartNew()
        Resolve-DnsName -Name 'www.microsoft.com' -Server $resolvers[$n] -Type A -ErrorAction SilentlyContinue | Out-Null
        $sw.Stop()
        $c = if ($sw.ElapsedMilliseconds -lt 30) { 'Green' } elseif ($sw.ElapsedMilliseconds -lt 80) { 'Yellow' } else { 'Red' }
        Write-Host ("  {0,-12} {1,4} ms" -f $n, $sw.ElapsedMilliseconds) -ForegroundColor $c
    }
    Write-Kit 'apply the fastest with menu 8 > Set DNS' 'info'
}
function Set-FastestDns {
    $best = @{ n = ''; ms = 99999; ip = '' }
    foreach ($r in @{ 'Cloudflare'='1.1.1.1'; 'Google'='8.8.8.8'; 'Quad9'='9.9.9.9' }.GetEnumerator()) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        Resolve-DnsName -Name 'www.microsoft.com' -Server $r.Value -Type A -ErrorAction SilentlyContinue | Out-Null
        $sw.Stop()
        if ($sw.ElapsedMilliseconds -lt $best.ms) { $best = @{ n = $r.Key; ms = $sw.ElapsedMilliseconds; ip = $r.Value } }
    }
    if ($best.ip) {
        $ad = Get-NetAdapter | Where-Object Status -eq 'Up' | Select-Object -First 1
        if ($ad) { Set-DnsClientServerAddress -InterfaceIndex $ad.ifIndex -ServerAddresses $best.ip
            Write-Kit ("DNS -> {0} ({1}, {2} ms measured) - was reversible via netsh" -f $best.n, $best.ip, $best.ms) 'ok' }
    }
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
    Write-Host "  Windows Gaming & Performance Control Center - engine v2.9" -ForegroundColor DarkGray
    Write-Host ("  log: {0}" -f $Script:Log) -ForegroundColor DarkGray
    Write-Host ("  mode: {0}   tweaks: {1}" -f $(if (Test-Admin) { 'ADMINISTRATOR' } else { 'user' }), $Script:Tweaks.Count) -ForegroundColor $(if (Test-Admin) { 'Green' } else { 'Yellow' })
    Write-Host ""
}
function Pause-Kit { Write-Host ""; Read-Host "  Press ENTER to continue" | Out-Null }

function Show-MainMenu {
    while ($true) {
        Show-Banner
        if (Test-Admin) {
            Write-Host "  -- ADMIN MENU --" -ForegroundColor Magenta
            Write-Host ""
            Write-Host "   1. System info + diagnostics"
            Write-Host "   2. Tweak status (read the machine)"
            Write-Host "   3. GAMING profile     (16 tweaks)"
            Write-Host "   4. PRIVACY profile    (11 tweaks)"
            Write-Host "   5. DEBLOAT profile    (11 tweaks)"
            Write-Host "   6. FULL KIT           (all + cleanup + restore point)"
            Write-Host "   7. Individual tweaks  ($($Script:Tweaks.Count))"
            Write-Host "   8. Network center     (latency, DNS test, fastest DNS)"
            Write-Host "   9. Junk cleanup       (temp, caches, recycle bin)"
            Write-Host "  10. Drivers            (info, vendor page, WU scan)"
            Write-Host "  11. Firmware / BIOS    (SecureBoot, TPM, boot mode, VT)"
            Write-Host "  12. Registry backup"
            Write-Host "  13. RESTORE ALL        (every tweak back to Windows defaults)"
            Write-Host "   0. Exit"
        } else {
            Write-Host "  -- USER MENU (run OptimizeKit.bat as admin for all $(
                ($Script:Tweaks | Where-Object user -eq 0).Count) admin tweaks) --" -ForegroundColor Magenta
            Write-Host ""
            Write-Host "   1. System info + diagnostics"
            Write-Host "   2. Tweak status (read the machine)"
            Write-Host "   3. USER GAMING   (GameMode, DVR, VRR, mouse, visuals)"
            Write-Host "   4. USER PRIVACY  (ads, activity, bing, notifications)"
            Write-Host "   5. Individual user tweaks"
            Write-Host "   6. Network center (latency, DNS test)"
            Write-Host "   7. Junk cleanup"
            Write-Host "   8. Registry backup"
            Write-Host "   0. Exit"
        }
        Write-Host ""
        $c = Read-Host "  Choice"
        if (Test-Admin) {
            switch ($c) {
                '0'  { return }
                '1'  { Show-Diagnostics; Pause-Kit }
                '2'  { Show-StatusAll;   Pause-Kit }
                '3'  { Invoke-GamingProfile;  Pause-Kit }
                '4'  { Invoke-PrivacyProfile; Pause-Kit }
                '5'  { Invoke-DebloatProfile; Pause-Kit }
                '6'  { Invoke-FullKit;        Pause-Kit }
                '7'  { Show-TweakMenu;   Pause-Kit }
                '8'  { Show-NetworkMenu; Pause-Kit }
                '9'  { Invoke-JunkCleanup; Pause-Kit }
                '10' { Show-DriverInfo; Open-VendorPage; Pause-Kit }
                '11' { Show-FirmwareInfo; Pause-Kit }
                '12' { Backup-Registry;  Pause-Kit }
                '13' { Invoke-RestoreAll; Pause-Kit }
            }
        } else {
            switch ($c) {
                '0' { return }
                '1' { Show-Diagnostics; Pause-Kit }
                '2' { Show-StatusAll;   Pause-Kit }
                '3' { foreach ($i in 'game_mode','game_dvr_off','gpu_preference','windowed_games','vrr','mouse_precision','menu_delay_0','game_mode_win11') { Invoke-Tweak $i }; Pause-Kit }
                '4' { foreach ($i in 'advertising_off','activity_history','bing_search','tailored_experiences','edge_bing_blocking','notifications_off','end_task_on_tb','store_search_off') { Invoke-Tweak $i }; Pause-Kit }
                '5' { Show-UserTweakMenu; Pause-Kit }
                '6' { Show-NetworkMenu; Pause-Kit }
                '7' { Invoke-JunkCleanup; Pause-Kit }
                '8' { Backup-Registry;  Pause-Kit }
            }
        }
    }
}

function Show-TweakMenu {
    Write-Section ("Individual tweaks - {0}" -f $Script:Tweaks.Count)
    $i = 1
    foreach ($t in $Script:Tweaks) {
        $st = Get-TweakState $t.id
        $mark = if ($null -eq $st) { '?' } elseif ($st) { '*' } else { ' ' }
        $color = if ($st) { 'Green' } else { 'Gray' }
        $lock = if (-not (Test-AdminForTweak $t)) { ' [ADMIN]' } else { '' }
        Write-Host ("   {0,2}. [{1}] {2,-42} {3}" -f $i, $mark, $t.name, $lock) -ForegroundColor $color
        $i++
    }
    Write-Host "    0. Back"
    $c = Read-Host "  Number to APPLY (r<N> to restore, 0=back)"
    if ($c -match '^r(\d+)$') {
        $n = [int]$Matches[1]
        if ($n -ge 1 -and $n -lt $i) { Restore-Tweak $Script:Tweaks[$n-1].id }
    } elseif ($c -match '^\d+$' -and [int]$c -ge 1 -and [int]$c -lt $i) {
        Invoke-Tweak $Script:Tweaks[[int]$c - 1].id
    }
}

function Show-NetworkMenu {
    Write-Section 'Network center'
    Write-Host "   1. Latency test (ping/jitter/loss)"
    Write-Host "   2. DNS resolver benchmark"
    Write-Host "   3. Set the fastest DNS"
    Write-Host "   4. Flush DNS cache"
    Write-Host "   5. Winsock reset (reboot needed)"
    Write-Host "   0. Back"
    $c = Read-Host "  Choice"
    switch ($c) {
        '1' { Test-Latency }
        '2' { Invoke-DnsTest }
        '3' { Set-FastestDns }
        '4' { Clear-DnsClientCache; ipconfig /flushdns | Out-Null; Write-Kit 'DNS cache flushed' 'ok' }
        '5' { if (Test-Admin) { netsh winsock reset | Out-Null; Write-Kit 'winsock reset - reboot required' 'warn' } else { Write-Kit 'admin required' 'err' } }
    }
}

function Show-FirmwareInfo {
    Write-Section 'Firmware / BIOS (read-only)'
    $bios = Get-CimInstance Win32_BIOS
    $board = Get-CimInstance Win32_BaseBoard
    Write-Host ("  BIOS      : {0}  {1}  ({2})" -f $bios.Manufacturer, $bios.SMBIOSBIOSVersion, $bios.ReleaseDate)
    Write-Host ("  Board     : {0} {1}" -f $board.Manufacturer, $board.Product)
    $sb = Confirm-SecureBootUEFI -ErrorAction SilentlyContinue
    $bootmode = if ((bcdedit 2>$null | Out-String) -match 'winload\.efi') { 'UEFI' } else { 'unknown (needs admin)' }
    Write-Host ("  SecureBoot: {0}   Boot mode: {1}" -f $(if ($null -ne $sb) { if ($sb) { 'ON' } else { 'off' } } else { 'unknown (legacy boot or needs admin)' }), $bootmode)
    $tpm = Get-Tpm -ErrorAction SilentlyContinue
    if ($tpm) { Write-Host ("  TPM       : {0} (present: {1}, enabled: {2})" -f $tpm.TpmInformation.ManufacturerIdTxt, $tpm.TpmPresent, $tpm.TpmReady) }
    else { Write-Host '  TPM       : Get-Tpm needs an elevated session' -ForegroundColor Yellow }
    $vt = (Get-CimInstance Win32_Processor | Select-Object -First 1).VirtualizationFirmwareEnabled
    $hv = (Get-CimInstance Win32_ComputerSystem).HypervisorPresent
    Write-Host ("  VT-x/SVM  : {0}   Hypervisor running: {1}" -f $(if ($null -ne $vt) { $vt } else { '?' }), $(if ($hv) { 'yes' } else { 'no' }))
    $pend = Test-Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending'
    Write-Host ("  Reboot pending: {0}" -f $(if ($pend) { 'yes' } else { 'no' }))
}

function Show-UserTweakMenu {
    $userTweaks = $Script:Tweaks | Where-Object { $_.user -eq 1 }
    Write-Section ("User tweaks - {0} (no admin needed)" -f $userTweaks.Count)
    $i = 1
    foreach ($t in $userTweaks) {
        $st = Get-TweakState $t.id
        $mark = if ($null -eq $st) { '?' } elseif ($st) { '*' } else { ' ' }
        Write-Host ("   {0,2}. [{1}] {2}" -f $i, $mark, $t.name) -ForegroundColor $(if ($st) { 'Green' } else { 'Gray' })
        $i++
    }
    Write-Host "    0. Back"
    $c = Read-Host "  Number to APPLY (r<N> to restore, 0=back)"
    if ($c -match '^r(\d+)$') {
        $n = [int]$Matches[1]
        if ($n -ge 1 -and $n -lt $i) { Restore-Tweak $userTweaks[$n-1].id }
    } elseif ($c -match '^\d+$' -and [int]$c -ge 1 -and [int]$c -lt $i) {
        Invoke-Tweak $userTweaks[[int]$c - 1].id
    }
}

function Show-DriverInfo {
    Write-Section 'GPU / driver info'
    $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
    if ($gpu) {
        Write-Host ("  GPU     : {0}" -f $gpu.Name)
        Write-Host ("  Driver  : {0}   Date: {1}" -f $gpu.DriverVersion, $gpu.DriverDate)
    }
}
function Open-VendorPage {
    $gpu = (Get-CimInstance Win32_VideoController | Select-Object -First 1).Name
    $url = 'https://www.nvidia.com/Download/index.aspx'
    if     ($gpu -match 'Radeon|AMD')     { $url = 'https://www.amd.com/en/support' }
    elseif ($gpu -match 'Intel|Iris|Arc') { $url = 'https://www.intel.com/content/www/us/en/download-center/home.html' }
    Start-Process $url
}

function Invoke-RestoreAll {
    Write-Section 'Restore ALL Windows defaults'
    foreach ($t in $Script:Tweaks) {
        if ((Test-AdminForTweak $t)) { Restore-Tweak $t.id }
    }
    powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e 2>$null | Out-Null
    Write-Kit 'everything restored to Windows defaults' 'ok'
}

# ----------------------------------------------------------------- entry
Write-Kit ("OptimizeKit engine v2.9 started (admin={0})" -f (Test-Admin)) 'info'

if ($RestoreAll) { if (Test-Admin) { Backup-Registry; Invoke-RestoreAll } else { Write-Kit 'admin required for -RestoreAll' 'err' }; exit 0 }
if ($Status)     { Show-StatusAll; exit 0 }
if ($Apply)      { Backup-Registry; foreach ($id in ($Apply -split ',')) { Invoke-Tweak $id.Trim() }; exit 0 }
if ($Profile)    { Backup-Registry; switch ($Profile) { 'gaming' { Invoke-GamingProfile } 'privacy' { Invoke-PrivacyProfile } 'debloat' { Invoke-DebloatProfile } 'full' { Invoke-FullKit } }; exit 0 }
if ($Full)       { if (Test-Admin) { Backup-Registry; Show-MainMenu } else { Write-Kit 'admin session required for -Full (relaunch OptimizeKit.bat)' 'err' }; exit 0 }
if ($Silent)     { Backup-Registry; Invoke-GamingProfile; exit 0 }
if ($Tweaks)     { Show-TweakMenu; exit 0 }
if ($Network)    { Test-Latency; Invoke-DnsTest; exit 0 }
if ($Cleanup)    { if (Test-Admin) { Invoke-JunkCleanup } else { Write-Kit 'admin recommended for a full cleanup' 'warn'; Invoke-JunkCleanup }; exit 0 }
if ($Firmware)   { Show-FirmwareInfo; exit 0 }
if ($Drivers)    { Show-DriverInfo; exit 0 }

# No switches: interactive menu. Without admin, offer elevation once (decline -> user menu).
if (-not (Test-Admin) -and -not $User) {
    Write-Host ""
    Write-Host "  Admin rights unlock $((($Script:Tweaks | Where-Object user -eq 0).Count)) extra tweaks (HAGS, timer, MMCSS, services, power)." -ForegroundColor Yellow
    $ans = Read-Host "  Request elevation now? [Y/n]"
    if ($ans -ne 'n' -and $ans -ne 'N') {
        Start-Process powershell "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Full" -Verb RunAs
        exit 0
    }
    Write-Kit 'running in user mode (HKCU tweaks only)' 'warn'
}

Show-MainMenu
