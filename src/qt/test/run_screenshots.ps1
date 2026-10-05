# Copyright (c) 2026 Stanislav Saveliev
# SPDX-License-Identifier: Apache-2.0
#
# CYBOU desktop screenshot QA matrix (fixtures x widths x Windows scaling).
# Usage: pwsh src/qt/test/run_screenshots.ps1 -Exe build\bin\cybou.exe -Out shots
#        [-Scales 1,1.25,1.5] [-Appearance dark]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [string]$Out = 'cybou-screenshots',
    [double[]]$Scales = @(1.0),
    [string[]]$Fixtures = @('empty', 'restoring', 'active', 'offline'),
    [string[]]$Sizes = @('1040x720', '1280x860', '1600x900', '1920x1080'),
    [ValidateSet('light', 'dark')][string]$Appearance = 'light'
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Out | Out-Null
$data = Join-Path ([IO.Path]::GetTempPath()) 'cybou-screenshot-data'
New-Item -ItemType Directory -Force $data | Out-Null
foreach ($scale in $Scales) {
    foreach ($size in $Sizes) {
        $w, $h = $size.Split('x')
        foreach ($fixture in $Fixtures) {
            $env:CYBOU_UI_FIXTURE = $fixture
            $env:CYBOU_SCREENSHOT_DIR = (Resolve-Path $Out).Path
            $env:CYBOU_SCREENSHOT_WIDTH = $w
            $env:CYBOU_SCREENSHOT_HEIGHT = $h
            $env:CYBOU_APPEARANCE = $Appearance
            $env:CYBOU_SCREENSHOT_PREFIX = "{0}-{1}pct-{2}-" -f $size, [int]($scale * 100), $Appearance
            $env:QT_SCALE_FACTOR = "$scale"
            $p = Start-Process -FilePath $Exe -ArgumentList "--datadir=$data" -PassThru -Wait
            if ($p.ExitCode -ne 0) { throw "cybou exited with $($p.ExitCode) for $fixture $size x$scale" }
        }
    }
}
Remove-Item Env:CYBOU_UI_FIXTURE, Env:CYBOU_APPEARANCE, Env:CYBOU_SCREENSHOT_DIR, Env:CYBOU_SCREENSHOT_PREFIX, Env:QT_SCALE_FACTOR -ErrorAction SilentlyContinue
Get-ChildItem $Out -Filter *.png | Measure-Object | ForEach-Object { "{0} screenshots in {1}" -f $_.Count, $Out }
