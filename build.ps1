# aNAMorphic-Rig : Windows 빌드
#
# 실행 (PowerShell, 프로젝트 루트에서):
#   powershell -ExecutionPolicy Bypass -File .\build.ps1
#
# 일반 PowerShell 창에서 실행해도 된다. clang-cl 은 MSVC 헤더/라이브러리(INCLUDE, LIB)가
# 있어야 동작하므로, 개발자 환경이 아니면 Visual Studio 개발자 셸을 스스로 불러온다.

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not $env:VSINSTALLDIR) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Visual Studio 설치 관리자를 찾지 못했습니다. script\setup.ps1 을 먼저 실행하세요."
    }
    $vs = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
    if (-not $vs) {
        throw "clang-cl 이 포함된 Visual Studio Build Tools 가 없습니다. script\setup.ps1 을 실행하거나 " +
                "Visual Studio Installer -> 수정 -> 'C++ Clang tools for Windows' 를 체크하세요."
    }
    Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
}

# 네이티브 명령은 실패해도 $ErrorActionPreference 로 멈추지 않으므로 종료 코드를 직접 확인
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
    -DCMAKE_C_FLAGS_INIT=-D__RTMINTRIN_H -DCMAKE_CXX_FLAGS_INIT=-D__RTMINTRIN_H `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
