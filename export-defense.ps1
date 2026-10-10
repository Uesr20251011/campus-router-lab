param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$defenseRoot = Join-Path $Destination '答辩资料'
$sourceRoot = Join-Path $defenseRoot '源码'
New-Item -ItemType Directory -Path $sourceRoot -Force | Out-Null
foreach ($folder in @('src', 'qml', 'assets', 'tests', 'docs', 'tools')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $folder) -Destination $sourceRoot -Recurse -Force
}
foreach ($file in @('CMakeLists.txt', 'README.md', 'package.ps1', 'export-defense.ps1', 'verify-portable.ps1', 'run.ps1', 'verify-secure.ps1')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $sourceRoot -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/defense/代码说明.md') -Destination $defenseRoot -Force

$html = [Text.StringBuilder]::new()
[void]$html.AppendLine(@'
<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>校园路由实验室 · 答辩导航</title>
<style>body{font:16px/1.7 'Microsoft YaHei',sans-serif;max-width:1100px;margin:40px auto;padding:0 24px;color:#24394e}nav{position:sticky;top:0;background:#fff;padding:12px;border-bottom:1px solid #ddd}a{color:#226a9b;margin-right:18px}section{scroll-margin-top:80px}pre{font:14px/1.7 Consolas,'Microsoft YaHei',monospace;background:#f4f7fb;overflow:auto;padding:20px;border-radius:12px}#guide pre{white-space:pre-wrap;word-break:break-word}.line{display:block}.number{display:inline-block;width:4em;color:#8391a2;user-select:none}h2{margin-top:48px}table{border-collapse:collapse}td,th{border:1px solid #d5deea;padding:8px}</style>
<h1>校园路由实验室 · 答辩导航</h1>
<p>以下代码由本次实际编译的独立算法文件生成。无需安装 Qt 或编辑器即可阅读。完整讲解见下方说明；可在“源码/src/algorithms”目录打开各份原始 .cpp。</p>
<nav><a href="#guide">代码说明</a><a href="#Prim">Prim</a><a href="#Kruskal">Kruskal</a><a href="#Dijkstra">Dijkstra</a><a href="#MaxFlow">最大流</a></nav>
'@)
[void]$html.AppendLine('<section id="guide"><h2>详细代码说明</h2><pre>')
$guideText = [IO.File]::ReadAllText((Join-Path $projectRoot 'docs/defense/代码说明.md'))
[void]$html.AppendLine([Net.WebUtility]::HtmlEncode($guideText))
[void]$html.AppendLine('</pre></section>')
foreach ($algorithm in @('Prim', 'Kruskal', 'Dijkstra', 'MaxFlow')) {
    $relative = "src/algorithms/$algorithm.cpp"
    [void]$html.AppendLine("<section id=`"$algorithm`"><h2>$algorithm</h2><p>源码位置：源码/$relative；文件中的 run$algorithm 是算法入口。</p><pre>")
    $lineNumber = 0
    foreach ($line in [IO.File]::ReadAllLines((Join-Path $projectRoot $relative))) {
        $lineNumber++
        $encoded = [Net.WebUtility]::HtmlEncode($line)
        [void]$html.AppendLine("<span class=`"line`"><span class=`"number`">$lineNumber</span>$encoded</span>")
    }
    [void]$html.AppendLine('</pre></section>')
}
[void]$html.AppendLine('</html>')
[IO.File]::WriteAllText((Join-Path $defenseRoot '答辩导航.html'), $html.ToString(), [Text.UTF8Encoding]::new($false))
@'
校园路由实验室 Windows 免安装包

运行：完整解压后双击 CampusRouter.exe，不需要安装 Qt，也不需要联网。
请保留整个目录：只复制 exe 会缺少运行库和插件。
已验证平台为 Windows 11 64 位；其他 Windows 版本未进行实际兼容性测试。

答辩：双击“答辩资料/答辩导航.html”，浏览详细说明及四个算法的带行号源码。
独立算法原文件：答辩资料/源码/src/algorithms/Prim.cpp、Kruskal.cpp、Dijkstra.cpp、MaxFlow.cpp。
其他算法、数据结构、界面与测试都在“答辩资料/源码”中。
这些算法文件是实际编译版本，重新构建源码需要 Qt/CMake/C++ 开发环境。

免安装验证范围：见“免Qt运行验证说明.txt”。
可用 PowerShell 执行 verify-portable.ps1 重做隔离运行测试，日志输出到临时目录。
'@ | Set-Content -LiteralPath (Join-Path $Destination '运行与答辩说明.txt') -Encoding UTF8
Write-Output "答辩资料已导出：$defenseRoot"
