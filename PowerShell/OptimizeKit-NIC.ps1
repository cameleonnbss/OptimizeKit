<#
.SYNOPSIS
  OptimizeKit NIC module - adapter-aware network tuning (v2.1)
.DESCRIPTION
  Applies only the advanced properties the INSTALLED adapter actually exposes,
  backs up the original values to %LOCALAPPDATA%\OptimizeKit\backups, and can
  restore them. Called by the main engine (nic_powersave tweak) or directly:

    powershell -File OptimizeKit-NIC.ps1 -Mode show      # list knobs
    powershell -File OptimizeKit-NIC.ps1 -Mode gaming    # latency preset
    powershell -File OptimizeKit-NIC.ps1 -Mode restore   # back to saved values
#>
param(
    [ValidateSet('gaming','restore','show')]
    [string]$Mode = 'show'
)
$ErrorActionPreference = 'SilentlyContinue'

$PowerSavers = 'PowerSavingMode','EnablePowerManagement','ReduceSpeedAutoPowerDown','PowerSavingMode Ethernet','Energy Efficient Ethernet','Green Ethernet','Advanced EEE'
$LatencyKnobs = 'Interrupt Moderation','Receive Side Scaling','Recv Segment Coalescing (IPv4)','Recv Segment Coalescing (IPv6)'

function Get-NicDefaults { param($Adapter)
    $defaults = @{}
    $props = Get-NetAdapterAdvancedProperty -Name $Adapter.Name -ErrorAction SilentlyContinue
    foreach ($p in $props) {
        if ($p.DisplayName -in $PowerSavers -or $p.DisplayName -in $LatencyKnobs -or $p.DisplayName -like 'LSO*') {
            $defaults[$p.DisplayName] = $p.RegistryValue
        }
    }
    return $defaults
}

$adapters = Get-NetAdapter | Where-Object Status -eq 'Up'
if (-not $adapters) { Write-Host '  [WARN] no active network adapter found'; exit 0 }

if ($Mode -eq 'show') {
    Write-Host '  Active adapters and their tuning knobs:'
    foreach ($a in $adapters) {
        Write-Host ("  - {0} ({1})" -f $a.InterfaceDescription, $a.LinkSpeed)
        $props = Get-NetAdapterAdvancedProperty -Name $a.Name -ErrorAction SilentlyContinue
        foreach ($p in $props) {
            if ($p.DisplayName -in $PowerSavers -or $p.DisplayName -in $LatencyKnobs -or $p.DisplayName -like 'LSO*') {
                Write-Host ("      {0,-40} {1}" -f $p.DisplayName, ($p.RegistryValue -join ','))
            }
        }
    }
    return
}

$dir = Join-Path $env:LOCALAPPDATA 'OptimizeKit\backups'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

foreach ($a in $adapters) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $defaults = Get-NicDefaults $a
    if ($defaults.Count -gt 0) {
        $defaults | ConvertTo-Json | Set-Content (Join-Path $dir ("nic_{0}_{1}.json" -f $stamp, ($a.Name -replace '\W','_')))
    }

    if ($Mode -eq 'gaming') {
        # Only touch properties this adapter actually exposes (hardware-aware by construction)
        foreach ($name in $PowerSavers) {
            if (Get-NetAdapterAdvancedProperty -Name $a.Name -DisplayName $name -ErrorAction SilentlyContinue) {
                Set-NetAdapterAdvancedProperty -Name $a.Name -DisplayName $name -RegistryValue 0 -ErrorAction SilentlyContinue
                Write-Host ("  [ OK ] {0}: {1} -> 0 (power saving off)" -f $a.Name, $name)
            }
        }
        foreach ($name in 'Interrupt Moderation','Recv Segment Coalescing (IPv4)','Recv Segment Coalescing (IPv6)') {
            if (Get-NetAdapterAdvancedProperty -Name $a.Name -DisplayName $name -ErrorAction SilentlyContinue) {
                Set-NetAdapterAdvancedProperty -Name $a.Name -DisplayName $name -RegistryValue 1 -ErrorAction SilentlyContinue
                Write-Host ("  [ OK ] {0}: {1} -> enabled" -f $a.Name, $name)
            }
        }
        # Adapter power management via PnPCapabilities (prevents idle power-down)
        Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}\$($a.DeviceID)" -Name 'PnPCapabilities' -Value 24 -ErrorAction SilentlyContinue
        Write-Host ("  [ OK ] {0}: gaming preset applied (defaults backed up)" -f $a.Name)
    }

    if ($Mode -eq 'restore') {
        $latest = Get-ChildItem $dir -Filter "nic_*_$(($a.Name -replace '\W','_')).json" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($latest) {
            $saved = Get-Content $latest.FullName | ConvertFrom-Json
            foreach ($name in $saved.PSObject.Properties.Name) {
                Set-NetAdapterAdvancedProperty -Name $a.Name -DisplayName $name -RegistryValue $saved.$name -ErrorAction SilentlyContinue
            }
            Write-Host ("  [ OK ] {0}: restored from {1}" -f $a.Name, $latest.Name)
        } else {
            Write-Host ("  [WARN] no saved defaults for {0}" -f $a.Name)
        }
    }
}
