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

    namespace hunter
    {
        template<typename FileType>
        class HunterBase
        {
        public:
            HunterBase() = default;

            // Add a buffer to be searched
            void addSourceBuffer(const std::vector<char>& buffer) { m_SourceBuffers.emplace_back(buffer); }

            // Pure virtual function to parse files from buffers, needs to be implemented by derived classes
            virtual std::vector<FileType> parseFiles() const = 0;

            // Pure virtual function to save files
            virtual void saveFiles(const std::vector<FileType>& files,
                                   const std::string&           outputDir,
                                   const std::string&           prefix) const = 0;

        protected:
            // Utility function to find a sequence of bytes in a buffer starting at a specific position
            size_t findSequenceInBuffer(const std::vector<char>& buffer, const std::string& seq, size_t startPos) const;

            std::vector<std::vector<char>> m_SourceBuffers; // Buffers to be scanned
        };

        struct PngFile
        {
            std::vector<char> data;
        };

        class PngHunter : public HunterBase<PngFile>
        {
        public:
            std::vector<PngFile> parseFiles() const override;
            void                 saveFiles(const std::vector<PngFile>& pngFiles,
                                           const std::string&          outputDir,
                                           const std::string&          prefix) const override;
        };

        struct MidiFile
        {
            std::vector<char> data;
        };

        struct MidiHunterSettings
        {
            bool exportWAV = true;

            std::string soundFontPath;
        };

        class MidiHunter : public HunterBase<MidiFile>
        {
        public:
            std::vector<MidiFile> parseFiles() const override;
            void                  saveFiles(const std::vector<MidiFile>& midiFiles,
                                            const std::string&           outputDir,
                                            const std::string&           prefix) const override;

            void setSettings(const MidiHunterSettings& settings);

        private:
            void exportWAVFile(const std::string& midiFileName, const std::string& outputFileName) const;

        private:
            MidiHunterSettings m_Settings;
        };
    } // namespace hunter
} // namespace jhunter