#include "j2me-asset-hunter/lib.hpp"

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("j2me-asset-hunter");

    program.add_argument("jar").help("a .jar file to handle with.");

    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    auto jarFilePath = program.get("jar");

    jhunter::hunter::PngHunter pngHunter;

    jhunter::hunter::MidiHunter         midiHunter;
    jhunter::hunter::MidiHunterSettings midiSettings {};
    midiSettings.soundFontPath = "assets/default.sf2";
    midiHunter.setSettings(midiSettings);

    jhunter::io::ZipArchive archive(jarFilePath);

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