$qtBin = 'E:\Tools_E\QtOrginal\6.11.2\mingw_64\bin'
$mingwBin = 'E:\Tools_E\QtOrginal\Tools\mingw1310_64\bin'
$env:PATH = "$qtBin;$mingwBin;$env:PATH"
& (Join-Path $PSScriptRoot 'build-ascii\campus_router.exe')
