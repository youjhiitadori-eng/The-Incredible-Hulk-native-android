// asset_probe: scan folder game, tulis report.txt (path, ukuran, 16 byte pertama, tebakan tipe).
// Pakai: asset_probe <folder_game> [report.txt]
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool starts(const std::vector<uint8_t>& b, const char* s, size_t n) {
    if (b.size() < n) return false;
    for (size_t i = 0; i < n; ++i) if (b[i] != (uint8_t)s[i]) return false;
    return true;
}

static std::string guess(const std::vector<uint8_t>& b) {
    if (starts(b, "DDS ", 4)) return "DDS texture";
    if (starts(b, "\x89PNG", 4)) return "PNG image";
    if (starts(b, "\xFF\xD8\xFF", 3)) return "JPEG image";
    if (starts(b, "BM", 2)) return "BMP image";
    if (starts(b, "OggS", 4)) return "Ogg audio";
    if (starts(b, "ID3", 3)) return "MP3 audio (ID3)";
    if (starts(b, "RIFF", 4)) {
        if (b.size() >= 12 && b[8] == 'W' && b[9] == 'A' && b[10] == 'V' && b[11] == 'E') return "WAV audio";
        if (b.size() >= 12 && b[8] == 'A' && b[9] == 'V' && b[10] == 'I') return "AVI video";
        return "RIFF container";
    }
    if (starts(b, "BIK", 3)) return "Bink video";
    if (starts(b, "MZ", 2)) return "Windows executable/DLL";
    if (starts(b, "PK\x03\x04", 4)) return "ZIP archive";
    if (starts(b, "Rar!", 4)) return "RAR archive";
    if (starts(b, "7z\xBC\xAF", 4)) return "7z archive";
    if (starts(b, "FSB", 3)) return "FMOD sound bank";
    if (starts(b, "FEV", 3)) return "FMOD event file";
    if (!b.empty()) {
        size_t printable = 0;
        for (uint8_t c : b) if ((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t') ++printable;
        if (printable == b.size()) return "text-like";
    }
    return "unknown (kemungkinan format custom / arsip game)";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Pakai: asset_probe <folder_game> [report.txt]\n";
        return 1;
    }
    fs::path root = argv[1];
    std::string outPath = argc >= 3 ? argv[2] : "report.txt";
    if (!fs::exists(root)) { std::cerr << "Folder tidak ada: " << root << "\n"; return 1; }

    std::ofstream out(outPath);
    std::map<std::string, std::pair<size_t, uintmax_t>> byExt;
    size_t count = 0;

    out << "path | size | first16 | guess\n";
    for (auto& e : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (!e.is_regular_file()) continue;
        std::ifstream f(e.path(), std::ios::binary);
        std::vector<uint8_t> buf(16);
        f.read((char*)buf.data(), 16);
        buf.resize((size_t)f.gcount());

        std::ostringstream hex;
        for (uint8_t c : buf) hex << std::hex << std::setw(2) << std::setfill('0') << (int)c << ' ';

        uintmax_t sz = e.file_size();
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext.empty()) ext = "(none)";
        byExt[ext].first++;
        byExt[ext].second += sz;
        ++count;

        out << fs::relative(e.path(), root).generic_string() << " | " << sz << " | " << hex.str()
            << "| " << guess(buf) << "\n";
    }

    out << "\n== Ringkasan per ekstensi ==\n";
    for (auto& kv : byExt)
        out << kv.first << " : " << kv.second.first << " file, " << kv.second.second << " byte\n";
    out << "Total file: " << count << "\n";
    std::cout << "Selesai: " << count << " file. Hasil: " << outPath << "\n";
    return 0;
}
