# aNAMorphic-Rig : Windows 빌드 준비물 설치
#
# 설치하는 것
#   - Visual Studio 2022 Build Tools (IDE 없음) 하나로 전부:
#     MSVC 링커, C++ 표준 라이브러리, Windows SDK, clang-cl, CMake, Ninja
#
# 실행 (PowerShell, 프로젝트 루트에서):
#   powershell -ExecutionPolicy Bypass -File .\script\setup.ps1
#
# Build Tools 설치 중 관리자 권한(UAC) 창이 뜹니다. 끝나면 터미널을 새로 여세요.

$ErrorActionPreference = 'Stop'

# Build Tools: 필요한 구성 요소
#   VCTools        : MSVC 빌드 도구 + Windows SDK (권장 구성 요소 포함)
#   VC.Llvm.Clang  : clang-cl
#   VC.CMake.Project : CMake + Ninja (권장 구성 요소에도 있지만 빠지지 않게 명시)
$vsComponents = @(
    'Microsoft.VisualStudio.Workload.VCTools',
    'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
    'Microsoft.VisualStudio.Component.VC.CMake.Project'
)
$vsAddArgs = $vsComponents | ForEach-Object { '--add', $_ }

$vsInstallerDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$vswhere = "$vsInstallerDir\vswhere.exe"

function Find-Vs([string[]]$Requires = @()) {
    if (-not (Test-Path $vswhere)) { return $null }
    $vsArgs = @('-latest', '-products', '*', '-property', 'installationPath')
    if ($Requires) { $vsArgs += @('-requires') + $Requires }
    & $vswhere @vsArgs
}

if (Find-Vs $vsComponents) {
    # Visual Studio(Community 등) 나 Build Tools 에 이미 다 있으면 건드리지 않음
    Write-Host "[skip] Visual Studio C++ 빌드 도구 + clang-cl + CMake : 이미 설치됨"
}
else {
    $btPath = $null
    if (Test-Path $vswhere) {
        $btPath = & $vswhere -latest -products Microsoft.VisualStudio.Product.BuildTools -property installationPath
    }

    if ($btPath) {
        # Build Tools 는 있는데 구성 요소가 빠진 경우: 기존 설치에 구성 요소만 추가
        Write-Host "[modify] Build Tools 에 빠진 구성 요소 추가 ($btPath)"
        $exe = "$vsInstallerDir\setup.exe"
        $argList = @('modify', '--installPath', "`"$btPath`"") + $vsAddArgs +
                @('--includeRecommended', '--passive', '--norestart')
    }
    else {
        # 새로 설치: Microsoft 공식 부트스트래퍼를 받아서 실행
        Write-Host "[install] Visual Studio 2022 Build Tools"
        $exe = Join-Path $env:TEMP 'vs_BuildTools.exe'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile $exe
        $argList = $vsAddArgs + @('--includeRecommended', '--passive', '--wait', '--norestart')
    }

    $p = Start-Process $exe -Verb RunAs -Wait -PassThru -ArgumentList $argList
    # 3010 = 성공, 재부팅 필요
    if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) {
        Write-Error "Build Tools 설치/수정 실패 (exit code $($p.ExitCode))"
    }
}

if (-not (Find-Vs $vsComponents)) {
    Write-Error "설치 후에도 clang-cl 과 CMake 가 포함된 Visual Studio 를 찾지 못했습니다."
}

Write-Host ""
Write-Host "완료. 터미널을 새로 연 뒤 프로젝트 루트에서 실행하세요:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\build.ps1"