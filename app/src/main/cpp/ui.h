// UI: pengaturan (disimpan ke file), font bitmap, kotak/tombol, render target (resolusi internal), layout menu & pengaturan.
#pragma once
#include <SDL.h>
#include <GLES2/gl2.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "gfx.h"
#include "font_data.h"

// ---------------- pengaturan ----------------
enum { ROW_RES, ROW_FPS, ROW_ASPECT, ROW_HZ, ROW_TEX, ROW_SHOWFPS, ROW_VIDEO, ROW_COUNT };

static const char* ROW_NAME[ROW_COUNT] = {"Resolusi", "FPS (Frames per Second)", "Aspect Ratio", "Screen Refresh Rate",
                                          "Kualitas Tekstur", "Tampilkan FPS", "Video Latar"};
static const char* OPT_LABELS[ROW_COUNT][7] = {
    {"Asli", "360p", "480p", "540p", "720p", "900p", "1080p"},
    {"20", "24", "30", "60"},
    {"Isi layar", "16:9"},
    {"Auto", "60 Hz", "90 Hz", "120 Hz"},
    {"Rendah", "Sedang", "Tinggi"},
    {"Mati", "Nyala"},
    {"Mati", "Nyala"}};
static const int OPT_COUNT[ROW_COUNT] = {7, 4, 2, 4, 3, 2, 2};
static const int RES_H[7] = {0, 360, 480, 540, 720, 900, 1080};
static const int FPS_V[4] = {20, 24, 30, 60};
static const float HZ_V[4] = {0.0f, 60.0f, 90.0f, 120.0f};
static const int TEX_V[3] = {128, 256, 512};

struct Settings {
    int v[ROW_COUNT] = {0, 2, 0, 0, 1, 1, 1};  // resolusi asli, 30 FPS, isi layar, auto Hz, tekstur sedang, FPS nyala, video nyala
};

static inline void saveSettings(const Settings& s, const std::string& path) {
    if (path.empty()) return;
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    for (int i = 0; i < ROW_COUNT; ++i) std::fprintf(f, "%d%c", s.v[i], i + 1 < ROW_COUNT ? ',' : '\n');
    std::fclose(f);
}

static inline void loadSettings(Settings& s, const std::string& path) {
    if (path.empty()) return;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return;
    int t[ROW_COUNT];
    int n = 0;
    while (n < ROW_COUNT && std::fscanf(f, "%d,", &t[n]) == 1) ++n;
    std::fclose(f);
    if (n != ROW_COUNT) return;
    for (int i = 0; i < ROW_COUNT; ++i) s.v[i] = std::max(0, std::min(OPT_COUNT[i] - 1, t[i]));
}

// ---------------- program warna solid + teks ----------------
struct UiGl {
    GLuint solid = 0, text = 0;
    GLint uSolidColor = -1, uTextColor = -1, uTextTex = -1;
    Tex2D font;
    bool fontOk = false;
};

static inline void initUi(UiGl& u) {
    const char* fsSolid = "precision mediump float; uniform vec4 uColor; void main(){ gl_FragColor = uColor; }\n";
    const char* fsText =
        "precision mediump float; varying vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor;\n"
        "void main(){ float a = texture2D(uTex, vUV).a; gl_FragColor = vec4(uColor.rgb, uColor.a * a); }\n";
    u.solid = linkProgram(VS_2D, fsSolid);
    u.uSolidColor = glGetUniformLocation(u.solid, "uColor");
    u.text = linkProgram(VS_2D, fsText);
    u.uTextColor = glGetUniformLocation(u.text, "uColor");
    u.uTextTex = glGetUniformLocation(u.text, "uTex");
    u.fontOk = loadAssetPng("font.png", u.font);
}

static inline float ndcX(float x, float W) { return -1.0f + 2.0f * x / W; }
static inline float ndcY(float y, float H) { return 1.0f - 2.0f * y / H; }

static inline void quadUV(float l, float t, float r, float b, float u0, float v0, float u1, float v1) {
    const GLfloat v[] = {l, t, u0, v0, l, b, u0, v1, r, t, u1, v0, r, b, u1, v1};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), v);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), v + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// Kotak berwarna (koordinat piksel layar internal, asal kiri-atas). Blending dikelola pemanggil.
static inline void rectTL(const UiGl& u, float x, float y, float w, float h, float r, float g, float b, float a, float W, float H) {
    glUseProgram(u.solid);
    glUniform4f(u.uSolidColor, r, g, b, a);
    quadUV(ndcX(x, W), ndcY(y, H), ndcX(x + w, W), ndcY(y + h, H), 0, 0, 1, 1);
}

static inline void outlineTL(const UiGl& u, float x, float y, float w, float h, float t, float r, float g, float b, float a, float W, float H) {
    rectTL(u, x, y, w, t, r, g, b, a, W, H);
    rectTL(u, x, y + h - t, w, t, r, g, b, a, W, H);
    rectTL(u, x, y, t, h, r, g, b, a, W, H);
    rectTL(u, x + w - t, y, t, h, r, g, b, a, W, H);
}

// ---------------- teks ----------------
static inline float textWidth(const char* s, float px) {
    float sc = px / FONT_PX, w = 0.0f;
    for (; *s; ++s) {
        int c = (unsigned char)*s - FONT_FIRST;
        if (c < 0 || c >= FONT_COUNT) c = '?' - FONT_FIRST;
        w += FONT_ADV[c] * sc;
    }
    return w;
}

