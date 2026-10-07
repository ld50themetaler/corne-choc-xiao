param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("left", "right")]
    [string]$Side,

    [string]$Port
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

# If not given, determine default port based on current devices
if (-not $Port) {
    if ($Side -eq "left") {
        $Port = "COM7"
    } else {
        $Port = "COM5"
    }
}

$uf2Name = "chippy_${Side}_xiao_ble.uf2"
$uf2Paths = @(
    "C:\Antigravity\Corne_Xiao_Choc\$uf2Name",
    "$projectDir\firmware\$uf2Name"
)

$uf2Path = $null
foreach ($p in $uf2Paths) {
    if (Test-Path $p) {
        $uf2Path = $p
        break
    }
}

if (-not $uf2Path) {
    Write-Error "UF2 file not found: $uf2Name"
    exit 1
}

Write-Host "=========================================="
Write-Host "Flashing $Side firmware: $uf2Path"
Write-Host "Target COM Port: $Port"
Write-Host "=========================================="

# Check if drive already in bootloader mode
$bootDrive = Get-Volume | Where-Object { $_.FileSystemLabel -match "XIAO|NRF52|UF2|BOOT" } | Select-Object -First 1

if (-not $bootDrive) {
    Write-Host "Triggering 1200bps bootloader reset on $Port..."
    try {
        $serial = New-Object System.IO.Ports.SerialPort $Port, 1200, "None", 8, "One"
        $serial.DtrEnable = $true
        $serial.Open()
        Start-Sleep -Milliseconds 200
        $serial.DtrEnable = $false
        $serial.Close()
        Write-Host "1200bps touch sent successfully."
    } catch {
        Write-Warning "Could not open $Port (might already be in bootloader or port differs): $_"
    }

    Write-Host "Waiting for bootloader drive to appear..."
    $timeout = 10
    $elapsed = 0
    while (-not $bootDrive -and $elapsed -lt $timeout) {
        Start-Sleep -Seconds 1
        $elapsed++
        $bootDrive = Get-Volume | Where-Object { $_.FileSystemLabel -match "XIAO|NRF52|UF2|BOOT" } | Select-Object -First 1
    }
}

if (-not $bootDrive) {
    Write-Error "Bootloader drive not found after $timeout seconds. If this is the first flash with the new firmware, double-tap the reset button on the Xiao."
    exit 1
}

$driveLetter = "$($bootDrive.DriveLetter):\"
Write-Host "Found bootloader drive: $driveLetter ($($bootDrive.FileSystemLabel))"

Write-Host "Copying $uf2Path to $driveLetter..."
Copy-Item -Path $uf2Path -Destination $driveLetter -Force

Write-Host "Write complete! Keyboard will reboot automatically."
