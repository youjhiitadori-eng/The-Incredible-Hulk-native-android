// Viewer model 3D: baca MODELS.ARC, tampilkan model dengan tekstur aslinya (kamera orbit).
#pragma once
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include "arc.h"
#include "dds.h"
#include "model.h"
#include "gfx.h"
#include "math3d.h"

struct ModelViewer {
    hulk::ArcIndex* index = nullptr;
    const hulk::ArcSection* models = nullptr;
    const hulk::ArcSection* texs = nullptr;
    std::unordered_map<uint32_t, size_t> texByKey;
    std::unordered_map<size_t, GLuint> texCache;
    hulk::Model cur;
    size_t curIdx = 0;
    float center[3] = {0, 0, 0}, radius = 1.0f;
    float yaw = 0.6f, pitch = 0.25f, zoom = 1.0f;
    int maxTexDim = 256;  // sisi tekstur maksimum untuk model (hemat RAM)
    bool ready = false;
    GLuint prog = 0, white = 0;
    GLint uMvp = -1, uTex = -1;

    bool init(hulk::ArcIndex* idx) {
        index = idx;
        models = idx->find("Models");
        texs = idx->find("Textures");
        if (!models || models->entries.empty() || !texs) { SDL_Log("Section Models/Textures tidak ada"); return false; }
        for (size_t k = 0; k < texs->entries.size(); ++k) texByKey[texs->entries[k].key] = k;

        const char* vs =
            "attribute vec3 aPos; attribute vec2 aUV; uniform mat4 uMVP; varying vec2 vUV;\n"
            "void main(){ gl_Position = uMVP * vec4(aPos, 1.0); vUV = aUV; }\n";
        const char* fs =
            "precision mediump float; varying vec2 vUV; uniform sampler2D uTex;\n"
            "void main(){ vec4 c = texture2D(uTex, vUV); if (c.a < 0.4) discard; gl_FragColor = vec4(c.rgb, 1.0); }\n";
        prog = linkProgram(vs, fs);
        uMvp = glGetUniformLocation(prog, "uMVP");
        uTex = glGetUniformLocation(prog, "uTex");

        const unsigned char px[4] = {200, 200, 200, 255};
        glGenTextures(1, &white);
        glBindTexture(GL_TEXTURE_2D, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

        ready = true;
        curIdx = 0;
        return step(1, true);
    }

    void freeTex() {
        for (auto& kv : texCache) glDeleteTextures(1, &kv.second);
        texCache.clear();
    }

    bool load(size_t i) {
        std::vector<uint8_t> raw;
        hulk::Model m;
        if (!index->readEntry(*models, models->entries[i], raw)) return false;
        if (!hulk::parseModel(raw.data(), raw.size(), m) || m.parts.empty()) return false;
        cur = std::move(m);
        curIdx = i;
        freeTex();
        float dx = cur.bmax[0] - cur.bmin[0], dy = cur.bmax[1] - cur.bmin[1], dz = cur.bmax[2] - cur.bmin[2];
        for (int a = 0; a < 3; ++a) center[a] = 0.5f * (cur.bmin[a] + cur.bmax[a]);
        radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
        if (radius < 0.01f) radius = 1.0f;
        zoom = 1.0f;
        size_t tris = 0;
        for (auto& p : cur.parts) tris += p.indices.size() / 3;
        SDL_Log("Model #%zu '%s': %zu part, %zu segitiga", i, cur.name.c_str(), cur.parts.size(), tris);
        return true;
    }

    // dir = +1 / -1. Model tanpa geometri dilewati. first=true: mulai dari curIdx tanpa maju dulu.
    bool step(int dir, bool first = false) {
        size_t n = models->entries.size();
        size_t i = curIdx;
        for (size_t tries = 0; tries < n; ++tries) {
            if (!(first && tries == 0)) i = (i + n + (dir < 0 ? n - 1 : 1)) % n;
            if (load(i)) return true;
        }
        return false;
    }

    GLuint textureFor(const hulk::ModelPart& p) {
        if (p.material >= cur.materials.size()) return white;
        auto it = texByKey.find(cur.materials[p.material].texKey);
        if (it == texByKey.end()) return white;
        auto cit = texCache.find(it->second);
        if (cit != texCache.end()) return cit->second;
        GLuint id = white;
        std::vector<uint8_t> raw;
        hulk::Image img;
        if (index->readEntry(*texs, texs->entries[it->second], raw) &&
            hulk::decodeTexture(raw.data(), raw.size(), maxTexDim, img)) {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
            texCache[it->second] = id;
        }
        return id;
    }

    // Pemanggil sudah membersihkan color + depth. Program 2D harus dipasang ulang setelah ini.
    void draw(int w, int h) {
        if (!ready || w <= 0 || h <= 0) return;
        float dist = radius * 2.4f / zoom;
        float mvp[16];
        m3::orbitMvp((float)w / (float)h, dist, yaw, pitch, center, radius, mvp);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glUseProgram(prog);
        glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp);
        glUniform1i(uTex, 0);
        glActiveTexture(GL_TEXTURE0);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        for (const auto& p : cur.parts) {
            glBindTexture(GL_TEXTURE_2D, textureFor(p));
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), p.verts.data());
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), p.verts.data() + 3);
            glDrawElements(GL_TRIANGLES, (GLsizei)p.indices.size(), GL_UNSIGNED_SHORT, p.indices.data());
        }
        glDisable(GL_DEPTH_TEST);
    }
};
