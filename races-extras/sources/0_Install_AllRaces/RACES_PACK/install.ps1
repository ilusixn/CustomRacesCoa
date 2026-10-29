# 4 custom races (Banshee 84, Elune's Chosen 85, Faceless One 86, Qiraji 87) for a CoA / Ascension-client AzerothCore server.
# Edit the 6 paths/settings below, then: right-click > Run with PowerShell (server + client closed). Backups: *.PRE_RACES next to every replaced file.
param(
  [string]$Source    = 'C:\path\to\azerothcore-wotlk-coa',      # server source (rebuild worldserver after this script)
  [string]$ServerDbc = 'C:\path\to\server\Data\dbc',            # worldserver DataDir\dbc
  [string]$ClientData= 'C:\path\to\client\Data',                # game client Data folder (every player needs the client files)
  [string]$MySqlBin  = 'C:\path\to\mysql\bin',
  [string]$DbHost = '127.0.0.1', [string]$DbPort = '3306', [string]$DbUser = 'root', [string]$WorldDb = 'acore_world'
)
$ErrorActionPreference = 'Stop'; $here = Split-Path -Parent $MyInvocation.MyCommand.Path
function Put($src, $dst) { if ((Test-Path $dst) -and -not (Test-Path "$dst.PRE_RACES")) { Copy-Item $dst "$dst.PRE_RACES" }; Copy-Item $src $dst -Force; Write-Host "installed $dst" }
# 1. client files (copy client\Data into every player's client)
Get-ChildItem "$here\client\Data\*.MPQ" | ForEach-Object { Put $_.FullName "$ClientData\$($_.Name)" }
New-Item -ItemType Directory -Force "$ClientData\dbc" | Out-Null; Put "$here\client\Data\dbc\Spell.dbc" "$ClientData\dbc\Spell.dbc"
if (Test-Path "$ClientData\..\Cache") { Rename-Item "$ClientData\..\Cache" "Cache.PRE_RACES_$(Get-Date -Format yyyyMMddHHmmss)" }
# 2. server dbc
Get-ChildItem "$here\server\dbc\*.dbc" | ForEach-Object { Put $_.FullName "$ServerDbc\$($_.Name)" }
# 3. world database (backup first)
$pw = Read-Host "MySQL password for $DbUser"; $env:MYSQL_PWD = $pw
& "$MySqlBin\mysqldump.exe" -h $DbHost -P $DbPort -u $DbUser --single-transaction --no-tablespaces "--result-file=$here\PRE_RACES_world_backup.sql" $WorldDb playercreateinfo playercreateinfo_item playercreateinfo_action player_race_stats ascension_custom_class_race spell_dbc creature_model_info
if ($LASTEXITCODE) { throw 'backup failed' }
Get-ChildItem "$here\server\sql\*.sql" | Sort-Object Name | ForEach-Object { cmd /c "`"$MySqlBin\mysql.exe`" -h $DbHost -P $DbPort -u $DbUser --default-character-set=utf8mb4 $WorldDb < `"$($_.FullName)`""; if ($LASTEXITCODE) { throw "SQL failed: $($_.Name)" }; Write-Host "sql $($_.Name)" }
Remove-Item Env:\MYSQL_PWD
# 4. source edits; then rebuild + replace worldserver yourself
py -3 "$here\server\src\apply_races_src.py" $Source; if ($LASTEXITCODE) { throw 'source edit failed' }
Write-Host 'DONE. Now rebuild worldserver, start the server, create a character on the new races.'
