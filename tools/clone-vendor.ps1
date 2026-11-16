# Recreate vendor clones (shallow). Run from repo root.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
New-Item -ItemType Directory -Force -Path vendor | Out-Null

$repos = @{
  "MeshCore" = "https://github.com/meshcore-dev/MeshCore.git"
  "esp32-rfid" = "https://github.com/pvtex/esp32-rfid.git"
  "esp-rfid" = "https://github.com/esprfid/esp-rfid.git"
  "eeh-esp32-rfid" = "https://github.com/smford/eeh-esp32-rfid.git"
  "HAPRS" = "https://github.com/rahul-chatterjee-27/HAPRS.git"
  "meshcore-open" = "https://github.com/zjs81/meshcore-open.git"
  "meshcore-client" = "https://github.com/OffbandMesh/meshcore-client.git"
  "meshcore.js" = "https://github.com/meshcore-dev/meshcore.js.git"
  "meshcore-cli" = "https://github.com/fdlamotte/meshcore-cli.git"
  "pidoors" = "https://github.com/sybethiesant/pidoors.git"
  "esp32-rfid-access-control" = "https://github.com/darthshadoww/esp32-rfid-access-control.git"
  "WiegandNG" = "https://github.com/pvtex/Wiegand-NG-Multi-Bit-Wiegand-Library-for-Arduino.git"
  "hass-hubc2000pp" = "https://github.com/Regressor/hass-hubc2000pp.git"
}

foreach ($name in $repos.Keys) {
  $dest = "vendor/$name"
  if (Test-Path $dest) {
    Write-Host "skip $name (exists)"
    continue
  }
  git clone --depth 1 $repos[$name] $dest
}
