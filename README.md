# aNAMorphic-Rig

## 1. Build
0. clone this repo, change directory to project root(`..../aNAMorphic-Rig/`)

1. run setup script\
   - Windows : `powershell -ExecutionPolicy Bypass .\script\setup.ps1` (UAC will appear)\
   - macOS : `./script/setup.zsh` (can ask password to access system area)\
   - linux : `sudo ./script/setup.bash` (package manager require root privilege)
   - 
2. close and open new terminal window (must be new process)

3. run build script
   - normal build
     - Windows : `powershell -ExecutionPolicy Bypass .\build.ps1`\
     - macOS : `./build.zsh`\
     - Linux : `./build.bash`
   - clean build (use if normal build fail)
     - Windows : `powershell -ExecutionPolicy Bypass .\build2.ps1`\
     - macOS : `./build2.zsh`\
     - Linux : `./build2.bash`

## 2. Binary Location

| OS | path (from project root) |
|---|---|
| Windows | `build\aNAMorphic-Rig_artefacts\Release\aNAMorphic-Rig.exe` |
| macOS | `build/aNAMorphic-Rig_artefacts/Release/aNAMorphic-Rig.app` |
| Linux | `build/aNAMorphic-Rig_artefacts/Release/aNAMorphic-Rig` |

## 3. Directory Structure

```
~/aNAMorphic-Rig/
├── library/
│   ├── JUCE/juce-X.Y.Z-linux/JUCE      <- OS에 맞는 JUCE 프레임워크 폴더를 CMake가 자동 선택
│   ├── JUCE/juce-X.Y.Z-osx/JUCE
│   ├── JUCE/juce-X.Y.Z-windows/JUCE
│   └── NeuralAmpModelerCore-X.Y.Z      <- Dependencies/eigen 서브모듈까지 받아져 있어야 함
├── script/                             <- 각 플렛폼용 셋업 스크립트
│   ├── setup.bash                          (Windows - ps1, macOS - zsh, Linux - bash)
│   ├── setup.ps1          
│   ├── setup.zsh 
├── source/                             <- 모든 소스
│   ├── Main.cpp                        앱 시작, 메인 창, 오디오 장치 초기화
│   ├── AppPaths.*                      ~/aNAMorphic-Rig 아래 경로 모음
│   ├── engine/                         오디오 처리 (UI 모름)
│   │   ├── Block.h                     블록 공통 인터페이스
│   │   ├── NamBlock.*                  .nam 캡처 (NAM Core)
│   │   ├── IrBlock.*                   .wav IR (juce::dsp::Convolution)
│   │   └── AudioEngine.*               오디오 콜백 + 직렬 체인 처리
│   ├── model/ 
│   │   └── RigModel.*                  체인 편집 상태, 패치 저장/불러오기, 백그라운드 로딩
│   └── ui/                             화면
│       ├── MainComponent.*             전체 배치
│       ├── TopBar.*                    패치 번호 콤보박스 + 설정 버튼
│       ├── SettingsDialog.*            설정 모달 (입력 소스, 입력 채널, 저장/취소)
│       ├── ChainView.*                 직렬 체인 (화면 상단 절반)
│       ├── BlockEditor.*               선택한 블록의 파일 지정 (하단 절반의 위쪽)
│       └── StatusBar.*                 IN 미터 / CPU / OUT 미터
├── build.bash                          <- 각 플렛폼용 빌드 스크립트
├── build.ps1                               (Windows - ps1, macOS - zsh, Linux - bash)
├── build.zsh 
├── CMakeLists.txt

(빌드 완료 시)
└── build/                              <- 빌드 결과물 (git 올리지 말것!!!!)
```