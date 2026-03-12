param([string]$TestExe, [string]$Filter = "")
$env:PATH = "D:\Claude\USDCleaner\build\tests\Release;D:\Claude\USDCleaner\build\src\core\Release;D:\USD\lib;D:\USD\bin;D:\USD\python;" + $env:PATH
if ($Filter) {
    & $TestExe --gtest_filter="$Filter" 2>&1
} else {
    & $TestExe 2>&1
}
