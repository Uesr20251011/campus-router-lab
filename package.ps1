param([ValidatePattern('^[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*$')][string]$PackageName = 'CampusRouter-Windows')
$ErrorActionPreference = 'Stop'

$qtBin = 'E:\Tools_E\QtOrginal\6.11.2\mingw_64\bin'
$mingwBin = 'E:\Tools_E\QtOrginal\Tools\mingw1310_64\bin'
$cmake = 'E:\Tools_E\QtOrginal\Tools\CMake_64\bin\cmake.exe'
$project = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$workspace = Split-Path $project -Parent
$virtualProject = 'Q:\campus-router-ui'

if (-not (Test-Path -LiteralPath $virtualProject)) {
    & subst Q: $workspace
}
if ((Get-FileHash -LiteralPath "$virtualProject\CMakeLists.txt").Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $project 'CMakeLists.txt')).Hash) {
    throw 'Q: 必须指向本项目所在的大作业目录。'
}

$env:PATH = "$qtBin;$mingwBin;$env:PATH"
& $cmake --build "$virtualProject\build-ascii" --target campus_router -j 4
if ($LASTEXITCODE -ne 0) { throw '构建失败。' }

$dist = Join-Path $project 'dist'
$portable = Join-Path $dist $PackageName
$module = Join-Path $portable 'CampusRouter'
$moduleQml = Join-Path $module 'qml'
New-Item -ItemType Directory -Path $moduleQml -Force | Out-Null
Copy-Item -LiteralPath "$virtualProject\build-ascii\campus_router.exe" -Destination (Join-Path $portable 'CampusRouter.exe') -Force

$deployTool = Join-Path $qtBin 'windeployqt.exe'
& $deployTool --release --compiler-runtime --no-translations --qmldir "$virtualProject\qml" --dir $portable (Join-Path $portable 'CampusRouter.exe') *> (Join-Path $dist 'package.log')
if ($LASTEXITCODE -ne 0) { throw "Qt 部署失败，请查看 $dist\package.log。" }

Copy-Item -LiteralPath "$virtualProject\build-ascii\CampusRouter\qmldir" -Destination (Join-Path $module 'qmldir') -Force
Get-ChildItem -LiteralPath "$virtualProject\build-ascii\CampusRouter\qml" -File |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $moduleQml $_.Name) -Force }
Copy-Item -LiteralPath (Join-Path $qtBin '..\plugins\platforms\qoffscreen.dll') -Destination (Join-Path $portable 'platforms\qoffscreen.dll') -Force

$archive = Join-Path $dist "$PackageName.zip"
Compress-Archive -LiteralPath $portable -DestinationPath $archive -CompressionLevel Optimal -Force
Write-Output "可双击运行：$portable\CampusRouter.exe"
Write-Output "分享压缩包：$archive"
