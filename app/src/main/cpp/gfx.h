// Helper grafis: shader, quad, loader PNG dari assets APK.
#pragma once
#include <SDL.h>
#include <GLES2/gl2.h>
#include <string>
#include <vector>
#include "stb_image.h"

struct Tex2D {
    GLuint id = 0;
    int w = 0, h = 0;
};

static inline GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("Shader error: %s", log);
    }
    return s;
}

// Atribut 0 = aPos, 1 = aUV (konvensi seluruh engine)
static inline GLuint linkProgram(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compileShader(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compileShader(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aUV");
    glLinkProgram(p);
    return p;
}

static const char* VS_2D =
    "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;\n"
    "void main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n";
static const char* FS_2D =
    "precision mediump float; varying vec2 vUV; uniform sampler2D uTex;\n"
    "void main(){ gl_FragColor = texture2D(uTex, vUV); }\n";

// Quad dalam koordinat NDC (l,t,r,b). flipV=true untuk tekstur video (asal koordinat di kiri-bawah).
static inline void drawQuad(float l, float t, float r, float b, bool flipV = false) {
    float vt = flipV ? 1.0f : 0.0f, vb = flipV ? 0.0f : 1.0f;
    const GLfloat v[] = {
        l, t, 0.0f, vt,
        l, b, 0.0f, vb,
        r, t, 1.0f, vt,
        r, b, 1.0f, vb,
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), v);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), v + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Gambar di posisi piksel (asal kiri-atas), program 2D harus sedang aktif.
static inline void drawImage(const Tex2D& t, float left, float top, float w, float h, int W, int H) {
    if (!t.id) return;
    glBindTexture(GL_TEXTURE_2D, t.id);
    drawQuad(-1.0f + 2.0f * left / W, 1.0f - 2.0f * top / H,
             -1.0f + 2.0f * (left + w) / W, 1.0f - 2.0f * (top + h) / H);
}

// Decode PNG dari memori menjadi tekstur GL.
static inline bool loadPngMem(const unsigned char* data, size_t len, Tex2D& t) {
    int w = 0, h = 0, c = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)len, &w, &h, &c, 4);
    if (!px) { SDL_Log("Decode PNG gagal"); return false; }
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    t.w = w; t.h = h;
    return true;
}

// Muat PNG dari assets APK (app/src/main/assets/<name>).
static inline bool loadAssetPng(const char* name, Tex2D& t) {
    SDL_RWops* rw = SDL_RWFromFile(name, "rb");
    if (!rw) { SDL_Log("Asset tidak ada: %s", name); return false; }
    Sint64 sz = SDL_RWsize(rw);
    if (sz <= 0) { SDL_RWclose(rw); return false; }
    std::vector<unsigned char> buf((size_t)sz);
    size_t got = SDL_RWread(rw, buf.data(), 1, (size_t)sz);
    SDL_RWclose(rw);
    if (got != (size_t)sz) return false;
    return loadPngMem(buf.data(), buf.size(), t);
}
