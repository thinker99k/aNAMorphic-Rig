#include "LibraryImport.h"

#include <vector>

namespace rig
{
    namespace
    {
        constexpr int copyBufferSize = 64 * 1024;

        enum class CopyResult { ok, failed, cancelled };

        // 같은 폴더의 숨은 임시 파일에 다 쓴 뒤 target 을 교체한다 (paths::writeText 와 같은 방식).
        // 원본을 다 읽고 나서 교체하므로 source == target (라이브러리 안의 파일을 다시 드롭) 이어도 안전
        CopyResult copyFile (const juce::File& source, const juce::File& target, const std::function<bool()>& shouldCancel)
        {
            juce::FileInputStream input (source);
            if (input.failedToOpen() || target.getParentDirectory().createDirectory().failed())
                return CopyResult::failed;

            juce::TemporaryFile temp (target, juce::TemporaryFile::useHiddenFile);   // 교체하지 않으면 소멸자가 지운다

            {
                juce::FileOutputStream output (temp.getFile());
                if (! output.openedOk())
                    return CopyResult::failed;

                std::vector<char> buffer ((size_t) copyBufferSize);

                for (;;)
                {
                    if (shouldCancel())
                        return CopyResult::cancelled;

                    const auto count = input.read (buffer.data(), copyBufferSize);
                    if (count < 0 || input.getStatus().failed())
                        return CopyResult::failed;
                    if (count == 0)
                        break;
                    if (! output.write (buffer.data(), (size_t) count))
                        return CopyResult::failed;
                }

                output.flush();
                if (output.getStatus().failed())
                    return CopyResult::failed;
            }

            if (shouldCancel())
                return CopyResult::cancelled;

            return temp.overwriteTargetFileWithTemporary() ? CopyResult::ok : CopyResult::failed;
        }
    }

    LibraryImportResult importLibraryFiles (const juce::StringArray& files,
                                            const juce::File& namDirectory,
                                            const juce::File& irDirectory,
                                            const std::function<bool()>& shouldCancel)
    {
        LibraryImportResult result;
        for (const auto& path : files)
        {
            if (shouldCancel())
            {
                result.cancelled = true;
                break;
            }

            const juce::File source (path);
            if (! source.existsAsFile() || ! source.hasFileExtension (".nam;.wav"))
            {
                result.failed.add (source.getFileName());
                continue;
            }

            const auto& folder = source.hasFileExtension (".nam") ? namDirectory : irDirectory;
            const auto target = folder.getChildFile (source.getFileNameWithoutExtension() + source.getFileExtension().toLowerCase());

            const auto copied = copyFile (source, target, shouldCancel);
            if (copied == CopyResult::cancelled)
            {
                result.cancelled = true;
                break;
            }

            if (copied == CopyResult::ok)
                result.imported.add (target);
            else
                result.failed.add (source.getFileName());
        }
        return result;
    }
}
