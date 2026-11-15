param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [string]$OutputDirectory = 'test-output'
)
. "$PSScriptRoot/Test-Support.ps1"
Ensure-TestDesktop
$Installer = (Resolve-Path $Installer).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$installDirectory = Join-Path $OutputDirectory 'Installed Location With Spaces'
$installedExe = Join-Path $installDirectory 'MaxPrioGames.exe'
$previousConfig = $env:MAXPRIOGAMES_CONFIG_DIR
$env:MAXPRIOGAMES_CONFIG_DIR = Join-Path $OutputDirectory 'installer-config'
New-Item -ItemType Directory -Path $env:MAXPRIOGAMES_CONFIG_DIR -Force | Out-Null
$desktopPaths = @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('CommonDesktopDirectory'))
$desktopBefore = @($desktopPaths | ForEach-Object { Get-ChildItem $_ -Filter '*MaxPrioGames*.lnk' -ErrorAction SilentlyContinue } | Select-Object -ExpandProperty FullName)
$menuLink = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'MaxPrioGames.lnk'
$checks = [ordered]@{}

try {
    $logPath = Join-Path $OutputDirectory 'installer.log'
    $setup = Start-Process -FilePath $Installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', ('/DIR="{0}"' -f $installDirectory), ('/LOG="{0}"' -f $logPath)) -PassThru
    Assert-True ($setup.WaitForExit(120000)) 'Silent installer timed out.'
    Assert-True ($setup.ExitCode -eq 0) "Silent installer failed with exit code $($setup.ExitCode)."
    Assert-True (Test-Path $installedExe) 'Installed executable is missing.'
    Assert-True (Test-Path (Join-Path $installDirectory 'installed.flag')) 'Installed-mode marker is missing.'
    $checks.installedFiles = $true
    $checks.installedProgramBytes = [long]((Get-ChildItem $installDirectory -File -Recurse | Measure-Object Length -Sum).Sum)

    $tasks = @(Get-MonitorTask $installedExe)
    Assert-True ($tasks.Count -eq 1) 'Installer did not register its startup task.'
    Assert-True ($tasks[0].Actions[0].Arguments -eq '--startup') 'Installed startup does not run in the tray.'
    Assert-True ($tasks[0].Principal.RunLevel -eq 'Highest') 'Installed startup lacks process priority permissions.'
    $checks.startupRegistered = $true

    Assert-True (Test-Path $menuLink) 'Start menu search entry is missing.'
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($menuLink)
    Assert-True ($shortcut.TargetPath -eq $installedExe) 'Start menu entry points to the wrong executable.'
    Assert-True ([string]::IsNullOrEmpty($shortcut.Arguments)) 'Start menu entry should use the normal tray-first launch.'
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($shortcut) | Out-Null
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($shell) | Out-Null
    $checks.startMenuSearchEntry = $true

    $desktopAfter = @($desktopPaths | ForEach-Object { Get-ChildItem $_ -Filter '*MaxPrioGames*.lnk' -ErrorAction SilentlyContinue } | Select-Object -ExpandProperty FullName)
    $newDesktopEntries = @($desktopAfter | Where-Object { $_ -notin $desktopBefore })
    Assert-True ($newDesktopEntries.Count -eq 0) 'Installer created a desktop shortcut.'
    $checks.noDesktopShortcut = $true

    Start-Sleep -Seconds 2
    $running = @(Get-Process -Name MaxPrioGames -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $installedExe })
    Assert-True ($running.Count -eq 1) 'Installer did not start exactly one app instance.'
    Assert-True ([MaxPrioGamesTests.Windows]::VisibleMainWindows($running[0].Id) -eq 0) 'Installed app should launch in the tray.'
    $checks.installerStartsAppInTray = $true
    Stop-Monitor $installedExe $running[0]

    $uninstaller = Join-Path $installDirectory 'unins000.exe'
    Assert-True (Test-Path $uninstaller) 'Uninstaller is missing.'
    $uninstallLog = Join-Path $OutputDirectory 'uninstaller.log'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', ('/LOG="{0}"' -f $uninstallLog)) -PassThru
    Assert-True ($uninstall.WaitForExit(120000)) 'Silent uninstaller timed out.'
    Assert-True ($uninstall.ExitCode -eq 0) 'Silent uninstaller failed.'
    Assert-True (-not (Test-Path $installedExe)) 'Uninstall left the app executable behind.'
    Assert-True (-not (Test-Path $menuLink)) 'Uninstall left the Start menu entry behind.'
    Assert-True (@(Get-MonitorTask $installedExe).Count -eq 0) 'Uninstall left the startup task behind.'
    $checks.uninstallCleansFilesMenuAndStartup = $true
    $checks.passed = $true
    $checks | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'installer-integration.json') -Encoding utf8
    $checks | ConvertTo-Json -Depth 5 | Write-Output
}
finally {
    $env:MAXPRIOGAMES_CONFIG_DIR = $previousConfig
}
