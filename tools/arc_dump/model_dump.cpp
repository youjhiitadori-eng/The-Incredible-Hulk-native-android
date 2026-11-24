// model_dump: uji parser model.
//   model_dump <folder_data> stats
//   model_dump <folder_data> list [maks]
//   model_dump <folder_data> obj <index|potongan_nama> <out.txt>   (format teks sederhana untuk dicek)
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include "../../engine/arc.h"
#include "../../engine/dds.h"
#include "../../engine/model.h"

int main(int argc, char** argv) {
    if (argc < 3) { std::puts("model_dump <data> stats|list [n]|obj <idx|nama> <out>"); return 1; }
    hulk::ArcIndex idx; std::string err;
    if (!idx.load(argv[1], &err)) { std::fprintf(stderr, "Gagal: %s\n", err.c_str()); return 1; }
    const hulk::ArcSection* ms = idx.find("Models");
    const hulk::ArcSection* ts = idx.find("Textures");
    if (!ms || !ts) { std::fprintf(stderr, "Section Models/Textures tidak ada\n"); return 1; }
    std::unordered_map<uint32_t, size_t> texByKey;
    for (size_t k = 0; k < ts->entries.size(); ++k) texByKey[ts->entries[k].key] = k;
    std::string cmd = argv[2];
    std::vector<uint8_t> raw;

    if (cmd == "stats" || cmd == "list") {
        size_t maxRows = (cmd == "list" && argc >= 4) ? (size_t)std::atoi(argv[3]) : 0, models = 0, parts = 0, withTex = 0, partsTotal = 0;
        for (size_t k = 0; k < ms->entries.size(); ++k) {
            if (!idx.readEntry(*ms, ms->entries[k], raw)) continue;
            hulk::Model m;
            if (!hulk::parseModel(raw.data(), raw.size(), m)) continue;
            ++models; parts += m.parts.size();
            for (auto& p : m.parts) { ++partsTotal; if (p.material < m.materials.size() && texByKey.count(m.materials[p.material].texKey)) ++withTex; }
            if (cmd == "list" && models <= maxRows) {
                size_t v = 0, t = 0;
                for (auto& p : m.parts) { v += p.verts.size() / 5; t += p.indices.size() / 3; }
                std::printf("%5zu %-50s parts=%zu verts=%zu tris=%zu\n", k, m.name.c_str(), m.parts.size(), v, t);
            }
        }
        std::printf("model terbaca: %zu / %zu, mesh total: %zu, mesh bertekstur: %zu/%zu\n", models, ms->entries.size(), parts, withTex, partsTotal);
        return 0;
    }
    if (cmd == "obj" && argc >= 5) {
        size_t pick = (size_t)-1; std::string q = argv[3];
        if (std::isdigit((unsigned char)q[0])) pick = (size_t)std::atoi(argv[3]);
        else for (size_t k = 0; k < ms->entries.size(); ++k)
            if (idx.entryName(*ms, ms->entries[k]).find(q) != std::string::npos) { pick = k; break; }
        if (pick >= ms->entries.size() || !idx.readEntry(*ms, ms->entries[pick], raw)) { std::fprintf(stderr, "Model tidak ketemu\n"); return 1; }
        hulk::Model m;
        if (!hulk::parseModel(raw.data(), raw.size(), m)) { std::fprintf(stderr, "Model tanpa geometri\n"); return 1; }
        FILE* f = std::fopen(argv[4], "w");
        std::fprintf(f, "model %s\n", m.name.c_str());
        for (auto& p : m.parts) {
            long tk = -1;
            if (p.material < m.materials.size()) { auto it = texByKey.find(m.materials[p.material].texKey); if (it != texByKey.end()) tk = (long)it->second; }
            std::fprintf(f, "part tex=%ld\n", tk);
            for (size_t i = 0; i < p.verts.size(); i += 5) std::fprintf(f, "v %f %f %f %f %f\n", p.verts[i], p.verts[i+1], p.verts[i+2], p.verts[i+3], p.verts[i+4]);
            for (size_t i = 0; i < p.indices.size(); i += 3) std::fprintf(f, "f %u %u %u\n", p.indices[i], p.indices[i+1], p.indices[i+2]);
        }
        std::fclose(f);
        std::printf("Model #%zu %s: %zu part -> %s\n", pick, m.name.c_str(), m.parts.size(), argv[4]);
        return 0;
    }
    return 1;
}
