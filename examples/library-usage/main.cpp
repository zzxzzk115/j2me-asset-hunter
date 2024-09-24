#include "j2me-asset-hunter/lib.hpp"

int main()
{
    auto pngHunter = jhunter::png::PngHunter();
    auto archive   = jhunter::io::ZipArchive("assets/test.jar");

    auto entries = archive.listEntries();
    for (const auto& entry : entries)
    {
        auto buffer = archive.readFile(entry);
        pngHunter.addSourceBuffer(buffer);
    }

    auto pngFiles = pngHunter.parsePngFiles();
    pngHunter.savePngFiles(pngFiles, "out", "image_");
    return 0;
}