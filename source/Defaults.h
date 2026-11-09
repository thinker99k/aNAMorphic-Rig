#pragma once

namespace rig::defaults
{
    // 처음 실행(또는 파일이 지워지거나 깨진 경우)에 필요한 폴더/파일을 만든다.
    // MainWindow 를 만들기 전에 불러야 한다 (RigModel 이 장치 초기화보다 먼저 생성되므로).
    //  - nam/, ir/, patch/ 폴더
    //  - patch/config01~32 : 없거나, 0 바이트거나, "blocks" 배열을 가진 JSON 객체가 아니면 기본 패치로 다시 쓴다
    //                        (내용이 있는데 깨진 파일은 configNN.bak 으로 먼저 복사)
    //  - settings.json       : 없거나, 0 바이트거나, JSON 객체가 아니면 기본값(version 1, 입력/출력 0 dB)으로 쓰고
    //                          (깨진 파일은 settings.json.bak 으로 먼저 복사), 빠진 키만 있으면 그 키만 채운다
    // audio-device.xml 은 장치를 연 뒤에 만들어야 하므로 MainWindow 에서 처리한다.
    void ensureAll();
}
