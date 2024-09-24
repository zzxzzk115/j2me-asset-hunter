#include "j2me-asset-hunter/lib.hpp"

int main()
{
    jhunter::hunter::PngHunter pngHunter;

    jhunter::hunter::MidiHunter         midiHunter;
    jhunter::hunter::MidiHunterSettings midiSettings {};
    midiSettings.soundFontPath = "assets/default.sf2";
    midiHunter.setSettings(midiSettings);

    jhunter::io::ZipArchive archive("assets/test.jar");

    auto entries = archive.listEntries();
    for (const auto& entry : entries)
    {
        auto buffer = archive.readFile(entry);
        pngHunter.addSourceBuffer(buffer);
        midiHunter.addSourceBuffer(buffer);
    }

    auto pngFiles = pngHunter.parseFiles();
    pngHunter.saveFiles(pngFiles, "out", "image_");

    auto midiFiles = midiHunter.parseFiles();
    midiHunter.saveFiles(midiFiles, "out", "audio_");

    return 0;
}