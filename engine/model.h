// Parser model dari MODELS.ARC (The Incredible Hulk, 2008).
// Layout entry hasil analisis:
//   u32 versi(=1), u8 0, nama\0, u8 0, u32 jumlahMaterial
//   material[]: nama string (A-Za-z0-9_) + blok data. Kunci tekstur (u32, cocok dengan key section Textures)
//               ada di (awalNama + 49 + panjangNama).
//   mesh[] (berurutan):
//     decl : u8 jumlahElemen, u16 stride, elemen[n] = {tipe,usage,idx,offsetAkhir}; elemen terakhir tanpa offsetAkhir (=stride).
//            tipe 2=float3, 3=float4, 4=ubyte4, 5=ubyte4 | usage 0=posisi 1=blendWeight 2=blendIndex 3=normal(packed) 5=texcoord 10=warna
//     u32 jumlahIndex, u32 jumlahVertex, u16 index[], vertex[stride]
//   Mesh berikutnya: decl dimulai beberapa byte setelah akhir vertex (10/12/47/72 byte, dicari dengan validasi); u32 tepat sebelum decl = indeks material mesh itu.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace hulk {

struct ModelMaterial {
    std::string name;
    uint32_t texKey = 0;  // 0 = tidak ada
};

struct ModelPart {
    uint32_t material = 0;
    std::vector<float> verts;       // x y z u v (5 float per vertex)
    std::vector<uint16_t> indices;  // segitiga
};

struct Model {
    std::string name;
    std::vector<ModelMaterial> materials;
    std::vector<ModelPart> parts;
    float bmin[3] = {0, 0, 0}, bmax[3] = {0, 0, 0};
};

