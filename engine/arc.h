// Pembaca arsip .ARC + index.ind milik The Incredible Hulk (2008, PC).
// Format hasil analisis:
//   index.ind : u32 N (=jumlah offset, 2 per section), lalu N u32 (offset_nama, offset_count) per section.
//   section   : "Nama\0" ... u32 count, count*u32 key (terurut), count*(u32 offset,u32 size,u32 0).
//   File data : <SECTION 8 huruf>.ARC. Offset di entry = posisi di file .ARC. 4 byte pertama .ARC = ujung data terakhir.
// Semua angka little-endian. Header-only supaya bisa dipakai tool PC dan engine Android.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace hulk {

struct ArcEntry {
    uint32_t key;     // nilai terurut dari index (kemungkinan hash nama, belum diverifikasi)
    uint32_t offset;  // posisi di file .ARC
    uint32_t size;    // ukuran data
};

struct ArcSection {
    std::string name;     // contoh: "Textures"
    std::string arcFile;  // contoh: "TEXTURES.ARC"
    std::vector<ArcEntry> entries;
};

inline std::string arcFileName(const std::string& section) {
    std::string s = section.substr(0, 8);
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s + ".ARC";
}

inline bool readWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { std::fclose(f); return false; }
    out.resize((size_t)n);
    size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == (size_t)n;
}

class ArcIndex {
public:
    std::string dir;
    std::vector<ArcSection> sections;

    bool load(const std::string& dataDir, std::string* err = nullptr) {
        dir = dataDir;
        sections.clear();
        std::vector<uint8_t> b;
        if (!readWholeFile(dir + "/index.ind", b)) return fail(err, "index.ind tidak ditemukan di " + dir);
        if (b.size() < 8) return fail(err, "index.ind terlalu kecil");
        auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, &b[o], 4); return v; };
        uint32_t n = u32(0);
        if (n % 2 != 0 || 4 + (size_t)n * 4 > b.size()) return fail(err, "header index.ind tidak valid");
        for (uint32_t p = 0; p < n / 2; ++p) {
            size_t no = u32(4 + 8 * p), co = u32(8 + 8 * p);
            if (no >= co || co + 4 > b.size()) return fail(err, "offset section di luar batas");
            ArcSection s;
            s.name.assign((const char*)&b[no], strnlen((const char*)&b[no], co - no));
            s.arcFile = arcFileName(s.name);
            uint32_t cnt = u32(co);
            size_t hs = co + 4, es = hs + (size_t)cnt * 4, end = es + (size_t)cnt * 12;
            if (end > b.size()) return fail(err, "section " + s.name + " melewati akhir file");
            s.entries.resize(cnt);
            for (uint32_t k = 0; k < cnt; ++k) {
                s.entries[k].key = u32(hs + 4 * k);
                s.entries[k].offset = u32(es + 12 * k);
                s.entries[k].size = u32(es + 12 * k + 4);
            }
            sections.push_back(std::move(s));
        }
        return true;
    }

    const ArcSection* find(const std::string& name) const {
        for (auto& s : sections) if (s.name == name) return &s;
        return nullptr;
    }

    // Baca isi satu entry (maksimal maxBytes) dari file .ARC-nya.
    bool readEntry(const ArcSection& s, const ArcEntry& e, std::vector<uint8_t>& out,
                   size_t maxBytes = (size_t)-1) const {
        FILE* f = std::fopen((dir + "/" + s.arcFile).c_str(), "rb");
        if (!f) return false;
        size_t n = e.size < maxBytes ? e.size : maxBytes;
        out.resize(n);
        bool ok = std::fseek(f, (long)e.offset, SEEK_SET) == 0 && (n == 0 || std::fread(out.data(), 1, n, f) == n);
        std::fclose(f);
        return ok;
    }

    // Nama aset = string C di awal entry (kalau terbaca). Prefix "TXFL" milik tekstur dibuang.
    std::string entryName(const ArcSection& s, const ArcEntry& e) const {
        std::vector<uint8_t> b;
        if (!readEntry(s, e, b, 96)) return "";
        size_t len = 0;
        while (len < b.size() && b[len] != 0) {
            if (b[len] < 32 || b[len] > 126) return "";
            ++len;
        }
        if (len == 0 || len == b.size()) return "";
        std::string n((const char*)b.data(), len);
        if (n.compare(0, 4, "TXFL") == 0) n = n.substr(4);
        return n;
    }

private:
    static bool fail(std::string* err, const std::string& m) { if (err) *err = m; return false; }
};

}  // namespace hulk
