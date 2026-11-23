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

실행 인자 `--spectrum` 을 주면 패치맵 오른쪽 위에 **Spectrum** 버튼이 생깁니다 (출력 주파수 분석 창, 개발·테스트용).
Windows `aNAMorphic-Rig.exe --spectrum`, macOS `open aNAMorphic-Rig.app --args --spectrum`, Linux `./aNAMorphic-Rig --spectrum`.

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
│   ├── ValueSteps.h                    값 단위 (dB 소수 1자리, Hz 정수, Q 소수 2자리): 노브 조작과 저장
│   ├── engine/                         오디오 처리 (UI 모름)
│   │   ├── Block.h                     블록 공통 인터페이스, 블록 종류 (None / NAM / IR), 전환 시간 20 ms
│   │   ├── BlockFactory.*              블록 종류별 로딩 (블록 종류를 늘릴 때 여기에 한 줄)
│   │   ├── NamBlock.*                  .nam 캡처 (NAM Core) + 뒤에 EQ
│   │   ├── IrBlock.*                   .wav IR (juce::dsp::Convolution) + 뒤에 L. Cut / H. Cut
│   │   ├── Merge.h                     병렬 두 갈래 합류 (갈래별 게인)
│   │   ├── AudioEngine.*               오디오 콜백 + 직렬/병렬 체인 처리, 크로스페이드, 입출력 볼륨
│   │   └── param/                      블록 파라미터
│   │       ├── common/                 BlockParams (파라미터 정의 + 지금 씬 값), ParamStage (블록 뒤 처리 틀), Biquad
│   │       ├── nam/                    NamParams (B / M / T, Freq, Middle Q), EqStage (3밴드 peak EQ)
│   │       └── ir/                     IrParams (L. Cut / H. Cut), CutStage (Butterworth 24 dB/oct)
│   ├── library/
│   │   ├── NamMetadata.*               .nam 파일의 메타데이터(이름, 장비 종류 등)만 읽기
│   │   └── LibraryImport.*             외부 NAM / WAV 파일 복사, 중복 이름 처리, 결과 집계
│   ├── model/
│   │   └── RigModel.*                  체인(직렬/병렬 트리) 편집 상태, 패치·씬, 자동 저장, 백그라운드 로딩
│   └── ui/                             화면
│       ├── MainComponent.*             전체 배치, Edit / Live 모드 전환, 외부 파일 드롭과 가져오기 큐
│       ├── TopBar.*                    패치 콤보박스, 이전/다음, Undo / Redo, 패치 이름(Rename / Patch Reset), 설정 버튼
│       ├── StatusBar.*                 입출력 볼륨 노브, IN / OUT 미터, CPU, 출력 뮤트
│       ├── GainKnob.*                  볼륨 노브
│       ├── ParamKnob.*                 블록 파라미터 노브 ([노브][이름 / 값])
│       ├── SettingsDialog.*            설정 모달 (오디오 장치·형식, 저장/취소, Refresh Library)
│       ├── SpectrumWindow.*            출력 주파수 분석 창 (실행 인자 --spectrum 일 때만, 테스트용)
│       ├── Widgets.h                   공용 위젯, 마우스 규칙, 기본 LookAndFeel, 노브 외형, 전원 버튼
│       ├── edit/                       Edit 모드 화면
│       │   ├── EditView.*              Edit 화면 배치
│       │   ├── ChainView.*             패치 맵 (직렬/병렬 체인, 합류 버튼), 왼쪽 위 오류·경고 문구
│       │   ├── ParamBar.*              선택 블록 / 합류 / IN·OUT 파라미터, Param Reset / Level Reset
│       │   ├── ParamPanel.h            블록 종류별 파라미터 패널 틀, 영역 묶음(ParamGroup)
│       │   ├── NamParamPanel.*         NAM 파라미터 패널
│       │   ├── IrParamPanel.*          IR 파라미터 패널
│       │   ├── BlockEditor.*           블록 선택기
│       │   └── SceneBar.*              씬 버튼, Copy / Swap, Scene Reset, Live 버튼
│       └── live/                       Live 모드 화면
│           ├── LiveView.*              Live 화면 배치
│           ├── ScenePads.*             씬 패드 4 x 2
│           └── PatchBar.*              이전/다음 패치, Live
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
├── settings.json                       앱 전역 설정: 입력/출력 볼륨 dB, 소수 1자리 (없거나 깨지면 0 dB 로 생성, 노브 드래그 끝/종료 시 저장)
├── settings.json.bak                   깨진 settings.json 을 다시 만들 때 남기는 백업
├── audio-device.xml                    오디오 장치 설정 (처음 실행 시 기본 장치로 생성, Settings 에서 Save 하거나 종료할 때 저장). 채널은 항상 전부 열고, 쓸 채널은 패치가 정함
├── nam/                                *.nam 캡처를 여기에 넣으면 블록 선택기 NAM 목록에 표시
├── ir/                                 *.wav IR 을 여기에 넣으면 블록 선택기 IR 목록에 표시
└── patch/
    ├── config01 ~ config32             패치별 체인 (JSON). 없거나 비었거나 깨지면 기본 패치(블록 없음, 씬 8개)로 다시 만듦
    └── configNN.bak                    깨진 파일을 다시 만들 때 남기는 백업
