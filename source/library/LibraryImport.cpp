#include "LibraryImport.h"

#include "AppPaths.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <system_error>

namespace rig
{
    namespace
    {
        juce::String systemError (int code)
        {
            return juce::String (std::error_code (code, std::generic_category()).message());
        }

        // wbx 는 이미 있는 파일을 열거나 지우지 않는다. 존재 확인 뒤 copyFileTo() 를 쓰면
        // 그 사이 다른 프로세스가 만든 파일을 덮어쓸 수 있으므로 생성 자체를 배타적으로 한다.
        std::FILE* createNewFile (const juce::File& file)
        {
           #if JUCE_WINDOWS
            return _wfopen (paths::toStdPath (file).c_str(), L"wbx");
           #else
            return std::fopen (paths::toStdPath (file).c_str(), "wbx");
           #endif
        }

        struct CopyTarget
        {
            juce::File file;
            std::unique_ptr<std::FILE, decltype (&std::fclose)> stream { nullptr, &std::fclose };
            bool complete = false;

            ~CopyTarget()
            {
                stream.reset();
                if (! complete && file != juce::File())
                    file.deleteFile();   // 실패·취소한 이번 복사의 불완전한 파일만 지운다
            }
        };

        juce::Result copyFile (const juce::File& source, const juce::File& folder,
                               const std::function<bool()>& shouldCancel)
        {
            juce::FileInputStream input (source);
            if (input.failedToOpen())
                return input.getStatus();
            const auto expectedSize = input.getTotalLength();

            const auto created = folder.createDirectory();
            if (created.failed())
                return created;

            CopyTarget target;
            const auto stem = source.getFileNameWithoutExtension();
            const auto extension = source.getFileExtension().toLowerCase();

            for (int number = 1; ; ++number)
            {
                if (shouldCancel())
                    return juce::Result::fail ("Import cancelled.");

                const auto suffix = number == 1 ? juce::String() : " (" + juce::String (number) + ")";
                const auto candidate = folder.getChildFile (stem + suffix + extension);
                target.stream.reset (createNewFile (candidate));

                if (target.stream != nullptr)
                {
                    target.file = candidate;
                    break;
                }

                const int error = errno;
                if (error != EEXIST && ! candidate.exists() && ! candidate.isSymbolicLink())
                    return juce::Result::fail (systemError (error));
            }

            std::array<char, 64 * 1024> buffer;
            // isExhausted() 는 원본 경로를 다시 stat 한다. 복사 중 원본이 이동해도 열린
            // 스트림을 끝까지 읽도록 EOF 를 사용하고, 크기가 바뀌었다면 실패로 처리한다.
            for (;;)
            {
                if (shouldCancel())
                    return juce::Result::fail ("Import cancelled.");

                const auto count = input.read (buffer.data(), (int) buffer.size());
                if (input.getStatus().failed())
                    return input.getStatus();
                if (count < 0)
                    return juce::Result::fail ("The source file changed or could not be read.");
                if (count == 0)
                    break;
                if (std::fwrite (buffer.data(), 1, (size_t) count, target.stream.get()) != (size_t) count)
                    return juce::Result::fail (systemError (errno));
            }

            if (input.getPosition() != expectedSize)
                return juce::Result::fail ("The source file changed during import. Please try again.");

            // fclose 의 flush 실패도 복사 실패로 처리한다.
            if (std::fclose (target.stream.release()) != 0)
                return juce::Result::fail (systemError (errno));
            if (shouldCancel())
                return juce::Result::fail ("Import cancelled.");

            target.complete = true;
            return juce::Result::ok();
        }
    }

    bool isLibraryFile (const juce::File& file)
    {
        return file.hasFileExtension (".nam;.wav");
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
            if (source.isDirectory() || ! isLibraryFile (source))
            {
                result.skipped.add (path);
                continue;
            }
            if (! source.existsAsFile())
            {
                result.failed.add (path + ": File not found.");
                continue;
            }

            const auto& folder = source.hasFileExtension (".nam") ? namDirectory : irDirectory;
            if (source.isAChildOf (folder))
            {
                ++result.alreadyPresent;
                continue;
            }

            const auto copied = copyFile (source, folder, shouldCancel);
            if (shouldCancel())
            {
                result.cancelled = true;
                break;
            }
            if (copied.wasOk())
                ++result.imported;
            else
                result.failed.add (path + ": " + copied.getErrorMessage());
        }
        return result;
    }
}
