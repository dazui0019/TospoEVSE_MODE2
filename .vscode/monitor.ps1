# Get available serial port names
$portNames = [System.IO.Ports.SerialPort]::GetPortNames()

# List all available serial ports
Write-Host "Available serial ports:"
for ($i = 0; $i -lt $portNames.Length; $i++) {
    Write-Host "$i. $($portNames[$i])"
}

# Let the user select a serial port
$selection = Read-Host "Please select the serial port number to monitor"
$portName = $portNames[$selection]

# Create a serial port object
$serialPort = New-Object System.IO.Ports.SerialPort $portName,115200,None,8,One

# Output serial port information
Write-Host "$portName-115200-8-N-1"

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
