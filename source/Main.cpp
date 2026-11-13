#include "AppPaths.h"
#include "Defaults.h"
#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/MainComponent.h"
#include "ui/Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace rig
{
    class MainWindow final : public LeftClickOnly<juce::DocumentWindow>   // 제목 표시줄 끌기도 왼쪽 클릭만 (마우스 규칙, Widgets.h)
    {
    public:
        explicit MainWindow (const juce::String& name)
            : LeftClickOnly (name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour (juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::closeButton)
        {
            // 저장된 장치 설정이 있으면 사용, 없으면 기본 장치 (입력 1채널, 출력 2채널)
            // 마지막 인자 true: 저장된 장치를 못 열면 기본 장치로 대신 연다
            std::unique_ptr<juce::XmlElement> saved;
            if (paths::deviceSettingsFile().existsAsFile())
                saved = juce::parseXML (paths::deviceSettingsFile());

            const auto error = deviceManager.initialise (1, 2, saved.get(), true);
            if (error.isNotEmpty())
                DBG ("Audio device error: " << error);

            // 처음 실행: 지금 열린 장치를 "고른 장치"로 확정하고 audio-device.xml 을 만든다.
            // 같은 설정을 그대로 넘기면 JUCE 가 아무것도 안 하고 돌아가므로(상태 XML 도 안 만들어짐)
            // 한 번 닫았다가 같은 설정으로 다시 연다
            if (! paths::deviceSettingsFile().existsAsFile())
            {
                const auto setup = deviceManager.getAudioDeviceSetup();
                deviceManager.closeAudioDevice();
                deviceManager.setAudioDeviceSetup (setup, true);
                saveDeviceState();
            }

            deviceManager.addAudioCallback (&engine);
            model.loadPatch (1);

            // 제목 표시줄은 JUCE 가 직접 그린다 (세 OS 모두 같은 모양, 닫기 버튼만)
            setUsingNativeTitleBar (false);
            setContentOwned (new MainComponent (deviceManager, engine, model), true);
            // 창 크기 고정 (1024 x 768). 크기 조절은 추후 추가 예정
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        ~MainWindow() override
        {
            // 순서 중요: UI 제거 -> 오디오 콜백 해제 -> 장치 닫기
            clearContentComponent();
            deviceManager.removeAudioCallback (&engine);
            saveDeviceState();   // 장치를 닫기 전에 저장
            deviceManager.closeAudioDevice();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        void saveDeviceState()
        {
            if (auto xml = deviceManager.createStateXml())
                xml->writeTo (paths::deviceSettingsFile());
        }

        juce::AudioDeviceManager deviceManager;
        AudioEngine engine;
        RigModel model { engine };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    class Application final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
        const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
        bool moreThanOneInstanceAllowed() override           { return false; }

        void initialise (const juce::String&) override
        {
            // 글자를 가로로 찌그러뜨리지 않는다: 일반 글자는 자리가 모자라면 말줄임표(...)로 자르고,
            // 버튼 글자는 글꼴을 줄여 맞춘다 (앱 전체)
            juce::Font::setDefaultMinimumHorizontalScaleFactor (1.0f);
            juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

            // RigModel 이 패치를 읽기 전에 기본 폴더/파일부터 준비
            defaults::ensureAll();
            mainWindow = std::make_unique<MainWindow> (getApplicationName());
        }

        void shutdown() override
        {
            mainWindow = nullptr;
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        }

        void systemRequestedQuit() override
        {
            quit();
        }

    private:
        AppLookAndFeel lookAndFeel;   // 창보다 먼저 선언: 창이 먼저 소멸한다
        std::unique_ptr<MainWindow> mainWindow;
    };
}

START_JUCE_APPLICATION (rig::Application)
