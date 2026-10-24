# Live progress of copying a folder to the G2: compares the size on the device with the source folder's.
# powershell -File push_progress.ps1 [-Src D:\Games\UnchartedPC\Game] -Dst /storage/<id SD>/.../CUSA02344
param(
    [string]$Src = 'D:\Games\UnchartedPC\Game',
    [string]$Dst = '/storage/XXXX-XXXX/ES-DE/ROMs/ps4/CUSA02344'
)
$pt = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Google.PlatformTools_*\platform-tools" -Directory | Select-Object -First 1
$adb = Join-Path $pt.FullName 'adb.exe'
$total = [math]::Round((Get-ChildItem $Src -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
$host.UI.RawUI.WindowTitle = "Copying to the G2"
$prev = $null; $prevT = $null
while ($true) {
    $mb = [int](((& $adb shell "du -sm '$Dst' 2>/dev/null") -split '\s+')[0])
    $now = Get-Date
    $speed = 0
    if ($prevT) { $speed = ($mb - $prev) / ($now - $prevT).TotalSeconds }
    $prev = $mb; $prevT = $now
    $pct = [math]::Min(100, [math]::Round(100 * $mb / $total, 1))
    $eta = if ($speed -gt 0) { $now.AddSeconds(($total - $mb) / $speed).ToString('HH:mm') } else { '?' }
    $status = "{0:N0} / {1:N0} MiB  ({2}%)  {3:N1} MiB/s  done at ~{4}" -f $mb, $total, $pct, $speed, $eta
    Write-Progress -Activity "Dump -> G2 ($Dst)" -Status $status -PercentComplete $pct
    Write-Host ("{0}  {1}" -f $now.ToString('HH:mm:ss'), $status)
    if ($mb -ge $total) { Write-Host "Done (the size matches)."; break }
    Start-Sleep 10
}
Read-Host "Enter to close"