namespace model_detail {

inline uint32_t u32(const uint8_t* b, size_t o) { uint32_t v; std::memcpy(&v, b + o, 4); return v; }
inline uint16_t u16(const uint8_t* b, size_t o) { uint16_t v; std::memcpy(&v, b + o, 2); return v; }
inline float f32(const uint8_t* b, size_t o) { float v; std::memcpy(&v, b + o, 4); return v; }

struct Decl { size_t end = 0; int stride = 0; int posOff = -1, uvOff = -1; };

inline bool parseDecl(const uint8_t* b, size_t n, size_t s, Decl& d) {
    if (s + 3 > n) return false;
    int c = b[s];
    if (c < 1 || c > 8) return false;
    int stride = u16(b, s + 1);
    if (stride < 12 || stride > 128) return false;
    size_t q = s + 3;
    int prev = 0;
    d = Decl();
    d.stride = stride;
    for (int r = 0; r < c; ++r) {
        if (q + 3 > n) return false;
        int t = b[q], u = b[q + 1], i = b[q + 2], end;
        if (r < c - 1) { if (q + 4 > n) return false; end = b[q + 3]; q += 4; }
        else { end = stride; q += 3; }
        if (end <= prev || end > stride || t == 0 || t > 8 || u > 20) return false;
        if (u == 0 && i == 0 && t == 2 && d.posOff < 0) d.posOff = prev;
        if (u == 5 && i == 0 && t == 3 && d.uvOff < 0) d.uvOff = prev;
        prev = end;
    }
    d.end = q;
    return d.posOff >= 0;
}

// Baca [u32 idx][u32 vtx][indices][vertices] mulai di d.end. Kembalikan akhir data vertex, 0 kalau gagal.
inline size_t parseMesh(const uint8_t* b, size_t n, const Decl& d, ModelPart& part) {
    size_t p = d.end;
    if (p + 8 > n) return 0;
    uint32_t ic = u32(b, p), vc = u32(b, p + 4);
    if (ic < 3 || ic % 3 || ic > 600000 || vc < 3 || vc > 65535) return 0;
    size_t ip = p + 8, vp = ip + (size_t)ic * 2;
    if (vp + (size_t)vc * d.stride > n) return 0;
    part.indices.resize(ic);
    for (uint32_t i = 0; i < ic; ++i) {
        uint16_t v = u16(b, ip + 2 * i);
        if (v >= vc) return 0;
        part.indices[i] = v;
    }
    part.verts.resize((size_t)vc * 5);
    for (uint32_t i = 0; i < vc; ++i) {
        size_t o = vp + (size_t)i * d.stride;
        float* w = &part.verts[(size_t)i * 5];
        w[0] = f32(b, o + d.posOff); w[1] = f32(b, o + d.posOff + 4); w[2] = f32(b, o + d.posOff + 8);
        w[3] = w[4] = 0.0f;
        if (d.uvOff >= 0) { w[3] = f32(b, o + d.uvOff); w[4] = f32(b, o + d.uvOff + 4); }
    }
    return vp + (size_t)vc * d.stride;
}

inline bool isNameChar(uint8_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

}  // namespace model_detail

// Return false kalau entry tidak punya geometri yang bisa dibaca (contoh: node kosong).
inline bool parseModel(const uint8_t* b, size_t n, Model& m) {
    using namespace model_detail;
    m = Model();
    if (n < 40) return false;
    size_t e = 6;
    while (e < n && b[e] != 0) ++e;
    if (e >= n || e == 6 || e + 6 > n) return false;
    m.name.assign((const char*)b + 6, e - 6);
    uint32_t mc = u32(b, e + 2);
    if (mc > 255) return false;

    // mesh pertama: cari decl valid + header mesh valid setelah daftar material
    size_t first = 0, limit = e + 6 + (size_t)mc * 220 + 512;
    if (limit > n) limit = n;
    Decl d;
    ModelPart part;
    size_t vend = 0;
    for (size_t s = e + 6; s + 16 < limit; ++s) {
        if (!parseDecl(b, n, s, d)) continue;
        ModelPart tmp;
        size_t ve = parseMesh(b, n, d, tmp);
        if (!ve) continue;
        first = s; part = std::move(tmp); vend = ve;
        break;
    }
    if (!first) return false;

    // material: nama + kunci tekstur
    for (size_t p = e + 6; p < first && m.materials.size() < mc; ++p) {
        if (!isNameChar(b[p]) || (p > 0 && isNameChar(b[p - 1]))) continue;
        size_t q = p;
        while (q < first && isNameChar(b[q])) ++q;
        size_t L = q - p;
        if (L < 3 || L > 40 || q >= n || b[q] != 0) { p = q; continue; }
        ModelMaterial mat;
        mat.name.assign((const char*)b + p, L);
        size_t kp = p + 49 + L;
        if (kp + 4 <= n) mat.texKey = u32(b, kp);
        m.materials.push_back(std::move(mat));
        p = q;
    }

    auto addPart = [&](size_t declStart, ModelPart&& pt) {
        pt.material = declStart >= 4 ? u32(b, declStart - 4) : 0;
        m.parts.push_back(std::move(pt));
    };
    addPart(first, std::move(part));
    // mesh berikutnya: jarak dari akhir vertex bervariasi (10, 12, 47, 72, ... byte), jadi cari decl valid terdekat
    while (m.parts.size() < 512) {
        size_t found = 0, ve = 0;
        ModelPart pt;
        for (size_t s = vend; s < vend + 128 && s + 16 < n; ++s) {
            if (!parseDecl(b, n, s, d)) continue;
            ModelPart tmp;
            size_t v2 = parseMesh(b, n, d, tmp);
            if (!v2) continue;
            found = s; ve = v2; pt = std::move(tmp);
            break;
        }
        if (!found) break;
        addPart(found, std::move(pt));
        vend = ve;
    }

    bool any = false;
    for (auto& pt : m.parts)
        for (size_t i = 0; i + 4 < pt.verts.size() + 1 && i < pt.verts.size(); i += 5)
            for (int a = 0; a < 3; ++a) {
                float v = pt.verts[i + a];
                if (!any) { m.bmin[0] = m.bmax[0] = pt.verts[i]; m.bmin[1] = m.bmax[1] = pt.verts[i + 1]; m.bmin[2] = m.bmax[2] = pt.verts[i + 2]; any = true; }
                if (v < m.bmin[a]) m.bmin[a] = v;
                if (v > m.bmax[a]) m.bmax[a] = v;
            }
    return any;
}

}  // namespace hulk
