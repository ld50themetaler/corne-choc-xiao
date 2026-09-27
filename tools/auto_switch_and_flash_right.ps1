param(
    [string]$ComPort = ""
)

$targetUf2 = "C:\Antigravity\Corne_Xiao_Choc\chippy_right_xiao_ble.uf2"
if (!(Test-Path $targetUf2)) {
    Write-Host "Target UF2 not found: $targetUf2"
    exit 1
}

# If COM port not provided, look for connected serial port
if ([string]::IsNullOrEmpty($ComPort)) {
    $ports = Get-CimInstance Win32_SerialPort | Where-Object { $_.PNPDeviceID -match '1D50' -or $_.Name -match 'COM' }
    if ($ports) {
        $ComPort = $ports[0].DeviceID
    }
}

if (![string]::IsNullOrEmpty($ComPort)) {
    Write-Host "Sending 'bootloader' command to $ComPort..."
    try {
        $p = New-Object System.IO.Ports.SerialPort $ComPort, 115200
        $p.DtrEnable = $true
        $p.RtsEnable = $true
        $p.Open()
        $p.WriteLine("bootloader")
        Start-Sleep -Milliseconds 300
        $p.Close()
        Write-Host "Bootloader command sent to $ComPort."
    } catch {
        Write-Host "COM error: $($_.Exception.Message)"
    }
}

Write-Host "Waiting for UF2 bootloader drive..."
$timeout = [DateTime]::Now.AddSeconds(60)
while ([DateTime]::Now -lt $timeout) {
    $drive = Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path "$($_.Root)INFO_UF2.TXT" }
    if ($drive) {
        Write-Host "Bootloader drive detected at $($drive.Root)! Flashing Right firmware..."
        Start-Sleep -Milliseconds 500
        Copy-Item $targetUf2 -Destination $drive.Root -Force
        Write-Host "Right side flashed successfully! Waiting for reboot..."
        Start-Sleep -Seconds 4
        Get-PnpDevice -Class Keyboard,HIDClass,Ports -PresentOnly | Where-Object { $_.InstanceId -match '1D50' } | Format-Table FriendlyName, Class, InstanceId
        exit 0
    }
    Start-Sleep -Milliseconds 300
}

Write-Host "Timed out waiting for bootloader drive."
exit 1
