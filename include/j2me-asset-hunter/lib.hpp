#pragma once

#include <argparse/argparse.hpp>
#include <zip.h>

namespace jhunter
{
    namespace cli
    {}

    namespace io
    {
        struct ZipDeleter
        {
            void operator()(zip_t* z) const
            {
                if (z)
                    zip_close(z);
            }
        };

        struct ZipFileDeleter
        {
            void operator()(zip_file_t* f) const
            {
                if (f)
                    zip_fclose(f);
            }
        };

        class ZipArchive
        {
        public:
            ZipArchive(const std::string& zipPath);

            std::vector<std::string> listEntries() const;
            std::vector<char>        readFile(const std::string& fileName) const;

        private:
            std::unique_ptr<zip_t, ZipDeleter> m_ZipHandle;
        };
    } // namespace io

    namespace png
    {
        struct PngFile
        {
            std::vector<char> buffer;
        };

        class PngHunter
        {
        public:
            PngHunter() = default;

            void addSourceBuffer(const std::vector<char>& buffer);

            std::vector<PngFile> parsePngFiles() const;
            void                 savePngFiles(const std::vector<PngFile>& pngFiles,
                                              const std::string&          outputDir,
                                              const std::string&          prefix) const;

        private:
            size_t findSequenceInBuffer(const std::vector<char>& buffer, const std::string& seq, size_t startPos) const;

        private:
            std::vector<std::vector<char>> m_SourceBuffers;
        };
    } // namespace png

    namespace midi
    {}
} // namespace jhunter