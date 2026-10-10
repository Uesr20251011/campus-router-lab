param([string]$PackagePath = $PSScriptRoot, [string]$ReportDirectory = '')
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $PackagePath).Path
$appPath = Join-Path $packageRoot 'CampusRouter.exe'
if (-not (Test-Path -LiteralPath $appPath)) { throw 'PackagePath must contain CampusRouter.exe' }
if (-not $ReportDirectory) {
    $ReportDirectory = Join-Path $env:TEMP ('CampusRouter-portable-' + [guid]::NewGuid().ToString('N'))
}
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$reportRoot = (Resolve-Path -LiteralPath $ReportDirectory).Path
$envNames = @('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML2_IMPORT_PATH','QML_IMPORT_PATH',
    'QT_QUICK_CONTROLS_CONF','QT_QPA_PLATFORM','QT_QUICK_BACKEND','QT_DEBUG_PLUGINS','QML_DISABLE_DISK_CACHE',
    'APPDATA','LOCALAPPDATA')
$savedEnvironment = @{}
foreach ($name in $envNames) { $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$allModules = @{}
$externalModules = @{}
$packageDllNames = @{}
Get-ChildItem -LiteralPath $packageRoot -Recurse -File -Filter '*.dll' | ForEach-Object {
    $packageDllNames[$_.Name.ToLowerInvariant()] = $true
}
$results = @()
try {
    # 只保留 Windows 路径；清除可能指向本机 Qt 安装的插件和 QML 设置。
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    foreach ($name in $envNames | Where-Object { $_ -notin @('PATH','APPDATA','LOCALAPPDATA') }) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $env:APPDATA = Join-Path $reportRoot 'profile/roaming'
    $env:LOCALAPPDATA = Join-Path $reportRoot 'profile/local'
    New-Item -ItemType Directory -Path $env:APPDATA,$env:LOCALAPPDATA -Force | Out-Null
    $env:QT_DEBUG_PLUGINS = '1'
    $env:QML_DISABLE_DISK_CACHE = '1'
    foreach ($state in @('cost-weights','tree','dijkstra-finish','flow-finish','secure-flow-counter','secure-flow-fail')) {
        $png = Join-Path $reportRoot "$state.png"
        $err = Join-Path $reportRoot "$state.log"
        # 不允许重复验证时借用旧截图：删除失败即停止，进程必须重新生成本次图片。
        if (Test-Path -LiteralPath $png) { Remove-Item -LiteralPath $png -Force }
        # 原生 Windows 平台启动；截图参数使程序加载对应功能并在完成后自行退出。
        $arguments = @('--screenshot', ('"' + $png + '"'), '--screenshot-state', $state,
            '--screenshot-width','1280','--screenshot-height','800')
        $process = Start-Process -FilePath $appPath -ArgumentList $arguments -WorkingDirectory $packageRoot `
            -WindowStyle Hidden -PassThru -RedirectStandardError $err
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        $sampledQt = $false
        try {
            while (-not $process.HasExited) {
                if ([DateTime]::UtcNow -gt $deadline) { throw "Timeout: $state" }
                $process.Refresh()
                try {
                    $modules = @($process.Modules)
                } catch {
                    # 进程恰好退出时模块枚举可能失败，退出后仍检查已采样的记录。
                    if (-not $process.HasExited) { throw }
                    break
                }
                foreach ($module in $modules) {
                    $name = $module.ModuleName
                    $path = $module.FileName
                    # 启动/退出瞬间 .NET 可能返回空模块或已失效的模块信息。
                    # 空记录不作通过证据；仍要求采样到 Qt6Quick 并验证本次截图。
                    if (-not $name -or -not $path) { continue }
                    # 包内 DLL 名称涵盖 Qt 动态插件；这些项目模块不能从包外借用。
                    # 声效/安全软件可能向进程注入其他模块，记录来源而不混称项目依赖。
                    $fromPackage = $path.StartsWith($packageRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
                    $fromWindows = $path.StartsWith($env:SystemRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
                    if ($packageDllNames.ContainsKey($name.ToLowerInvariant()) -or
                        $name -match '^Qt6.*\.dll$|^lib(gcc|stdc\+\+|winpthread).*\.dll$|^qwindows\.dll$') {
                        if (-not $fromPackage) {
                            throw "External Qt/compiler runtime loaded: $path"
                        }
                        if ($name -eq 'Qt6Quick.dll') { $sampledQt = $true }
                    }
                    if (-not $fromPackage -and -not $fromWindows) {
                        $externalModules[$path] = @{Name=$name; Path=$path}
                    }
                    $allModules[$path] = @{Name=$name; Path=$path}
                }
                Start-Sleep -Milliseconds 80
            }
            $process.WaitForExit()
            if ($process.ExitCode -ne 0 -or -not $sampledQt -or -not (Test-Path -LiteralPath $png) `
                -or (Get-Item -LiteralPath $png).Length -lt 10000) { throw "Launch/render/module check failed: $state" }
            $errors = Get-Content -LiteralPath $err -Raw
            if ($errors -match 'QQmlApplicationEngine failed|TypeError|ReferenceError|Unable to assign|Binding loop|could not load.*platform plugin') {
                throw "QML/platform error in $state; see $err"
            }
            $results += @{State=$state; ExitCode=$process.ExitCode; Screenshot=$png; Passed=$true}
            Write-Output "PASS: $state (all observed Qt/compiler DLLs came from the package)"
        } finally {
            if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
            $process.Dispose()
        }
    }
    $report = @{Package=$packageRoot; TestedAt=[DateTimeOffset]::Now.ToString('o');
        Scope='Isolated PATH and Qt/QML settings plus native startup and observed module origins; not a clean VM.';
        Results=$results; Modules=@($allModules.Values | Sort-Object Path);
        ExternalHostModules=@($externalModules.Values | Sort-Object Path)}
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reportRoot 'runtime-report.json') -Encoding UTF8
    Write-Output "Report: $reportRoot"
} finally {
    foreach ($name in $envNames) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
}
