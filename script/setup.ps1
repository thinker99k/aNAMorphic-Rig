# aNAMorphic-Rig : Windows 빌드 준비물 설치
#
# 설치하는 것
#   - Visual Studio 2022 Build Tools (IDE 없음): MSVC 링커, C++ 표준 라이브러리, Windows SDK, clang-cl
#   - CMake, Ninja
#
# 실행 (PowerShell, 프로젝트 루트에서):
#   powershell -ExecutionPolicy Bypass -File .\script\setup.ps1
#
# Build Tools 설치 중 관리자 권한(UAC) 창이 뜹니다. 끝나면 터미널을 새로 여세요.

$ErrorActionPreference = 'Stop'

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Error "winget 이 없습니다. Microsoft Store 에서 '앱 설치 관리자(App Installer)'를 설치/업데이트한 뒤 다시 실행하세요."
}

function Test-WingetInstalled([string]$Id) {
    winget list --id $Id --exact --accept-source-agreements *> $null
    return $LASTEXITCODE -eq 0
}

function Install-WingetPackage([string]$Id, [string[]]$Extra = @()) {
    if (Test-WingetInstalled $Id) {
        Write-Host "[skip] $Id : 이미 설치됨"
        return
    }
    Write-Host "[install] $Id"
    winget install --id $Id --exact --accept-source-agreements --accept-package-agreements @Extra
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$Id 설치 실패 (exit code $LASTEXITCODE)"
    }
}

# Build Tools: 필요한 구성 요소 (C++ 빌드 도구 + Windows SDK + clang-cl)
$vsComponents = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
    'Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset'
)
$vsAddArgs = $vsComponents | ForEach-Object { '--add', $_ }

$vsInstallerDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vswhere = "$vsInstallerDir\vswhere.exe"

function Find-VsWithClang {
    if (-not (Test-Path $vswhere)) { return $null }
    & $vswhere -latest -products * -requires $vsComponents -property installationPath
}

if (Find-VsWithClang) {
    # Visual Studio(Community 등) 나 Build Tools 에 이미 다 있으면 건드리지 않음
    Write-Host "[skip] Visual Studio C++ 빌드 도구 + clang-cl : 이미 설치됨"
}
elseif (Test-WingetInstalled 'Microsoft.VisualStudio.2022.BuildTools') {
    # Build Tools 는 있는데 clang 구성 요소가 빠진 경우: 기존 설치에 구성 요소만 추가
    $btPath = & $vswhere -latest -products Microsoft.VisualStudio.Product.BuildTools -property installationPath
    Write-Host "[modify] Build Tools 에 C++ 빌드 도구 / clang-cl 추가 ($btPath)"
    $p = Start-Process "$vsInstallerDir\setup.exe" -Verb RunAs -Wait -PassThru -ArgumentList (
        @('modify', '--installPath', "`"$btPath`"") + $vsAddArgs +
        @('--includeRecommended', '--passive', '--norestart'))
    # 3010 = 성공, 재부팅 필요
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) {
        Write-Error "Build Tools 수정 실패 (exit code $($p.ExitCode))"
    }
}
else {
    $override = (@('--passive', '--wait') + $vsAddArgs + @('--includeRecommended')) -join ' '
    Install-WingetPackage 'Microsoft.VisualStudio.2022.BuildTools' @('--override', $override)
}

if (-not (Find-VsWithClang)) {
    Write-Error "설치 후에도 clang-cl 이 포함된 Visual Studio 를 찾지 못했습니다."
}

Install-WingetPackage 'Kitware.CMake'
Install-WingetPackage 'Ninja-build.Ninja'

Write-Host ""
Write-Host "완료. 터미널을 새로 연 뒤 프로젝트 루트에서 실행하세요:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\build.ps1"