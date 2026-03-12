$env:PATH = "D:\Claude\USDCleaner\build\tests\Release;D:\Claude\USDCleaner\build\src\core\Release;D:\USD\lib;D:\USD\bin;D:\USD\python;" + $env:PATH
$allPassed = $true
Get-ChildItem "D:\Claude\USDCleaner\build\tests\Release\*.exe" | ForEach-Object {
    Write-Host "=== Running $($_.Name) ===" -ForegroundColor Cyan
    & $_.FullName 2>&1 | Out-String | Write-Host
    if ($LASTEXITCODE -ne 0) { $allPassed = $false }
    Write-Host ""
}
if ($allPassed) { Write-Host "ALL TESTS PASSED" -ForegroundColor Green; exit 0 }
else { Write-Host "SOME TESTS FAILED" -ForegroundColor Red; exit 1 }
