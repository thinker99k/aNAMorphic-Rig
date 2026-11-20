# aNAMorphic-Rig 개발 문서

사용법은 [README](../README.md) 를 보세요. 이 문서는 소스 빌드, 코드 구조, 저장 파일의 세부 동작을 다룹니다.

## 1. Build

0. clone this repo, change directory to project root(`..../aNAMorphic-Rig/`)

1. run setup script
   - Windows : `powershell -ExecutionPolicy Bypass .\script\setup.ps1` (UAC will appear)
   - macOS : `./script/setup.zsh` (can ask password to access system area)
   - linux : `sudo ./script/setup.bash` (package manager require root privilege, apt / dnf only)

2. close and open new terminal window (must be new process)

3. run build script
   - normal build
     - Windows : `powershell -ExecutionPolicy Bypass .\build.ps1`
     - macOS : `./build.zsh`
     - Linux : `./build.bash`
   - clean build (use if normal build fail) : `build/` 안을 비우고 처음부터 다시 빌드
     - Windows : `powershell -ExecutionPolicy Bypass .\build2.ps1`
     - macOS : `./build2.zsh`
     - Linux : `./build2.bash`

## 2. Binary Location

| OS | path (from project root) |
|---|---|
| Windows | `build\aNAMorphic-Rig_artefacts\Release\aNAMorphic-Rig.exe` |
| macOS | `build/aNAMorphic-Rig_artefacts/Release/aNAMorphic-Rig.app` |
| Linux | `build/aNAMorphic-Rig_artefacts/Release/aNAMorphic-Rig` |

## 3. Build Rules

`CMakeLists.txt` 와 빌드 스크립트의 규칙입니다.

- 컴파일러는 clang 으로 통일 (Windows 는 clang-cl). 다른 컴파일러면 CMake 가 멈춥니다
- 빌드 타입을 주지 않으면 Release. NAM 은 Debug 로는 실시간 처리가 안 될 만큼 느립니다
- 프로젝트 루트에 직접 빌드할 수 없습니다. `build/` 같은 별도 폴더를 쓰세요
- 직접 빌드하려면:
  ```
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  cmake --build build
  ```

프로젝트를 Dropbox 폴더에 두는 경우 `script/dropbox-ignore.*` 를 실행하면 `build/`, `cmake-build-debug/`, `cmake-build-release/` 가 동기화에서 빠집니다.

## 4. Directory Structure

