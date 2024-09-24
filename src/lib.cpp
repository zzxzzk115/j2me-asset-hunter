#include "j2me-asset-hunter/lib.hpp"

#include <fluidsynth.h>

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace
{
    void writeWavHeader(std::ofstream& file, int sampleRate, int numChannels, int bitsPerSample, int dataSize)
    {
        // Write the RIFF header
        file.write("RIFF", 4);                                    // Chunk ID
        int32_t chunkSize = 36 + dataSize;                        // Chunk size
        file.write(reinterpret_cast<const char*>(&chunkSize), 4); // Chunk size
        file.write("WAVE", 4);                                    // Format

        // Write the fmt subchunk
        file.write("fmt ", 4);                                           // Subchunk1 ID
        int32_t subchunk1Size = 16;                                      // Subchunk1 size (16 for PCM)
        file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
        int16_t audioFormat = 1;                                         // Audio format (1 for PCM)
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        file.write(reinterpret_cast<const char*>(&numChannels), 2);      // Number of channels
        file.write(reinterpret_cast<const char*>(&sampleRate), 4);       // Sample rate
        int32_t byteRate = sampleRate * numChannels * bitsPerSample / 8; // Byte rate
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        int16_t blockAlign = numChannels * bitsPerSample / 8;            // Block align
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);    // Bits per sample

        // Write the data subchunk
        file.write("data", 4);                                   // Subchunk2 ID
        file.write(reinterpret_cast<const char*>(&dataSize), 4); // Subchunk2 size
    }
} // namespace

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
                std::cerr << "Error opening zip file: " << zipPath << " (Error code: " << error << ")" << std::endl;
                ;
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
                std::cerr << "Error getting file stat: " << fileName << std::endl;
                throw std::runtime_error("Failed to get file stat.");
            }

            std::unique_ptr<zip_file_t, ZipFileDeleter> file(zip_fopen(m_ZipHandle.get(), fileName.c_str(), 0));
            if (!file)
            {
                std::cerr << "Error opening file: " << fileName << std::endl;
                throw std::runtime_error("Failed to open file.");
            }

            std::vector<char> buffer(st.size);
            zip_fread(file.get(), buffer.data(), buffer.size());

            return buffer;
        }
    } // namespace io

    namespace hunter
    {
        // Find a sequence of bytes in a buffer starting from a given position
        template<typename FileType>
        size_t HunterBase<FileType>::findSequenceInBuffer(const std::vector<char>& buffer,
                                                          const std::string&       seq,
                                                          size_t                   startPos) const
        {
            auto it = std::search(buffer.begin() + startPos, buffer.end(), seq.begin(), seq.end());
            return it != buffer.end() ? std::distance(buffer.begin(), it) : std::string::npos;
        }

        const std::string MAGIC_PNG_START = "\x89PNG\r\n\x1A\n";                // The PNG file signature (header)
        const std::string MAGIC_PNG_END   = "\x49\x45\x4E\x44\xAE\x42\x60\x82"; // The IEND chunk (PNG end marker)

        std::vector<PngFile> PngHunter::parseFiles() const
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

        void PngHunter::saveFiles(const std::vector<PngFile>& pngFiles,
                                  const std::string&          outputDir,
                                  const std::string&          prefix) const
        {
            // Check if the output directory exists, if not, create it
            std::filesystem::path dir(outputDir);
            if (!std::filesystem::exists(dir))
            {
                if (!std::filesystem::create_directories(dir))
                {
                    std::cerr << "Error: Failed to create output directory: " << outputDir << std::endl;
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
                    std::cerr << "Error: Failed to open file for writing: " << filePath << std::endl;
                    throw std::runtime_error("Failed to open file for writing.");
                }

                // Write the PNG data to the file
                outputFile.write(pngFiles[i].data.data(), pngFiles[i].data.size());
                if (!outputFile)
                {
                    std::cerr << "Error: Failed to write PNG data to file: " << filePath << std::endl;
                    throw std::runtime_error("Failed to write PNG data to file.");
                }

                // Close the file
                outputFile.close();
                std::cout << "PNG file saved: " << filePath << std::endl;
            }
        }

        const std::string MAGIC_MIDI_HEADER = "MThd";

        std::vector<MidiFile> MidiHunter::parseFiles() const
        {
            std::vector<MidiFile> midiFiles;

            // Iterate through each buffer in the source buffers
            for (const auto& buffer : m_SourceBuffers)
            {
                size_t searchPos = 0; // Start position for searching in the current buffer

                // Continue searching for 'MThd' until no more headers are found in the buffer
                while (searchPos < buffer.size())
                {
                    // Find MIDI header 'MThd'
                    size_t headerPos = findSequenceInBuffer(buffer, MAGIC_MIDI_HEADER, searchPos);
                    if (headerPos != std::string::npos)
                    {
                        // From the found header to the end of the buffer, assume it's a MIDI file
                        std::vector<char> currentMidi;
                        currentMidi.insert(currentMidi.end(), buffer.begin() + headerPos, buffer.end());

                        // Add the found MIDI file to the result list
                        midiFiles.emplace_back(currentMidi);

                        // Move the search position forward to avoid infinite loop
                        searchPos = headerPos + MAGIC_MIDI_HEADER.size();
                    }
                    else
                    {
                        // No more MIDI headers found in this buffer, stop searching
                        break;
                    }
                }
            }

            return midiFiles;
        }

        void MidiHunter::saveFiles(const std::vector<MidiFile>& midiFiles,
                                   const std::string&           outputDir,
                                   const std::string&           prefix) const
        {
            // Check if the output directory exists, if not, create it
            std::filesystem::path dir(outputDir);
            if (!std::filesystem::exists(dir))
            {
                if (!std::filesystem::create_directories(dir))
                {
                    std::cerr << "Error: Failed to create output directory: " << outputDir << std::endl;
                    throw std::runtime_error("Failed to create output directory.");
                }
            }

            // Iterate over all MIDI files and save them
            for (size_t i = 0; i < midiFiles.size(); ++i)
            {
                // Construct the output file path with the prefix and index
                std::string           fileName = prefix + std::to_string(i) + ".mid";
                std::filesystem::path filePath = dir / fileName;

                // Open a binary file stream for writing
                std::ofstream outputFile(filePath, std::ios::binary);
                if (!outputFile)
                {
                    std::cerr << "Error: Failed to open file for writing: " << filePath << std::endl;
                    throw std::runtime_error("Failed to open file for writing.");
                }

                // Write the MIDI data to the file
                outputFile.write(midiFiles[i].data.data(), midiFiles[i].data.size());
                if (!outputFile)
                {
                    std::cerr << "Error: Failed to write MIDI data to file: " << filePath << std::endl;
                    throw std::runtime_error("Failed to write MIDI data to file.");
                }

                // Close the file
                outputFile.close();
                std::cout << "MIDI file saved: " << filePath << std::endl;

                // If export WAV
                if (m_Settings.exportWAV)
                {
                    std::string           outWAVFile     = prefix + std::to_string(i) + ".wav";
                    std::filesystem::path outWAVFilePath = dir / outWAVFile;
                    exportWAVFile(filePath.generic_string(), outWAVFilePath.generic_string());
                }
            }
        }

        void MidiHunter::setSettings(const MidiHunterSettings& settings) { m_Settings = settings; }

        void MidiHunter::exportWAVFile(const std::string& midiFileName, const std::string& outputFileName) const
        {
            // Initialize FluidSynth settings
            fluid_settings_t* settings = new_fluid_settings();

            // Set the sample rate and other settings for WAV output
            int sampleRate = 44100; // Standard sample rate
            fluid_settings_setnum(settings, "synth.sample-rate", sampleRate);

            // Create FluidSynth synthesizer
            fluid_synth_t* synth = new_fluid_synth(settings);

            // Load a SoundFont
            if (fluid_synth_sfload(synth, m_Settings.soundFontPath.c_str(), 1) == FLUID_FAILED)
            {
                std::cerr << "Error: Failed to load SoundFont." << std::endl;
                delete_fluid_synth(synth);
                delete_fluid_settings(settings);
                return;
            }

            // Create a FluidSynth player to load and play the MIDI file
            fluid_player_t* player = new_fluid_player(synth);
            if (fluid_player_add(player, midiFileName.c_str()) == FLUID_FAILED)
            {
                std::cerr << "Error: Failed to load MIDI file." << std::endl;
                delete_fluid_player(player);
                delete_fluid_synth(synth);
                delete_fluid_settings(settings);
                return;
            }

            // Start playing the MIDI file
            fluid_player_play(player);

            // Prepare output WAV file
            std::ofstream wavFile(outputFileName, std::ios::binary);
            if (!wavFile)
            {
                std::cerr << "Error: Unable to open output WAV file." << std::endl;
                return;
            }

            // Number of channels, bits per sample, and buffer size
            const int numChannels   = 2;    // Stereo
            const int bitsPerSample = 16;   // 16-bit audio
            const int bufferSize    = 1024; // Size of the buffer to process

            // Prepare to store audio data
            int16_t* leftBuffer   = new int16_t[bufferSize]; // Left channel
            int16_t* rightBuffer  = new int16_t[bufferSize]; // Right channel
            int      totalSamples = 0;                       // Count the total number of samples written

            // Write a placeholder WAV header (we'll overwrite this later with correct data size)
            writeWavHeader(wavFile, sampleRate, numChannels, bitsPerSample, 0);

            // Loop until MIDI playback finishes
            while (fluid_player_get_status(player) == FLUID_PLAYER_PLAYING)
            {
                // Synthesize audio data and write it to the buffers
                fluid_synth_write_s16(synth, bufferSize, leftBuffer, 0, 1, rightBuffer, 0, 1);

                // Write audio data to the WAV file
                for (int i = 0; i < bufferSize; ++i)
                {
                    wavFile.write(reinterpret_cast<const char*>(&leftBuffer[i]), sizeof(int16_t));
                    wavFile.write(reinterpret_cast<const char*>(&rightBuffer[i]), sizeof(int16_t));
                }

                totalSamples += bufferSize;
            }

            // Calculate the final size of the audio data
            int dataSize = totalSamples * numChannels * bitsPerSample / 8;

            // Seek back to the beginning of the file and write the correct header
            wavFile.seekp(0, std::ios::beg);
            writeWavHeader(wavFile, sampleRate, numChannels, bitsPerSample, dataSize);

            // Clean up resources
            delete[] leftBuffer;
            delete[] rightBuffer;
            wavFile.close();
            delete_fluid_player(player);
            delete_fluid_synth(synth);
            delete_fluid_settings(settings);

            std::cout << "MIDI to WAV conversion complete. Output saved as '" << outputFileName << "'." << std::endl;
        }
    } // namespace hunter
} // namespace jhunter