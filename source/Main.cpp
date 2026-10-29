#include "AppPaths.h"
#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/MainComponent.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace rig
{
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            // 저장된 장치 설정이 있으면 사용, 없으면 기본 장치 (입력 1채널, 출력 2채널)
            std::unique_ptr<juce::XmlElement> saved;
            if (paths::deviceSettingsFile().existsAsFile())
                saved = juce::parseXML (paths::deviceSettingsFile());

            const auto error = deviceManager.initialise (1, 2, saved.get(), true);
            if (error.isNotEmpty())
                DBG ("Audio device error: " << error);

            deviceManager.addAudioCallback (&engine);
            model.loadPatch (1);

            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (deviceManager, engine, model), true);
            setResizable (true, true);
            setResizeLimits (720, 480, 4096, 4096);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        ~MainWindow() override
        {
            // 순서 중요: UI 제거 -> 오디오 콜백 해제 -> 장치 닫기
            clearContentComponent();
            deviceManager.removeAudioCallback (&engine);
            deviceManager.closeAudioDevice();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
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
            mainWindow = std::make_unique<MainWindow> (getApplicationName());
        }

        void shutdown() override
        {
            mainWindow = nullptr;
        }

        void systemRequestedQuit() override
        {
            quit();
        }

    private:
        std::unique_ptr<MainWindow> mainWindow;
    };
}

START_JUCE_APPLICATION (rig::Application)
