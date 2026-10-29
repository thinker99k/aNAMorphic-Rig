# aNAMorphic-Rig : Windows 빌드 결과를 지우고 처음부터 다시 빌드
#
# build\ 안의 내용(숨김/읽기 전용 파일 포함)을 모두 지운다. build\ 폴더 자체는 남긴다.
# 그 다음 build.ps1 을 실행한다.
#
# 실행 (PowerShell, 프로젝트 루트에서):
#   powershell -ExecutionPolicy Bypass -File .\build2.ps1

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (Test-Path -LiteralPath build -PathType Container) {
    Get-ChildItem -LiteralPath build -Force | Remove-Item -Recurse -Force
    Write-Host "build\ 안의 내용을 모두 지웠습니다."
}

& "$PSScriptRoot\build.ps1"
exit $LASTEXITCODE
