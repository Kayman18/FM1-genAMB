$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$port = 8765
$url = "http://127.0.0.1:$port/"
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
$listener.Start()
$browser = $null
foreach ($name in @('msedge.exe','chrome.exe','chromium.exe')) {
  $cmd = Get-Command $name -ErrorAction SilentlyContinue
  if ($cmd) { $browser = $cmd.Source; break }
}
if ($browser) { Start-Process $browser $url } else { Start-Process $url }
Write-Host "genAMB FM-1 USB-C Editor: $url"
Write-Host "Chrome/Edge/Chromium with Web MIDI SysEx is recommended."
Write-Host "Press Ctrl+C to stop."
try {
  while ($true) {
    $client = $listener.AcceptTcpClient()
    try {
      $stream = $client.GetStream()
      $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::ASCII, $false, 4096, $true)
      $request = $reader.ReadLine()
      if (-not $request) { $client.Close(); continue }
      do { $line = $reader.ReadLine() } while ($line -ne $null -and $line -ne '')
      $parts = $request.Split(' ')
      $path = if ($parts.Length -gt 1) { [Uri]::UnescapeDataString($parts[1].Split('?')[0]) } else { '/' }
      if ($path -eq '/') { $path = '/index.html' }
      $full = [System.IO.Path]::GetFullPath((Join-Path $root $path.TrimStart('/')))
      if (-not $full.StartsWith([System.IO.Path]::GetFullPath($root)) -or -not (Test-Path $full -PathType Leaf)) {
        $body = [Text.Encoding]::UTF8.GetBytes('Not found')
        $head = "HTTP/1.1 404 Not Found`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n"
      } else {
        $body = [IO.File]::ReadAllBytes($full)
        $ext = [IO.Path]::GetExtension($full).ToLowerInvariant()
        $mime = switch ($ext) { '.html' {'text/html; charset=utf-8'} '.js' {'text/javascript; charset=utf-8'} '.css' {'text/css; charset=utf-8'} '.txt' {'text/plain; charset=utf-8'} '.genamb' {'application/json; charset=utf-8'} default {'application/octet-stream'} }
        $head = "HTTP/1.1 200 OK`r`nContent-Type: $mime`r`nCache-Control: no-store`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n"
      }
      $hb = [Text.Encoding]::ASCII.GetBytes($head); $stream.Write($hb,0,$hb.Length); $stream.Write($body,0,$body.Length); $stream.Flush()
    } finally { $client.Close() }
  }
} finally { $listener.Stop() }
