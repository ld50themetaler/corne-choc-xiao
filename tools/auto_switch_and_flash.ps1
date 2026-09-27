# Copy new UF2 from WSL
wsl bash -c "cp /home/ld50/zmk_build/artifacts/chippy_left_xiao_ble.uf2 /mnt/c/Antigravity/Corne_Xiao_Choc/chippy_left_xiao_ble.uf2"
Write-Host "Copied UF2. Checking size: $(Get-Item C:\Antigravity\Corne_Xiao_Choc\chippy_left_xiao_ble.uf2 | Select-Object -ExpandProperty Length)"

# Try sending bootloader commands
Write-Host "Sending 'bootloader' command to COM3..."
try {
    $p = New-Object System.IO.Ports.SerialPort "COM3", 115200
    $p.DtrEnable = $true
    $p.RtsEnable = $true
    $p.Open()
    $p.WriteLine("bootloader")
    Start-Sleep -Milliseconds 300
    $p.Close()
    Write-Host "Command sent."
} catch {
    Write-Host "COM3 error: $($_.Exception.Message)"
}

Write-Host "Waiting for UF2 bootloader drive (D:\)..."
$timeout = [DateTime]::Now.AddSeconds(60)
while ([DateTime]::Now -lt $timeout) {
    $drive = Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path "$($_.Root)INFO_UF2.TXT" }
    if ($drive) {
        Write-Host "Bootloader drive detected at $($drive.Root)! Flashing..."
        Start-Sleep -Milliseconds 500
        Copy-Item "C:\Antigravity\Corne_Xiao_Choc\chippy_left_xiao_ble.uf2" -Destination $drive.Root -Force
        Write-Host "Flashed successfully! Waiting for reboot..."
        Start-Sleep -Seconds 4
        Get-PnpDevice -Class Keyboard,HIDClass,Ports -PresentOnly | Where-Object { $_.InstanceId -match '1D50' } | Format-Table FriendlyName, Class, InstanceId
        exit 0
    }
    Start-Sleep -Milliseconds 300
}

Write-Host "Timed out waiting for drive."
exit 1