// top = tepi atas sel teks. Gunakan drawTextCenter untuk tengah-vertikal.
static inline void drawText(const UiGl& u, const char* s, float x, float top, float px, float r, float g, float b, float a, float W, float H) {
    if (!u.fontOk) return;
    glUseProgram(u.text);
    glUniform1i(u.uTextTex, 0);
    glUniform4f(u.uTextColor, r, g, b, a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, u.font.id);
    float sc = px / FONT_PX;
    for (; *s; ++s) {
        int c = (unsigned char)*s - FONT_FIRST;
        if (c < 0 || c >= FONT_COUNT) c = '?' - FONT_FIRST;
        if (*s != ' ') {
            int col = c % FONT_COLS, row = c / FONT_COLS;
            float u0 = (float)(col * FONT_CELL_W) / FONT_ATLAS_W, v0 = (float)(row * FONT_CELL_H) / FONT_ATLAS_H;
            float u1 = (float)((col + 1) * FONT_CELL_W) / FONT_ATLAS_W, v1 = (float)((row + 1) * FONT_CELL_H) / FONT_ATLAS_H;
            quadUV(ndcX(x - FONT_PAD_X * sc, W), ndcY(top, H), ndcX(x - FONT_PAD_X * sc + FONT_CELL_W * sc, W),
                   ndcY(top + FONT_CELL_H * sc, H), u0, v0, u1, v1);
        }
        x += FONT_ADV[c] * sc;
    }
}

static inline void drawTextCenter(const UiGl& u, const char* s, float cx, float cy, float px, float r, float g, float b, float a, float W, float H) {
    float sc = px / FONT_PX;
    drawText(u, s, cx - textWidth(s, px) * 0.5f, cy - (FONT_ASCENT - 0.35f * FONT_PX) * sc, px, r, g, b, a, W, H);
}

// ---------------- tombol ----------------
struct Btn {
    float x = 0, y = 0, w = 0, h = 0;
    int row = 0, opt = 0;
    std::string label;
};

static inline int hitTest(const std::vector<Btn>& v, float x, float y) {
    for (size_t i = 0; i < v.size(); ++i)
        if (x >= v[i].x && x <= v[i].x + v[i].w && y >= v[i].y && y <= v[i].y + v[i].h) return (int)i;
    return -1;
}

static inline void drawButton(const UiGl& u, const Btn& b, bool selected, bool focused, float W, float H) {
    float t = std::max(2.0f, H * 0.004f);
    if (selected) rectTL(u, b.x, b.y, b.w, b.h, 0.72f, 0.07f, 0.11f, 1.0f, W, H);
    else rectTL(u, b.x, b.y, b.w, b.h, 0.07f, 0.07f, 0.09f, 0.88f, W, H);
    outlineTL(u, b.x, b.y, b.w, b.h, t, selected ? 0.95f : 0.55f, selected ? 0.25f : 0.55f, selected ? 0.28f : 0.60f, 1.0f, W, H);
    if (focused) outlineTL(u, b.x - t * 2, b.y - t * 2, b.w + t * 4, b.h + t * 4, t * 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
    drawTextCenter(u, b.label.c_str(), b.x + b.w * 0.5f, b.y + b.h * 0.5f, b.h * 0.50f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
}

enum { MENU_MODELS, MENU_TEXTURES, MENU_SETTINGS, MENU_QUIT, MENU_COUNT };
static const char* MENU_LABEL[MENU_COUNT] = {"MODEL 3D", "TEKSTUR", "PENGATURAN", "KELUAR"};

static inline void layoutMenu(std::vector<Btn>& out, float W, float H) {
    out.clear();
    float bw = W * 0.28f, bh = H * 0.085f, x = W * 0.03f + (W * 0.39f - bw) * 0.5f, y = H * 0.40f, gap = H * 0.022f;
    for (int i = 0; i < MENU_COUNT; ++i) {
        Btn b;
        b.x = x; b.y = y + i * (bh + gap); b.w = bw; b.h = bh; b.row = i; b.opt = 0; b.label = MENU_LABEL[i];
        out.push_back(b);
    }
}

static inline void layoutSettings(std::vector<Btn>& out, float W, float H) {
    out.clear();
    float px = H * 0.034f, pad = W * 0.016f, gap = W * 0.010f;
    for (int r = 0; r < ROW_COUNT; ++r) {
        float y0 = H * 0.16f + r * H * 0.108f, x = W * 0.06f;
        for (int o = 0; o < OPT_COUNT[r]; ++o) {
            Btn b;
            b.label = OPT_LABELS[r][o];
            b.w = textWidth(b.label.c_str(), px) + pad * 2.0f;
            b.h = H * 0.062f; b.x = x; b.y = y0 + H * 0.040f; b.row = r; b.opt = o;
            x += b.w + gap;
            out.push_back(b);
        }
    }
    Btn back;
    back.x = W * 0.78f; back.y = H * 0.88f; back.w = W * 0.19f; back.h = H * 0.075f; back.row = ROW_COUNT; back.opt = 0; back.label = "KEMBALI";
    out.push_back(back);
}

// ---------------- render target (resolusi internal lebih kecil, lalu diperbesar ke layar) ----------------
struct RenderTarget {
    GLuint fbo = 0, tex = 0, depth = 0;
    int w = 0, h = 0;

    void destroy() {
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (tex) glDeleteTextures(1, &tex);
        if (depth) glDeleteRenderbuffers(1, &depth);
        fbo = tex = depth = 0; w = h = 0;
    }

    bool ensure(int W, int H) {
        if (fbo && W == w && H == h) return true;
        destroy();
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (!ok) { SDL_Log("FBO %dx%d gagal, pakai resolusi asli", W, H); destroy(); return false; }
        w = W; h = H;
        return true;
    }

    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }
    static void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
};