```

출력 뮤트는 저장하지 않습니다 (패치를 바꿔도 유지, 앱을 다시 켜면 해제).

라이브러리 가져오기와 갱신

- `MainComponent`가 OS 파일 드롭을 받습니다. 제목표시줄은 부모 `DocumentWindow`이므로 제외되며, Edit/Live의 모든 자식 영역은 메인 컴포넌트로 전달됩니다. 앱 내부의 블록 드래그와는 별도 경로입니다.
- `.nam` / `.wav`는 대소문자 구분 없이 받아 `nam/` / `ir/`에 복사하고, 저장 확장자는 소문자로 통일합니다. 폴더와 다른 확장자는 건너뜁니다. 이미 올바른 라이브러리 하위에 있는 파일은 복사하지 않습니다.
- `LibraryImport`는 UI·모델·오디오를 참조하지 않습니다. 단일 `ThreadPool`에서 드롭 순서대로 복사하고, 이름 충돌 시 `name (2).nam`부터 번호를 붙입니다. 배타적 파일 생성으로 기존 파일을 덮어쓰지 않으며, 실패·취소 시 이번 복사에서 만든 불완전한 파일을 정리합니다.
- 종료 시 대기 작업을 취소하고 진행 중 작업은 버퍼 단위로 중단합니다. 완료 콜백은 메시지 스레드에서 `SafePointer`를 확인한 뒤 드롭당 한 번 `EditView::refreshLibrary()`를 호출하고 제외·실패 내역을 알립니다.
- `SettingsDialog::show()`의 `onRefreshLibrary` 콜백도 같은 `EditView → BlockEditor::refresh()` 경로를 사용합니다. NAM / IR 하위 폴더를 재귀 스캔하는 기존 구현을 사용하며 검색어와 표별 정렬은 유지합니다. Refresh는 오디오 설정 Save/Cancel과 독립적입니다.
- 블록 선택기의 `Browse`는 `paths::root().startAsProcess()`로 루트를 엽니다. IN/OUT 선택 중에도 활성입니다. 파일 이동 추적이나 패치 경로 마이그레이션은 하지 않으므로, 사용 중인 파일을 옮겼다면 블록에 새 경로를 다시 지정해야 합니다.

패치 파일 형식

```json
{
  "version": 1,
  "name": "Clean Chorus",
  "scenes": [ { "name": "Scene 1" }, ... 8개 ],
  "io": { "in": 0, "out": [0, 1] },
  "blocks": [
    { "type": "nam", "file": "/home/<user>/Documents/aNAMorphic-Rig/nam/drive.nam", "on": [true, true, false, ...8개],
      "params": { "bassDb": [0.0, ...8개], "bassHz": [100, ...8개], ... } },
    { "type": "parallel",
      "branches": [
        { "on": [true, ...8개], "level": [-3.0, ...8개],
          "blocks": [ { "type": "nam", "file": ".../nam/clean.nam", "on": [true, ...8개] } ] },
        { "on": [true, false, ...8개], "level": [-3.0, ...8개],
          "blocks": [ { "type": "nam", "file": ".../nam/lead.nam", "on": [true, ...8개] } ] }
      ] },
    { "type": "ir",  "file": "/home/<user>/Documents/aNAMorphic-Rig/ir/cab.wav",  "on": [true, ...8개],
      "params": { "lowCutOn": [false, ...8개], "lowCutHz": [100, ...8개], "highCutOn": [false, ...8개], "highCutHz": [5000, ...8개] } }
  ]
}
```

- `blocks` : IN 에서 OUT 까지의 본 줄. 블록 또는 병렬
- `type` : `nam` / `ir` / `none`(미지정 블록, 소리 그대로 통과) / `parallel`(병렬)
- `file` : 절대 경로. 파일이 필요한 종류인데 경로가 비어 있으면 미지정 블록으로 읽음
- `on` : 씬 1~8 에서 이 블록의 On/Off (Off = 바이패스)
- `params` : NAM / IR 블록의 파라미터. 이름마다 씬 1~8 의 값. 스위치는 true / false, dB 는 소수 1자리, Hz 는 정수, Q 는 소수 2자리.
  없거나 이상한 값은 기본값, 범위 밖은 범위 안으로
  - NAM (블록 뒤 3밴드 EQ, 세 밴드 모두 peak, 항상 켜짐)

    | 이름 | 범위 | 기본 |
    |---|---|---|
    | `bassDb` / `middleDb` / `trebleDb` | -18 ~ 18 dB | 0 |
    | `bassHz` | 25 ~ 400 Hz | 100 |
    | `middleHz` | 400 ~ 2500 Hz | 1000 |
    | `middleQ` | 0.2 ~ 3.2 (Middle 폭, Bass / Treble 은 0.8 고정) | 0.8 |
    | `trebleHz` | 2500 ~ 10000 Hz | 5000 |

  - IR (블록 뒤 L. Cut / H. Cut, 24 dB/oct)

    | 이름 | 범위 | 기본 |
    |---|---|---|
    | `lowCutOn` / `highCutOn` | true / false | false |
    | `lowCutHz` | 25 ~ 400 Hz | 100 |
    | `highCutHz` | 1250 ~ 20000 Hz | 5000 |

- `branches` : 병렬의 위 / 아래 두 갈래. 갈래 안에는 블록만 (병렬은 한 단계). 두 갈래는 합류에서 다시 합쳐짐
  - `on` : 씬 1~8 에서 이 갈래의 On/Off (Off = 그 갈래 무음)
  - `level` : 씬 1~8 에서 이 갈래의 레벨 dB, 소수 1자리 (-60 = -inf ~ 0, 기본 -3)
  - 갈래가 두 개가 아니면 병렬을 버리고, 블록이 없는 갈래가 있으면 남은 갈래의 블록만 본 줄에 둠
- `io` : 장치 채널 번호(0부터). `in` 은 입력 채널 하나, `out` 은 스테레오 쌍 `[짝수, 짝수+1]` 또는 단일 `[n]`. 이상한 값이면 기본값(`in` 0, `out` [0, 1]). 지금 장치에 없는 번호는 그대로 두고 소리는 기본 채널로 내며, 패치 맵의 IN / OUT 노드에 빨간 빗금
