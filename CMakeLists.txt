# aNAMorphic-Rig
#
# 빌드는 프로젝트 루트의 build 스크립트로 (build.ps1 / build.zsh / build.bash). 직접 하려면:
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++    (Windows: clang-cl)
#   cmake --build build
#
# 결과물: build/aNAMorphic-Rig/Release/aNAMorphic-Rig(.exe / .app)

cmake_minimum_required(VERSION 3.22)

# 컴파일러를 따로 지정하지 않으면 clang 사용 (CLion 기본 설정 대비)
if(NOT CMAKE_CXX_COMPILER)
    if(CMAKE_HOST_WIN32)
        set(CMAKE_C_COMPILER clang-cl)
        set(CMAKE_CXX_COMPILER clang-cl)
    else()
        set(CMAKE_C_COMPILER clang)
        set(CMAKE_CXX_COMPILER clang++)
    endif()
endif()

project(aNAMorphicRig VERSION 0.1.0 LANGUAGES C CXX)

project(aNAMorphic-Rig VERSION 0.1.0 LANGUAGES C CXX)

# 빌드 타입을 안 주면 Release. NAM은 Debug로 돌리면 실시간 처리가 안 될 만큼 느림.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


set(CMAKE_CXX_STANDARD 20)              # NAM Core가 C++20을 요구
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clangd / IDE 자동완성용

# 팀 규칙: 컴파일러는 clang으로 통일 (Windows는 clang-cl도 Clang으로 잡힘)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
        "clang으로 빌드해야 합니다. 현재 컴파일러: ${CMAKE_CXX_COMPILER_ID}\n"
        "예) -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++  (Windows: clang-cl)")
endif()

# ---------------------------------------------------------------------------
# 라이브러리 경로 (OS별 JUCE 폴더 자동 선택)
# ---------------------------------------------------------------------------
set(AR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# 루트에서 바로 cmake 를 돌리면 루트가 빌드 산출물로 어질러지므로 막는다
if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR
        "프로젝트 루트에 직접 빌드하지 마세요. build/ 폴더를 쓰세요:\n"
        "  cmake -S . -B build ...\n"
        "(이미 생긴 CMakeCache.txt 와 CMakeFiles/ 는 지우고 다시 실행)")
endif()

if(WIN32)
    set(AR_BUILD_OS "windows")
elseif(APPLE)
    set(AR_BUILD_OS "osx")
else()
    set(AR_BUILD_OS "linux")
endif()

set(AR_JUCE_VER "9.0.4")
set(AR_NAMCORE_VER "0.6.0")

set(AR_JUCE_DIR "${AR_ROOT}/library/JUCE/juce-${AR_JUCE_VER}-${AR_BUILD_OS}/JUCE")
set(AR_NAMCORE_DIR "${AR_ROOT}/library/NeuralAmpModelerCore-${AR_NAMCORE_VER}")

foreach(_req "${AR_JUCE_DIR}/CMakeLists.txt"
             "${AR_NAMCORE_DIR}/NAM/dsp.h"
             "${AR_NAMCORE_DIR}/Dependencies/eigen/Eigen/Core"
             "${AR_NAMCORE_DIR}/Dependencies/nlohmann/json.hpp")
    if(NOT EXISTS "${_req}")
        message(FATAL_ERROR "필요한 파일이 없습니다: ${_req}\n"
                            "library/ 폴더 구성과 git 서브모듈(eigen 등)을 확인하세요.")
    endif()
endforeach()

message(STATUS "JUCE : ${AR_JUCE_DIR}")
message(STATUS "NAM  : ${AR_NAMCORE_DIR}")

add_subdirectory("${AR_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

# ---------------------------------------------------------------------------
# namcore : NAM Core에는 라이브러리 타깃이 없어서 직접 묶음
#
# 주의: STATIC 이 아니라 OBJECT 라이브러리여야 한다.
# NAM은 WaveNet/LSTM 등 아키텍처를 전역 static 객체로 "자가 등록"하는데,
# 정적 라이브러리(.a/.lib)로 만들면 링커가 아무도 참조하지 않는 .o 를 버려서
# 실행 시 "No config parser registered for architecture: WaveNet" 오류가 난다.
# ---------------------------------------------------------------------------
file(GLOB NAMCORE_SOURCES CONFIGURE_DEPENDS
    "${AR_NAMCORE_DIR}/NAM/*.cpp"
    "${AR_NAMCORE_DIR}/NAM/*/*.cpp")

add_library(namcore OBJECT ${NAMCORE_SOURCES})

# SYSTEM: 외부 코드의 경고가 우리 빌드 로그를 덮지 않게
target_include_directories(namcore SYSTEM PUBLIC
    "${AR_NAMCORE_DIR}"
    "${AR_NAMCORE_DIR}/Dependencies/eigen"
    "${AR_NAMCORE_DIR}/Dependencies/nlohmann")

target_compile_definitions(namcore
    PUBLIC
        NAM_SAMPLE_FLOAT        # NAM 샘플 타입을 float로 (기본은 double). 앱과 반드시 동일해야 함
        NAM_ENABLE_A2_FAST      # A2 계열 WaveNet 고속 경로
    PRIVATE
        $<$<BOOL:${WIN32}>:NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES>)

set_target_properties(namcore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# 앱
# ---------------------------------------------------------------------------
juce_add_gui_app(aNAMorphic-Rig
    PRODUCT_NAME "aNAMorphic-Rig"
    COMPANY_NAME "thinker99k"
    BUNDLE_ID "com.thinker99k.anamorphic-rig"
    MICROPHONE_PERMISSION_ENABLED TRUE   # macOS: 없으면 입력이 무음
    MICROPHONE_PERMISSION_TEXT "Guitar input is needed to process audio."
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(aNAMorphic-Rig PRIVATE
        source/Main.cpp
        source/AppPaths.cpp
        source/engine/AudioEngine.cpp
        source/engine/NamBlock.cpp
        source/engine/IrBlock.cpp
        source/model/RigModel.cpp
        source/ui/MainComponent.cpp
        source/ui/TopBar.cpp
        source/ui/SettingsDialog.cpp
        source/ui/ChainView.cpp
        source/ui/BlockEditor.cpp
        source/ui/StatusBar.cpp)

target_include_directories(aNAMorphic-Rig PRIVATE source)

target_compile_definitions(aNAMorphic-Rig PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:aNAMorphic-Rig,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:aNAMorphic-Rig,AR_JUCE_VER>")

target_link_libraries(aNAMorphic-Rig
    PRIVATE
        namcore
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)