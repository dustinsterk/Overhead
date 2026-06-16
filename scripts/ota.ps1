#requires -version 5
# OTA-flash the built firmware to the device over WiFi (ElegantOTA v3).
# Stable single-command invocation so it can be allow-listed once.
param([string]$ip = "192.168.86.92", [string]$user = "admin", [string]$pass = "overhead",
      [string]$env = "cyd28_ili9341")
$ErrorActionPreference = "Stop"
$f = Join-Path $PSScriptRoot "..\.pio\build\$env\firmware.bin"
$h = (Get-FileHash $f -Algorithm MD5).Hash.ToLower()
& curl.exe -s -m 15 -u "${user}:${pass}" "http://$ip/ota/start?mode=fr&hash=$h" | Out-Null
& curl.exe -s -m 120 -u "${user}:${pass}" -F "file=@$f;filename=firmware.bin" "http://$ip/ota/upload" -w "`nupload=%{http_code}`n"