```
~/aNAMorphic-Rig/
├── library/
│   ├── JUCE/juce-9.0.4-linux/JUCE      <- OS에 맞는 JUCE 프레임워크 폴더를 CMake가 자동 선택
│   ├── JUCE/juce-9.0.4-osx/JUCE
│   ├── JUCE/juce-9.0.4-windows/JUCE
│   └── NeuralAmpModelerCore-0.6.0      <- Dependencies/eigen, nlohmann 까지 받아져 있어야 함
├── script/                             <- 각 플랫폼용 스크립트 (Windows - ps1, macOS - zsh, Linux - bash)
│   ├── setup.*                             빌드 도구/의존 패키지 설치
│   └── dropbox-ignore.*                    빌드 폴더를 Dropbox 동기화에서 제외
├── source/                             <- 모든 소스
│   ├── Main.cpp                        앱 시작, 메인 창, 오디오 장치 초기화
│   ├── AppPaths.*                      사용자 데이터(~/Documents/aNAMorphic-Rig) 경로 모음
│   ├── Defaults.*                      처음 실행 시 기본 폴더/패치/설정 파일 생성, 깨진 파일 복구
│   ├── engine/                         오디오 처리 (UI 모름)
│   │   ├── Block.h                     블록 공통 인터페이스, 블록 종류 (None / NAM / IR)
│   │   ├── NamBlock.*                  .nam 캡처 (NAM Core)
│   │   ├── IrBlock.*                   .wav IR (juce::dsp::Convolution)
│   │   └── AudioEngine.*               오디오 콜백 + 직렬 체인 처리, 크로스페이드, 입출력 볼륨
│   ├── library/
│   │   └── NamMetadata.*               .nam 파일의 메타데이터(이름, 장비 종류 등)만 읽기
│   ├── model/
│   │   └── RigModel.*                  체인 편집 상태, 패치·씬, 자동 저장, 백그라운드 로딩
│   └── ui/                             화면
│       ├── MainComponent.*             전체 배치, Edit / Live 모드 전환
│       ├── TopBar.*                    패치 콤보박스, 이전/다음, 패치 이름(Rename / Reset), 설정 버튼
│       ├── StatusBar.*                 입출력 볼륨 노브, IN / OUT 미터, CPU, 출력 뮤트
│       ├── GainKnob.*                  볼륨 노브
│       ├── SettingsDialog.*            설정 모달 (입출력 장치, 채널, 저장/취소)
│       ├── Widgets.h                   공용 위젯, 마우스 규칙, 기본 LookAndFeel
│       ├── edit/                       Edit 모드 화면
│       │   ├── EditView.*              Edit 화면 배치
│       │   ├── ChainView.*             패치 맵 (직렬 체인)
│       │   ├── ParamBar.*              선택 블록 파라미터
│       │   ├── BlockEditor.*           블록 선택기
│       │   └── SceneBar.*              씬 버튼, Copy / Swap, All Reset, Live 버튼
│       └── live/                       Live 모드 화면
│           ├── LiveView.*              Live 화면 배치
│           ├── ScenePads.*             씬 패드 4 x 2
│           └── PatchBar.*              Live ON, 이전/다음 패치
├── build.*                             <- 각 플랫폼용 빌드 스크립트 (Windows - ps1, macOS - zsh, Linux - bash)
├── build2.*                            <- 클린 빌드 스크립트
├── CMakeLists.txt

(빌드 완료 시)
└── build/                              <- 빌드 결과물 (git 올리지 말것!!!!)
```

## 5. User Data

앱이 쓰는 파일은 OS별 문서 폴더 아래에 있습니다. 처음 실행할 때 빠진 폴더와 파일은 자동으로 만들어집니다.

```
~/Documents/aNAMorphic-Rig/             (Windows: C:\Users\<user>\Documents\aNAMorphic-Rig)
├── settings.json                       앱 전역 설정: 입력/출력 볼륨 (없거나 깨지면 0 dB 로 생성, 노브 드래그 끝/종료 시 저장)
├── settings.json.bak                   깨진 settings.json 을 다시 만들 때 남기는 백업
├── audio-device.xml                    오디오 장치 설정 (처음 실행 시 기본 장치로 생성, Settings 에서 Save 하거나 종료할 때 저장)
├── nam/                                *.nam 캡처를 여기에 넣으면 블록 선택기 NAM 목록에 표시
├── ir/                                 *.wav IR 을 여기에 넣으면 블록 선택기 IR 목록에 표시
└── patch/
    ├── config01 ~ config32             패치별 체인 (JSON). 없거나 비었거나 깨지면 기본 패치(블록 없음, 씬 8개)로 다시 만듦
    └── configNN.bak                    깨진 파일을 다시 만들 때 남기는 백업
```

출력 뮤트는 저장하지 않습니다 (패치를 바꿔도 유지, 앱을 다시 켜면 해제).

패치 파일 형식

```json
{
  "version": 1,
  "name": "Clean Chorus",
  "scenes": [ { "name": "Scene 1" }, ... 8개 ],
  "blocks": [
    { "type": "nam", "file": "/home/<user>/Documents/aNAMorphic-Rig/nam/amp.nam", "on": [true, true, false, ...8개] },
    { "type": "ir",  "file": "/home/<user>/Documents/aNAMorphic-Rig/ir/cab.wav",  "on": [true, ...8개] }
  ]
}
```

- `type` : `nam` / `ir` / `none`(미지정 블록, 소리 그대로 통과)
- `file` : 절대 경로. 파일이 필요한 종류인데 경로가 비어 있으면 미지정 블록으로 읽음
- `on` : 씬 1~8 에서 이 블록의 On/Off
