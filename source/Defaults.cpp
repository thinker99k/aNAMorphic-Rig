#include "Defaults.h"

#include "AppPaths.h"
#include "model/RigModel.h"

namespace rig::defaults
{
    namespace
    {
        // RigModel::loadPatch 가 읽을 수 있는 형태인지: JSON 객체 + "blocks" 배열 (빈 배열도 정상)
        bool isValidPatch (const juce::String& text)
        {
            const auto json = juce::JSON::parse (text);
            return json.getDynamicObject() != nullptr && json.getProperty ("blocks", {}).isArray();
        }

        // 기본 패치: 빈 NAM 블록 + 빈 IR 블록 ("file": "" = 아직 파일 미지정)
        juce::String defaultPatchText()
        {
            juce::Array<juce::var> blocks;

            for (const auto* type : { "nam", "ir" })
            {
                auto obj = std::make_unique<juce::DynamicObject>();
                obj->setProperty ("type", type);
                obj->setProperty ("file", "");
                obj->setProperty ("bypass", false);
                blocks.add (juce::var (obj.release()));
            }

            auto root = std::make_unique<juce::DynamicObject>();
            root->setProperty ("version", 1);
            root->setProperty ("blocks", blocks);

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
                const auto text = file.loadFileAsString();

                if (isValidPatch (text))
                    return;

                // 깨진 파일은 덮어쓰기 전에 남겨 둔다
                file.copyFileTo (file.getSiblingFile (file.getFileName() + ".bak"));
            }

            file.replaceWithText (defaultPatchText());
        }
    }

    void ensureAll()
    {
        ensureDirectory (paths::namDir());
        ensureDirectory (paths::irDir());
        ensureDirectory (paths::patchDir());

        for (int n = 1; n <= RigModel::numPatches; ++n)
            ensurePatch (paths::patchFile (n));
    }
}
