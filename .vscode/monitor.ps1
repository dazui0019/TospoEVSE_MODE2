# Get available serial port names
$portNames = [System.IO.Ports.SerialPort]::GetPortNames()

# Read config.ini
$config = Get-Content -Path "config.ini" | ConvertFrom-StringData

if (-not $config.ContainsKey("monitor_port") -or -not $config.ContainsKey("monitor_speed") -or -not $config.ContainsKey("monitor_parity")) {
    Write-Host "Error: Missing configuration parameters in config.ini"
    exit
}

$portName = $config.monitor_port
$baudRate = $config.monitor_speed
$parity = $config.monitor_parity
$dataBits = 8
$stopBits = 1

if ($parity -eq "N") {
    $parity = [System.IO.Ports.Parity]::None
    $dataBits = 8
    $stopBits = 1
} elseif ($parity -eq "E") {
    $parity = [System.IO.Ports.Parity]::Even
    $dataBits = 7
    $stopBits = 2
} elseif ($parity -eq "O") {
    $parity = [System.IO.Ports.Parity]::Odd
    $dataBits = 7
    $stopBits = 2
}

# Check if the configured port is available
if (-not ($portNames -contains $portName)) {
    Write-Host "Error: Configured port $portName is not available"
    Write-Host "Available ports: $portNames"
    exit
}

# Create a serial port object
$serialPort = New-Object System.IO.Ports.SerialPort $portName,$baudRate,$parity,$dataBits,$stopBits

# Output serial port information
Write-Host "$portName-$baudRate-$parity-$dataBits-$stopBits"

# Open the serial port
$serialPort.Open()

# Read data and display it in the terminal
try {
    while ($true) {
        if ($serialPort.BytesToRead -gt 0) {
            $data = $serialPort.ReadLine()
            Write-Host $data
        }
        Start-Sleep -Milliseconds 10
    }
} catch {
    Write-Host "Error: " $_.Exception.Message
} finally {
    # Close the serial port
    $serialPort.Close()
}
