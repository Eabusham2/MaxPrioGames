param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$OutputDirectory = 'test-output'
)
. "$PSScriptRoot/Test-Support.ps1"
Ensure-TestDesktop
$Executable = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$previousConfig = $env:MAXPRIOGAMES_CONFIG_DIR
$env:MAXPRIOGAMES_CONFIG_DIR = Join-Path $OutputDirectory 'portable-config'
New-Item -ItemType Directory -Path $env:MAXPRIOGAMES_CONFIG_DIR -Force | Out-Null
$portableDir = Join-Path $OutputDirectory 'Portable Location With Spaces'
New-Item -ItemType Directory -Path $portableDir -Force | Out-Null
$portable = Join-Path $portableDir 'MaxPrioGames.exe'
Copy-Item $Executable $portable -Force
$process = $null
$checks = [ordered]@{}

try {
    $smokePath = Join-Path $OutputDirectory 'ui-smoke.json'
    Invoke-AppCommand $portable @('--smoke-test', ('"{0}"' -f $smokePath)) 90
    Assert-True (Test-Path $smokePath) 'The UI smoke test did not write its report.'
    $smoke = Get-Content $smokePath -Raw | ConvertFrom-Json
    Assert-True ($smoke.passed -eq $true) 'The native UI smoke test failed.'
    $checks.nativeUiSmoke = $true

    # A clean portable location opens settings on its first explicit launch.
    $process = Start-Process -FilePath $portable -PassThru
    Start-Sleep -Seconds 3
    $process.Refresh()
    Assert-True (-not $process.HasExited) 'The portable app exited unexpectedly.'
    Assert-True ([MaxPrioGamesTests.Windows]::VisibleMainWindows($process.Id) -eq 1) 'First portable launch did not show the main window.'
    $checks.portableFirstLaunchShowsWindow = $true

    $second = Start-Process -FilePath $portable -PassThru
    Assert-True ($second.WaitForExit(10000)) 'A second application instance remained running.'
    Assert-True ($second.ExitCode -eq 0) 'Second instance did not exit cleanly.'
    $process.Refresh()
    Assert-True (-not $process.HasExited) 'The existing instance was unexpectedly terminated.'
    $checks.singleInstance = $true
    Stop-Monitor $portable $process
    $process = $null

    $process = Start-Process -FilePath $portable -PassThru
    Start-Sleep -Seconds 3
    $process.Refresh()
    Assert-True (-not $process.HasExited) 'The second normal launch exited unexpectedly.'
    Assert-True ([MaxPrioGamesTests.Windows]::VisibleMainWindows($process.Id) -eq 0) 'Subsequent manual launch did not remain in the tray.'
    $checks.laterManualLaunchIsTrayOnly = $true
    Stop-Monitor $portable $process
    $process = $null

    $process = Start-Process -FilePath $portable -ArgumentList '--startup' -PassThru
    Start-Sleep -Seconds 3
    $process.Refresh()
    Assert-True (-not $process.HasExited) 'The startup launch exited unexpectedly.'
    Assert-True ([MaxPrioGamesTests.Windows]::VisibleMainWindows($process.Id) -eq 0) 'Startup launch showed a window.'
    $checks.startupIsTrayOnly = $true
    Stop-Monitor $portable $process
    $process = $null

    Invoke-AppCommand $portable @('--enable-startup')
    $tasks = @(Get-MonitorTask $portable)
    Assert-True ($tasks.Count -eq 1) 'Portable startup did not create exactly one task for its actual location.'
    Assert-True ($tasks[0].Actions[0].Arguments -eq '--startup') 'Startup task does not use --startup.'
    Assert-True ($tasks[0].Principal.RunLevel -eq 'Highest') 'Startup task is not configured for process priority and affinity access.'
    Assert-True ($tasks[0].Triggers.Count -ge 1) 'Startup task has no logon trigger.'
    $checks.portableStartupUsesActualPath = $true
    Invoke-AppCommand $portable @('--disable-startup')
    Assert-True (@(Get-MonitorTask $portable).Count -eq 0) 'Portable startup task was not removed.'
    $checks.portableStartupRemoval = $true
    $checks.passed = $true
    $checks | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'windows-integration.json') -Encoding utf8
    $checks | ConvertTo-Json -Depth 5 | Write-Output
}
finally {
    if ($null -ne $process) { Stop-Monitor $portable $process }
    if (@(Get-MonitorTask $portable).Count -gt 0) {
        Invoke-AppCommand $portable @('--disable-startup')
    }
    $env:MAXPRIOGAMES_CONFIG_DIR = $previousConfig
}
