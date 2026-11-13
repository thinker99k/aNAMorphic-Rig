#include "Defaults.h"

#include "AppPaths.h"
#include "model/RigModel.h"

namespace rig::defaults
{
    namespace
    {
        // RigModel::loadPatch 가 읽을 수 있는 형태인지: JSON 객체 + "blocks" 배열 (빈 배열도 정상)
        bool isValidPatch (const juce::var& json)
        {
            return json.getDynamicObject() != nullptr && json.getProperty ("blocks", {}).isArray();
        }

        // 기본 패치: 기본 이름의 씬 8개, 블록 없음 (IN -> OUT)
        juce::String defaultPatchText (const juce::String& name)
        {
            juce::Array<juce::var> scenes;

            for (int i = 1; i <= RigModel::numScenes; ++i)
            {
                auto scene = std::make_unique<juce::DynamicObject>();
                scene->setProperty ("name", "Scene " + juce::String (i));
                scenes.add (juce::var (scene.release()));
            }

            auto root = std::make_unique<juce::DynamicObject>();
            root->setProperty ("version", 1);
            root->setProperty ("name", name);
            root->setProperty ("scenes", scenes);
            root->setProperty ("blocks", juce::Array<juce::var>());

            return juce::JSON::toString (juce::var (root.release()));
        }

        void ensureDirectory (const juce::File& dir)
        {
            if (! dir.isDirectory())
            {
                const auto result = dir.createDirectory();
                if (result.failed())
                    DBG ("Could not create " << dir.getFullPathName() << ": " << result.getErrorMessage());
            }
        }

        void ensurePatch (const juce::File& file)
        {
            if (file.existsAsFile() && file.getSize() > 0)
            {
                // 이름 검증은 RigModel::loadPatch 에서 (파일은 다음 저장 때 version, name, blocks 순으로 다시 써진다)
                if (isValidPatch (juce::JSON::parse (file.loadFileAsString())))
                    return;

                // 깨진 파일은 덮어쓰기 전에 남겨 둔다
                file.copyFileTo (file.getSiblingFile (file.getFileName() + ".bak"));
            }

            file.replaceWithText (defaultPatchText (file.getFileName()));
        }

        // settings.json: 없거나, 0 바이트거나, JSON 객체가 아니면 기본값으로 다시 쓴다.
        // 객체인데 키가 빠져 있으면 빠진 키만 기본값으로 채운다 (범위 제한은 RigModel::loadSettings 에서)
        void ensureSettings (const juce::File& file)
        {
            const std::pair<const char*, juce::var> defaultsList[] = {
                { "version", 1 },
                { "inputGainDb", 0.0 },
                { "outputGainDb", 0.0 },
            };

            juce::var json;

            if (file.existsAsFile() && file.getSize() > 0)
            {
                json = juce::JSON::parse (file.loadFileAsString());

                // 깨진 파일은 덮어쓰기 전에 남겨 둔다
                if (json.getDynamicObject() == nullptr)
                    file.copyFileTo (file.getSiblingFile (file.getFileName() + ".bak"));
            }

            if (json.getDynamicObject() == nullptr)
                json = juce::var (new juce::DynamicObject());

            auto* obj = json.getDynamicObject();
            bool changed = ! file.existsAsFile() || file.getSize() == 0;

            for (const auto& [key, value] : defaultsList)
            {
                if (! obj->hasProperty (key))
                {
                    obj->setProperty (key, value);
                    changed = true;
                }
            }

            if (changed)
                file.replaceWithText (juce::JSON::toString (json));
        }
    }

    void ensureAll()
    {
        ensureDirectory (paths::namDir());
        ensureDirectory (paths::irDir());
        ensureDirectory (paths::patchDir());

        for (int n = 1; n <= RigModel::numPatches; ++n)
            ensurePatch (paths::patchFile (n));

        ensureSettings (paths::settingsFile());
    }
}
