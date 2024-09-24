#include "j2me-asset-hunter/lib.hpp"

#include <fstream>
#include <filesystem>

namespace jhunter
{
    namespace cli
    {}

    namespace io
    {
        ZipArchive::ZipArchive(const std::string& zipPath)
        {
            int error;
            m_ZipHandle = std::unique_ptr<zip_t, ZipDeleter>(zip_open(zipPath.c_str(), 0, &error));
            if (!m_ZipHandle)
            {
                std::cerr << "Error opening zip file: " << zipPath << " (Error code: " << error << ")\n";
                throw std::runtime_error("Failed to open zip file.");
            }
        }

        std::vector<std::string> ZipArchive::listEntries() const
        {
            std::vector<std::string> entries;
            zip_int64_t              num_entries = zip_get_num_entries(m_ZipHandle.get(), 0);
            for (zip_int64_t i = 0; i < num_entries; ++i)
            {
                const char* name = zip_get_name(m_ZipHandle.get(), i, 0);
                if (name)
                {
                    entries.emplace_back(name);
                }
            }
            return entries;
        }

        std::vector<char> ZipArchive::readFile(const std::string& fileName) const
        {
            struct zip_stat st;
            zip_stat_init(&st);
            if (zip_stat(m_ZipHandle.get(), fileName.c_str(), 0, &st) != 0)
            {
                std::cerr << "Error getting file stat: " << fileName << "\n";
                throw std::runtime_error("Failed to get file stat.");
            }

            std::unique_ptr<zip_file_t, ZipFileDeleter> file(zip_fopen(m_ZipHandle.get(), fileName.c_str(), 0));
            if (!file)
            {
                std::cerr << "Error opening file: " << fileName << "\n";
                throw std::runtime_error("Failed to open file.");
            }

            std::vector<char> buffer(st.size);
            zip_fread(file.get(), buffer.data(), buffer.size());

            return buffer;
        }
    } // namespace io

    namespace png
    {
        const std::string MAGIC_PNG_START = "\x89PNG\r\n\x1A\n";                // The PNG file signature (header)
        const std::string MAGIC_PNG_END   = "\x49\x45\x4E\x44\xAE\x42\x60\x82"; // The IEND chunk (PNG end marker)

        void PngHunter::addSourceBuffer(const std::vector<char>& buffer) { m_SourceBuffers.emplace_back(buffer); }

        std::vector<PngFile> PngHunter::parsePngFiles() const
        {
            std::vector<PngFile> pngFiles;
            std::vector<char>    currentPng;         // Buffer to store ongoing PNG data
            bool                 pngStarted = false; // Flag to track if PNG start was found

            // Iterate through all source buffers
            for (const auto& sourceBuffer : m_SourceBuffers)
            {
                size_t bufferSize = sourceBuffer.size();
                size_t i          = 0;

                // Traverse the buffer to search for PNG start and end markers
                while (i < bufferSize)
                {
                    if (!pngStarted)
                    {
                        // Look for the start of a PNG file
                        size_t startPos = findSequenceInBuffer(sourceBuffer, MAGIC_PNG_START, i);
                        if (startPos != std::string::npos)
                        {
                            // Found PNG start, begin collecting data
                            pngStarted = true;
                            i          = startPos; // Move index to start position
                            currentPng.insert(currentPng.end(),
                                              sourceBuffer.begin() + i,
                                              sourceBuffer.end()); // Add from start to end of current buffer
                            i += MAGIC_PNG_START.size();           // Move index beyond the PNG start marker
                        }
                        else
                        {
                            // No PNG start found, check if there's a PNG end marker without a start
                            size_t endPos = findSequenceInBuffer(sourceBuffer, MAGIC_PNG_END, i);
                            if (endPos != std::string::npos && !currentPng.empty())
                            {
                                // Found PNG end without start in this buffer (part of ongoing PNG from previous buffer)
                                currentPng.insert(currentPng.end(),
                                                  sourceBuffer.begin(),
                                                  sourceBuffer.begin() + endPos + MAGIC_PNG_END.size());
                                pngFiles.emplace_back(currentPng); // Add complete PNG file to the list
                                currentPng.clear();                // Clear buffer for next PNG
                                i = endPos + MAGIC_PNG_END.size(); // Move index past the PNG end
                            }
                            else
                            {
                                // No PNG start or end found in this buffer, skip the buffer
                                break;
                            }
                        }
                    }
                    else
                    {
                        // We are in the middle of a PNG, so look for the PNG end marker
                        size_t endPos = findSequenceInBuffer(sourceBuffer, MAGIC_PNG_END, i);
                        if (endPos != std::string::npos)
                        {
                            // Found PNG end marker, complete the PNG file
                            pngStarted = false;
                            currentPng.insert(currentPng.end(),
                                              sourceBuffer.begin(),
                                              sourceBuffer.begin() + endPos + MAGIC_PNG_END.size());
                            pngFiles.emplace_back(currentPng); // Add complete PNG file to the list
                            currentPng.clear();                // Clear buffer for next PNG
                            i = endPos + MAGIC_PNG_END.size(); // Move index past the PNG end marker
                        }
                        else
                        {
                            // PNG end not found, append the entire buffer to ongoing PNG data
                            currentPng.insert(currentPng.end(), sourceBuffer.begin(), sourceBuffer.end());
                            break; // Continue to the next buffer
                        }
                    }
                }
            }

            return pngFiles;
        }

        void PngHunter::savePngFiles(const std::vector<PngFile>& pngFiles,
                                     const std::string&          outputDir,
                                     const std::string&          prefix) const
        {
            // Check if the output directory exists, if not, create it
            std::filesystem::path dir(outputDir);
            if (!std::filesystem::exists(dir))
            {
                if (!std::filesystem::create_directories(dir))
                {
                    std::cerr << "Error: Failed to create output directory: " << outputDir << "\n";
                    throw std::runtime_error("Failed to create output directory.");
                }
            }

            // Iterate over all PNG files and save them
            for (size_t i = 0; i < pngFiles.size(); ++i)
            {
                // Construct the output file path with the prefix and index
                std::string           fileName = prefix + std::to_string(i) + ".png";
                std::filesystem::path filePath = dir / fileName;

                // Open a binary file stream for writing
                std::ofstream outputFile(filePath, std::ios::binary);
                if (!outputFile)
                {
                    std::cerr << "Error: Failed to open file for writing: " << filePath << "\n";
                    throw std::runtime_error("Failed to open file for writing.");
                }

                // Write the PNG data to the file
                outputFile.write(pngFiles[i].buffer.data(), pngFiles[i].buffer.size());
                if (!outputFile)
                {
                    std::cerr << "Error: Failed to write PNG data to file: " << filePath << "\n";
                    throw std::runtime_error("Failed to write PNG data to file.");
                }

                // Close the file
                outputFile.close();
                std::cout << "PNG file saved: " << filePath << "\n";
            }
        }

        size_t
        PngHunter::findSequenceInBuffer(const std::vector<char>& buffer, const std::string& seq, size_t startPos) const
        {
            size_t bufferSize = buffer.size();
            size_t seqSize    = seq.size();

            if (seqSize > bufferSize || startPos >= bufferSize)
                return std::string::npos;

            for (size_t i = startPos; i <= bufferSize - seqSize; ++i)
            {
                if (std::equal(seq.begin(), seq.end(), buffer.begin() + i))
                {
                    return i;
                }
            }

            return std::string::npos;
        }
    } // namespace png

    namespace midi
    {}
} // namespace jhunter