#include "j2me-asset-hunter/lib.hpp"

#include <filesystem>

int main(int argc, char* argv[])
{
    auto exePath    = std::filesystem::path(argv[0]);
    auto workingDir = exePath.parent_path();

    argparse::ArgumentParser program("j2me-asset-hunter");

    program.add_argument("jar").help("a .jar file to handle with.");
    program.add_argument("-o", "--output_dir").help("the output directory.").default_value("");

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
    auto jarFileName = std::filesystem::path(jarFilePath).stem().generic_string();

    std::string outPath = program.get("-o");
    if (outPath.empty())
    {
        outPath = jarFileName + "_out";
    }

    jhunter::hunter::PngHunter pngHunter;

    jhunter::hunter::MidiHunter         midiHunter;
    jhunter::hunter::MidiHunterSettings midiSettings {};
    midiSettings.soundFontPath = (workingDir / "assets/default.sf2").generic_string();
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
    pngHunter.saveFiles(pngFiles, outPath, "image_");

    auto midiFiles = midiHunter.parseFiles();
    midiHunter.saveFiles(midiFiles, outPath, "audio_");

    return 0;
}