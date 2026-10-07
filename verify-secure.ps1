param([switch]$PortableOnly, [string]$PackageName = 'CampusRouter-Crypto-Flow-v2-Windows')
$ErrorActionPreference = 'Stop'
if ($PortableOnly) {
    $projectDir = 'Q:\campus-router-ui'
    $portableDir = Join-Path $projectDir "dist\$PackageName"
    $appFile = Join-Path $portableDir 'CampusRouter.exe'
    $builtFile = Join-Path $projectDir 'build-ascii\campus_router.exe'
    if ((Get-FileHash -LiteralPath $appFile).Hash -ne (Get-FileHash -LiteralPath $builtFile).Hash) { throw 'Packaged executable differs from the verified build' }
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = ''
    $env:QML2_IMPORT_PATH = ''
    $env:QML_IMPORT_PATH = ''
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_BACKEND = 'software'
    $env:QT_QUICK_CONTROLS_STYLE = 'Basic'
    $env:QT_QPA_FONTDIR = 'C:\Windows\Fonts'
    $png = Join-Path $projectDir 'docs\secure-visual\portable.png'
    $err = Join-Path $projectDir 'build-ascii\portable-visual.err'
    $appProcess = Start-Process -FilePath $appFile -ArgumentList @('--screenshot',$png,'--screenshot-state','secure-flow-counter','--screenshot-width','1280','--screenshot-height','800') -WorkingDirectory $portableDir -WindowStyle Hidden -Wait -PassThru -RedirectStandardError $err
    Get-Content -LiteralPath $err
    if ($appProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $png)) { throw 'Portable application failed' }
    if ((Get-Item -LiteralPath $png).Length -lt 10000) { throw 'Portable screenshot is empty' }
    Write-Output "Portable verification passed: $png"
    exit 0
}
$qtBin = 'E:\Tools_E\QtOrginal\6.11.2\mingw_64\bin'
$mingwBin = 'E:\Tools_E\QtOrginal\Tools\mingw1310_64\bin'
$env:PATH = "$qtBin;$mingwBin;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QUICK_BACKEND = 'software'
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
$env:QT_QPA_FONTDIR = 'C:\Windows\Fonts'
$projectDir = 'Q:\campus-router-ui'
$buildDir = Join-Path $projectDir 'build-ascii'
$previewDir = Join-Path $projectDir 'docs\secure-visual'
New-Item -ItemType Directory -Path $previewDir -Force | Out-Null
& 'E:\Tools_E\QtOrginal\Tools\CMake_64\bin\cmake.exe' --build $buildDir -j 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
foreach ($name in @('router_tests', 'secure_tests', 'canvas_tests', 'layout_tests')) {
    $log = Join-Path $buildDir "$name-visual.txt"
    $err = Join-Path $buildDir "$name-visual.err"
    $testProcess = Start-Process -FilePath (Join-Path $buildDir "$name.exe") -ArgumentList @('-o',"$log,txt") -WorkingDirectory $buildDir -WindowStyle Hidden -Wait -PassThru -RedirectStandardError $err
    Get-Content -LiteralPath $log | Select-String 'FAIL|Totals:|TypeError|ReferenceError|Unable to assign|Binding loop'
    Get-Content -LiteralPath $err
    if ($testProcess.ExitCode -ne 0) { throw "$name failed" }
}
foreach ($case in @(
    @{Name='input'; State='secure-input'; Width=1080; Height=680},
    @{Name='encrypted'; State='secure'; Width=1440; Height=900},
    @{Name='intercepted'; State='secure-intercept'; Width=1440; Height=900},
    @{Name='tampered'; State='secure-tamper'; Width=1440; Height=900},
    @{Name='plaintext'; State='secure-plain'; Width=1440; Height=900},
    @{Name='compact'; State='secure-intercept'; Width=880; Height=560},
    @{Name='default'; State='secure-transit'; Width=1080; Height=680},
    @{Name='flow'; State='secure-flow'; Width=1440; Height=900},
    @{Name='aes-round'; State='secure-aes'; Width=1440; Height=900},
    @{Name='flow-failed'; State='secure-flow-fail'; Width=1440; Height=900},
    @{Name='flow-compact'; State='secure-flow'; Width=880; Height=560},
    @{Name='flow-raw'; State='secure-flow-raw'; Width=1440; Height=900},
    @{Name='flow-hash'; State='secure-flow-hash'; Width=1440; Height=900},
    @{Name='flow-raw-compact'; State='secure-flow-raw'; Width=880; Height=560},
    @{Name='flow-counter'; State='secure-flow-counter'; Width=1440; Height=900},
    @{Name='flow-counter-compact'; State='secure-flow-counter'; Width=880; Height=560}
)) {
    $png = Join-Path $previewDir ($case.Name + '.png')
    $err = Join-Path $buildDir ($case.Name + '-visual.err')
    $appProcess = Start-Process -FilePath (Join-Path $buildDir 'campus_router.exe') -ArgumentList @('--screenshot',$png,'--screenshot-state',$case.State,'--screenshot-width',$case.Width,'--screenshot-height',$case.Height) -WorkingDirectory $buildDir -WindowStyle Hidden -Wait -PassThru -RedirectStandardError $err
    Get-Content -LiteralPath $err
    if ($appProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $png)) { throw "Preview failed: $($case.Name)" }
    Write-Output "Preview: $png"
}
