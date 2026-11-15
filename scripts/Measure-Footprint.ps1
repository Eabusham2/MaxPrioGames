param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$OutputDirectory = 'test-output',
    [ValidateRange(35, 300)][int]$SampleSeconds = 40
)
. "$PSScriptRoot/Test-Support.ps1"
Ensure-TestDesktop
$Executable = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$previousConfig = $env:MAXPRIOGAMES_CONFIG_DIR
$env:MAXPRIOGAMES_CONFIG_DIR = Join-Path $OutputDirectory 'benchmark-config'
New-Item -ItemType Directory -Path $env:MAXPRIOGAMES_CONFIG_DIR -Force | Out-Null
$internalReport = Join-Path $OutputDirectory 'benchmark-internal.json'
$process = $null

try {
    $duration = $SampleSeconds + 10
    $process = Start-Process -FilePath $Executable -ArgumentList @('--benchmark', "$duration", ('"{0}"' -f $internalReport)) -PassThru
    Start-Sleep -Seconds 5
    $process.Refresh()
    Assert-True (-not $process.HasExited) 'Benchmark process exited during warmup.'
    Assert-True ([MaxPrioGamesTests.Windows]::VisibleMainWindows($process.Id) -eq 0) 'Benchmark must measure tray-only operation.'
    $initialCpu = $process.TotalProcessorTime.TotalSeconds
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $samples = @()
    for ($index = 0; $index -lt $SampleSeconds; ++$index) {
        Start-Sleep -Seconds 1
        $process.Refresh()
        Assert-True (-not $process.HasExited) 'Benchmark process exited during sampling.'
        $samples += [pscustomobject]@{
            elapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
            workingSetBytes = $process.WorkingSet64
            privateBytes = $process.PrivateMemorySize64
            handles = $process.HandleCount
            threads = $process.Threads.Count
        }
    }
    $timer.Stop()
    $process.Refresh()
    $cpuSeconds = $process.TotalProcessorTime.TotalSeconds - $initialCpu
    $cpuPctOneCore = 100 * $cpuSeconds / $timer.Elapsed.TotalSeconds
    $logicalProcessors = [Environment]::ProcessorCount
    Assert-True ($process.WaitForExit(30000)) 'The benchmark did not exit automatically.'
    Assert-True ($process.ExitCode -eq 0) 'The benchmark exited with an error.'
    Assert-True (Test-Path $internalReport) 'The app did not create its internal benchmark report.'
    $nativeReport = Get-Content $internalReport -Raw | ConvertFrom-Json
    Assert-True ($nativeReport.monitor_scan_count -ge 10) 'The benchmark did not complete at least ten real process scans.'
    Assert-True ($nativeReport.tray_added -eq $true) 'The benchmark did not register a real tray icon.'
    $report = [ordered]@{
        schemaVersion = 1
        measuredAtUtc = [DateTime]::UtcNow.ToString('o')
        executable = (Split-Path $Executable -Leaf)
        executableBytes = (Get-Item $Executable).Length
        sha256 = (Get-FileHash -Algorithm SHA256 $Executable).Hash.ToLowerInvariant()
        environment = if ($env:GITHUB_ACTIONS -eq 'true') { 'GitHub-hosted Windows virtual machine' } else { 'Local Windows machine' }
        os = [Environment]::OSVersion.VersionString
        logicalProcessors = $logicalProcessors
        warmupSeconds = 5
        sampleCount = $samples.Count
        sampleDurationSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
        processCpuSeconds = [Math]::Round($cpuSeconds, 6)
        averageCpuPercentOfOneCore = [Math]::Round($cpuPctOneCore, 6)
        averageCpuPercentOfMachine = [Math]::Round($cpuPctOneCore / $logicalProcessors, 6)
        workingSetBytesMean = [long](($samples | Measure-Object workingSetBytes -Average).Average)
        workingSetBytesMax = [long](($samples | Measure-Object workingSetBytes -Maximum).Maximum)
        privateBytesMean = [long](($samples | Measure-Object privateBytes -Average).Average)
        privateBytesMax = [long](($samples | Measure-Object privateBytes -Maximum).Maximum)
        handlesMax = [int](($samples | Measure-Object handles -Maximum).Maximum)
        threadsMax = [int](($samples | Measure-Object threads -Maximum).Maximum)
        applicationReport = $nativeReport
        limitations = @(
            'This measures hidden-tray idle monitoring with real process discovery on the named machine.',
            'The idle VM sample is taken without a running game; it measures the manager, not a game workload.',
            'This does not establish in-game frame-time improvement or the overhead on a different process workload.',
            'Short samples can round very low CPU use to zero. Zero recorded CPU does not establish zero overhead.'
        )
        samples = $samples
    }
    $report | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $OutputDirectory 'footprint.json') -Encoding utf8
    $exeKiB = [Math]::Round($report.executableBytes / 1KB, 1)
    $privateMiB = [Math]::Round($report.privateBytesMean / 1MB, 2)
    $workingMiB = [Math]::Round($report.workingSetBytesMean / 1MB, 2)
    $markdown = @"
# MaxPrioGames measured footprint

| Measurement | Result |
| --- | ---: |
| Portable executable | $exeKiB KiB |
| Average private memory | $privateMiB MiB |
| Average working set | $workingMiB MiB |
| Average CPU, one core = 100% | $($report.averageCpuPercentOfOneCore)% |
| Average CPU, whole machine = 100% | $($report.averageCpuPercentOfMachine)% |
| Process CPU consumed in sample | $($report.processCpuSeconds) seconds |
| Sampling duration after warmup | $($report.sampleDurationSeconds) seconds |
| Peak handles in sample | $($report.handlesMax) |
| Peak threads in sample | $($report.threadsMax) |

Measured on $($report.environment), $($report.os), with $logicalProcessors logical processors.
The app used actual process discovery and its normal three-second monitoring interval.
The app must complete at least ten real process scans for the measurement to pass.

These results establish the idle application footprint on the named Windows VM without a running game.
They do not establish game performance improvements or overhead for a different process workload.
A zero displayed CPU value is a sampling result, not a zero-overhead claim.

Full measurements, sample data, build SHA-256, and the app's own counters are in footprint.json.
"@
    $markdown | Set-Content (Join-Path $OutputDirectory 'FOOTPRINT.md') -Encoding utf8
    $markdown | Write-Output
}
finally {
    if ($null -ne $process) { Stop-Monitor $Executable $process }
    $env:MAXPRIOGAMES_CONFIG_DIR = $previousConfig
}
