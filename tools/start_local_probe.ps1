param(
    [ValidateRange(1, 65535)]
    [int]$Port = 18080
)

$ErrorActionPreference = 'Stop'
$listener = [System.Net.Sockets.TcpListener]::new(
    [System.Net.IPAddress]::Any,
    $Port
)

try {
    $listener.Start()
    Write-Host "Local TCP probe listening on port $Port."
    Write-Host "Keep this window open. Stop it with Ctrl+C."
    Write-Host "If Windows Firewall asks, allow access on Private networks."

    while ($true) {
        $client = $listener.AcceptTcpClient()
        try {
            $remote = $client.Client.RemoteEndPoint
            $stream = $client.GetStream()
            $stream.ReadTimeout = 1000
            $buffer = New-Object byte[] 32
            $text = ''
            try {
                $read = $stream.Read($buffer, 0, $buffer.Length)
                if ($read -gt 0) {
                    $text = [Text.Encoding]::ASCII.GetString($buffer, 0, $read).Trim()
                }
            } catch [System.IO.IOException] {
                # The TCP handshake itself is the diagnostic; payload is optional.
            }

            $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
            Write-Host "[$timestamp] Connection from $remote payload='$text'"
            $reply = [Text.Encoding]::ASCII.GetBytes("OK`n")
            $stream.Write($reply, 0, $reply.Length)
            $stream.Flush()
        } finally {
            $client.Close()
        }
    }
} finally {
    $listener.Stop()
}
