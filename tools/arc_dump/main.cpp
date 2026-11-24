// arc_dump: lihat isi arsip .ARC Hulk dan ekstrak tekstur untuk dicek.
//   arc_dump <folder_data> sections
//   arc_dump <folder_data> list <Section> [maks_baris]
//   arc_dump <folder_data> tex <index|potongan_nama> <out.ppm> [maxDim]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../engine/arc.h"
#include "../../engine/dds.h"

static void usage() {
    std::puts("arc_dump <folder_data> sections\n"
              "arc_dump <folder_data> list <Section> [maks_baris]\n"
              "arc_dump <folder_data> tex <index|potongan_nama> <out.ppm> [maxDim]");
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(); return 1; }
    hulk::ArcIndex idx;
    std::string err;
    if (!idx.load(argv[1], &err)) { std::fprintf(stderr, "Gagal: %s\n", err.c_str()); return 1; }
    std::string cmd = argv[2];

    if (cmd == "sections") {
        for (auto& s : idx.sections) std::printf("%-14s %-13s %6zu entri\n", s.name.c_str(), s.arcFile.c_str(), s.entries.size());
        return 0;
    }
    if (cmd == "list" && argc >= 4) {
        const hulk::ArcSection* s = idx.find(argv[3]);
        if (!s) { std::fprintf(stderr, "Section tidak ada\n"); return 1; }
        size_t maxRows = argc >= 5 ? (size_t)std::atoi(argv[4]) : s->entries.size();
        for (size_t k = 0; k < s->entries.size() && k < maxRows; ++k) {
            auto& e = s->entries[k];
            std::printf("%5zu  off=%-10u size=%-9u %s\n", k, e.offset, e.size, idx.entryName(*s, e).c_str());
        }
        return 0;
    }
    if (cmd == "tex" && argc >= 5) {
        const hulk::ArcSection* s = idx.find("Textures");
        if (!s) { std::fprintf(stderr, "Section Textures tidak ada\n"); return 1; }
        size_t pick = (size_t)-1;
        std::string q = argv[3];
        if (std::isdigit((unsigned char)q[0])) pick = (size_t)std::atoi(argv[3]);
        else for (size_t k = 0; k < s->entries.size(); ++k)
            if (idx.entryName(*s, s->entries[k]).find(q) != std::string::npos) { pick = k; break; }
        if (pick >= s->entries.size()) { std::fprintf(stderr, "Tekstur tidak ketemu\n"); return 1; }
        std::vector<uint8_t> data;
        if (!idx.readEntry(*s, s->entries[pick], data)) { std::fprintf(stderr, "Gagal baca entry\n"); return 1; }
        hulk::Image img;
        int maxDim = argc >= 6 ? std::atoi(argv[5]) : 1024;
        if (!hulk::decodeTexture(data.data(), data.size(), maxDim, img)) { std::fprintf(stderr, "Format tidak didukung\n"); return 1; }
        FILE* f = std::fopen(argv[4], "wb");
        if (!f) return 1;
        std::fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
        for (int p = 0; p < img.w * img.h; ++p) std::fwrite(&img.rgba[4 * p], 1, 3, f);
        std::fclose(f);
        std::printf("Tekstur #%zu %s -> %dx%d, disimpan %s\n", pick, idx.entryName(*s, s->entries[pick]).c_str(), img.w, img.h, argv[4]);
        return 0;
    }
    usage();
    return 1;
}
