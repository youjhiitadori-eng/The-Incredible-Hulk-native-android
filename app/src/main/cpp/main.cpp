// Hulk port - milestone 4: menu + pengaturan (resolusi/FPS/aspect/refresh rate/kualitas), viewer model 3D, viewer tekstur.
// Data game dicari di: <app external files>/data lalu /sdcard/Hulk/data  (butuh index.ind, TEXTURES.ARC, MODELS.ARC)
// Video latar (opsional, mp4 H.264): bg.mp4 di <app external files>/ atau /sdcard/Hulk/
#include <SDL.h>
#include <GLES2/gl2.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "arc.h"
#include "dds.h"
#include "gfx.h"
#include "video.h"
#include "mview.h"
#include "ui.h"

static const char* CREATED_BY = "Created by why01";  // ganti sesuai nama lu

// ---------- overlay FPS (scissor + glClear, koordinat layar fisik) ----------
static void fillRect(int x, int y, int w, int h, float r, float g, float b) {
    glScissor(x, y, w, h);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
static const uint8_t SEG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static void drawDigit(int x, int y, int u, int d) {
    uint8_t m = SEG[d % 10];
    const float R = 0.2f, G = 1.0f, B = 0.2f;
    if (m & 0x01) fillRect(x,         y + 6 * u, 4 * u, u,     R, G, B);
    if (m & 0x02) fillRect(x + 3 * u, y + 3 * u, u,     4 * u, R, G, B);
    if (m & 0x04) fillRect(x + 3 * u, y,         u,     4 * u, R, G, B);
    if (m & 0x08) fillRect(x,         y,         4 * u, u,     R, G, B);
    if (m & 0x10) fillRect(x,         y,         u,     4 * u, R, G, B);
    if (m & 0x20) fillRect(x,         y + 3 * u, u,     4 * u, R, G, B);
    if (m & 0x40) fillRect(x,         y + 3 * u, 4 * u, u,     R, G, B);
}
static void drawFps(int fps, int screenH) {
    if (fps > 999) fps = 999;
    int u = screenH / 100;
    if (u < 2) u = 2;
    int x0 = u * 2, y0 = screenH - 10 * u;
    fillRect(x0 - u, y0 - u, 16 * u, 9 * u, 0.0f, 0.0f, 0.0f);
    int digits[3] = {fps / 100 % 10, fps / 10 % 10, fps % 10};
    bool started = false;
    int pos = 0;
    for (int i = 0; i < 3; ++i) {
        if (digits[i] != 0 || started || i == 2) {
            started = true;
            drawDigit(x0 + pos * 5 * u, y0, u, digits[i]);
            ++pos;
        }
    }
}

// ---------- viewer tekstur ----------
struct Viewer {
    hulk::ArcIndex index;
    const hulk::ArcSection* tex = nullptr;
    size_t cur = 0;
    GLuint glTex = 0;
    int texW = 0, texH = 0;
    int maxDim = 256;
    bool ready = false;
    std::string name;

    bool init(const std::string& dir) {
        std::string err;
        if (!index.load(dir, &err)) { SDL_Log("Index gagal (%s): %s", dir.c_str(), err.c_str()); return false; }
        tex = index.find("Textures");
        if (!tex || tex->entries.empty()) { SDL_Log("Section Textures kosong"); return false; }
        SDL_Log("Data OK di %s: %zu section, %zu tekstur", dir.c_str(), index.sections.size(), tex->entries.size());
        ready = true;
        return step(0);
    }

    bool load(size_t i) {
        std::vector<uint8_t> raw;
        hulk::Image img;
        if (!index.readEntry(*tex, tex->entries[i], raw)) return false;
        if (!hulk::decodeTexture(raw.data(), raw.size(), maxDim * 2, img)) return false;
        if (!glTex) glGenTextures(1, &glTex);
        glBindTexture(GL_TEXTURE_2D, glTex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
        texW = img.w; texH = img.h;
        name = index.entryName(*tex, tex->entries[i]);
        SDL_Log("Tekstur #%zu '%s' %dx%d", i, name.c_str(), texW, texH);
        return true;
    }

    // dir = +1 / -1 / 0 (muat entry saat ini). Entry yang gagal di-decode dilewati.
    bool step(int dir) {
        size_t n = tex->entries.size();
        for (size_t tries = 0; tries < n; ++tries) {
            if (dir != 0 || tries > 0) cur = (cur + n + (dir < 0 ? n - 1 : 1)) % n;
            if (load(cur)) return true;
        }
        return false;
    }

    void draw(int screenW, int screenH) {
        if (!ready || !glTex || screenW <= 0 || screenH <= 0) return;
        float ia = (float)texW / (float)texH, sa = (float)screenW / (float)screenH;
        float sx = 0.92f, sy = 0.92f;
        if (ia > sa) sy *= sa / ia; else sx *= ia / sa;
        glBindTexture(GL_TEXTURE_2D, glTex);
        drawQuad(-sx, sy, sx, -sy);
    }
};

enum class Scene { Menu, Settings, Models, Textures };

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init gagal: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    SDL_Window* win = SDL_CreateWindow("Hulk", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
    if (!win) { SDL_Log("CreateWindow gagal: %s", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { SDL_Log("GL context gagal: %s", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    SDL_Log("GL: %s", (const char*)glGetString(GL_VERSION));

    GLuint prog2d = linkProgram(VS_2D, FS_2D);
    GLint uTex2d = glGetUniformLocation(prog2d, "uTex");
    glUseProgram(prog2d);
    glUniform1i(uTex2d, 0);
    glActiveTexture(GL_TEXTURE0);

    UiGl ui;
    initUi(ui);
    Tex2D pp, title;
    loadAssetPng("pp.png", pp);
    loadAssetPng("title.png", title);

    // pengaturan tersimpan
    const char* ext = SDL_AndroidGetExternalStoragePath();
    std::string extDir = ext ? ext : "";
    std::string settingsPath = extDir.empty() ? "" : extDir + "/settings.cfg";
    Settings st;
    loadSettings(st, settingsPath);

    // data game
    Viewer viewer;
    std::string d1 = extDir + "/data";
    SDL_Log("Taruh data game di: %s  atau  /sdcard/Hulk/data", d1.c_str());
    if (!viewer.init(d1)) viewer.init("/sdcard/Hulk/data");
    ModelViewer mview;
    if (viewer.ready) mview.init(&viewer.index);
    viewer.maxDim = mview.maxTexDim = TEX_V[st.v[ROW_TEX]];

    // video latar (opsional)
    VideoBg video;
    std::string videoPath;
    {
        const std::string cands[] = {extDir + "/bg.mp4", "/sdcard/Hulk/bg.mp4", "asset:bg.mp4"};  // file luar menimpa yang di dalam APK
        for (const auto& c : cands) if (video.fileExists(c)) { videoPath = c; break; }
        if (videoPath.empty()) SDL_Log("Video latar tidak ada. Taruh bg.mp4 di app/src/main/assets/ (masuk APK) atau %s/ atau /sdcard/Hulk/", extDir.c_str());
    }
    if (st.v[ROW_VIDEO]) video.start(videoPath);
    if (st.v[ROW_HZ]) video.setRefresh(HZ_V[st.v[ROW_HZ]]);

    // gamepad (PS4/PS5/Xbox/apa pun yang dikenali SDL)
    SDL_GameController* pad = nullptr;
    auto openPad = [&](int idx) {
        if (!pad && SDL_IsGameController(idx)) {
            pad = SDL_GameControllerOpen(idx);
            if (pad) SDL_Log("Gamepad terhubung: %s", SDL_GameControllerName(pad));
        }
    };
    for (int i = 0; i < SDL_NumJoysticks(); ++i) openPad(i);

    Scene scene = Scene::Menu;
    bool running = true, paused = false, padActive = false;
    int menuFocus = 0, setRow = 0, setOpt = st.v[0];
    std::vector<Btn> menuBtns, setBtns;
    RenderTarget rt;

    // geometri frame terakhir (untuk memetakan sentuhan ke koordinat UI)
    int sw = 1, sh = 1, vx = 0, vy = 0, vw = 1, vh = 1;
    float W = 1.0f, H = 1.0f;

    auto goMenu = [&]() { scene = Scene::Menu; if (st.v[ROW_VIDEO] && !video.active) video.start(videoPath); };
    auto goSettings = [&]() { scene = Scene::Settings; setRow = 0; setOpt = st.v[0]; };
    auto goModels = [&]() { scene = Scene::Models; video.stop(); };
    auto goTextures = [&]() { scene = Scene::Textures; video.stop(); };
    auto stepCur = [&](int d) {
        if (scene == Scene::Models && mview.ready) mview.step(d);
        else if (scene == Scene::Textures && viewer.ready) viewer.step(d);
    };
    auto toggleViewer = [&]() { if (scene == Scene::Models) goTextures(); else goModels(); };
    auto menuAction = [&](int i) {
        if (i == MENU_MODELS) goModels();
        else if (i == MENU_TEXTURES) goTextures();
        else if (i == MENU_SETTINGS) goSettings();
        else if (i == MENU_QUIT) running = false;
    };
    auto setOption = [&](int row, int opt) {
        st.v[row] = opt;
        if (row == ROW_TEX) viewer.maxDim = mview.maxTexDim = TEX_V[opt];
        else if (row == ROW_HZ) video.setRefresh(HZ_V[opt]);
        else if (row == ROW_VIDEO) { if (!opt) video.stop(); else video.start(videoPath); }
        saveSettings(st, settingsPath);
    };
    auto toLocal = [&](float nx, float ny, float& lx, float& ly) {
        float px = nx * sw, py = ny * sh, top = (float)(sh - (vy + vh));
        lx = (px - vx) / vw * W;
        ly = (py - top) / vh * H;
        return lx >= 0 && ly >= 0 && lx <= W && ly <= H;
    };
    int stickDir = 0;
    float downX = 0.0f, downY = 0.0f, moved = 0.0f;
    uint32_t downT = 0, lastTouchT = 0;

    int frames = 0, fps = 0;
    uint32_t fpsTimer = SDL_GetTicks(), lastFrame = SDL_GetTicks();

    while (running) {
        uint32_t start = SDL_GetTicks();
        float dt = (float)(start - lastFrame) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastFrame = start;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_APP_WILLENTERBACKGROUND: paused = true; break;
                case SDL_APP_DIDENTERFOREGROUND: paused = false; break;
                case SDL_FINGERDOWN:
                    downX = e.tfinger.x; downY = e.tfinger.y; moved = 0.0f; downT = SDL_GetTicks(); lastTouchT = downT;
                    padActive = false;
                    break;
                case SDL_FINGERMOTION:
                    moved += std::fabs(e.tfinger.dx) + std::fabs(e.tfinger.dy);
                    lastTouchT = SDL_GetTicks();
                    if (scene == Scene::Models && mview.ready && moved > 0.02f) {
                        mview.yaw += e.tfinger.dx * 4.0f;
                        mview.pitch += e.tfinger.dy * 4.0f;
                        if (mview.pitch > 1.4f) mview.pitch = 1.4f;
                        if (mview.pitch < -1.4f) mview.pitch = -1.4f;
                    }
                    break;
                case SDL_FINGERUP:
                    if (moved < 0.03f && SDL_GetTicks() - downT < 400) {  // ketukan
                        float lx = 0, ly = 0;
                        bool inside = toLocal(downX, downY, lx, ly);
                        if (scene == Scene::Menu) {
                            int h = inside ? hitTest(menuBtns, lx, ly) : -1;
                            if (h >= 0) { menuFocus = h; menuAction(menuBtns[h].row); }
                        } else if (scene == Scene::Settings) {
                            int h = inside ? hitTest(setBtns, lx, ly) : -1;
                            if (h >= 0) {
                                if (setBtns[h].row == ROW_COUNT) goMenu();
                                else { setRow = setBtns[h].row; setOpt = setBtns[h].opt; setOption(setRow, setOpt); }
                            }
                        } else {
                            bool dataOk = (scene == Scene::Models) ? mview.ready : viewer.ready;
                            if (!dataOk) goMenu();
                            else if (downX < 0.33f) stepCur(-1);
                            else if (downX > 0.67f) stepCur(1);
                            else toggleViewer();
                        }
                    }
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) {
                        if (scene == Scene::Menu) running = false; else goMenu();
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                        SDL_GameControllerClose(pad);
                        pad = nullptr;
                        SDL_Log("Gamepad dicabut");
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN: {
                    int b = e.cbutton.button;
                    padActive = true;
                    SDL_Log("Gamepad tombol %d", b);
                    if (scene == Scene::Menu) {
                        if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) menuFocus = (menuFocus + MENU_COUNT - 1) % MENU_COUNT;
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) menuFocus = (menuFocus + 1) % MENU_COUNT;
                        else if (b == SDL_CONTROLLER_BUTTON_A) menuAction(menuFocus);   // A = tombol X (silang) PS
                        else if (b == SDL_CONTROLLER_BUTTON_START) goModels();
                    } else if (scene == Scene::Settings) {
                        if (b == SDL_CONTROLLER_BUTTON_B) goMenu();                      // B = bulat PS
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) setRow = std::max(0, setRow - 1);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) setRow = std::min((int)ROW_COUNT, setRow + 1);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT && setRow < ROW_COUNT) setOpt = std::max(0, setOpt - 1);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && setRow < ROW_COUNT) setOpt = std::min(OPT_COUNT[setRow] - 1, setOpt + 1);
                        else if (b == SDL_CONTROLLER_BUTTON_A) {
                            if (setRow == ROW_COUNT) goMenu(); else setOption(setRow, setOpt);
                        }
                        if (setRow < ROW_COUNT) setOpt = std::min(setOpt, OPT_COUNT[setRow] - 1); else setOpt = 0;
                    } else {
                        bool dataOk = (scene == Scene::Models) ? mview.ready : viewer.ready;
                        if (b == SDL_CONTROLLER_BUTTON_B) goMenu();
                        else if (!dataOk) { if (b == SDL_CONTROLLER_BUTTON_A) goMenu(); }
                        else if (b == SDL_CONTROLLER_BUTTON_Y) toggleViewer();           // Y = segitiga PS
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT || b == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) stepCur(-1);
                        else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) stepCur(1);
                    }
                    break;
                }
                case SDL_CONTROLLERAXISMOTION:
                    if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX && (scene == Scene::Models || scene == Scene::Textures)) {
                        int d = e.caxis.value > 20000 ? 1 : (e.caxis.value < -20000 ? -1 : 0);
                        if (d != 0 && d != stickDir) stepCur(d);
                        stickDir = d;
                    }
                    break;
                default: break;
            }
        }
        if (paused) { SDL_Delay(100); continue; }

        // kamera model: putar otomatis + stik kanan/kiri
        if (scene == Scene::Models && mview.ready) {
            if (SDL_GetTicks() - lastTouchT > 2000) mview.yaw += dt * 0.6f;
            if (pad) {
                auto ax = [&](int a) { float v = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)a) / 32768.0f; return std::fabs(v) < 0.2f ? 0.0f : v; };
                mview.yaw += ax(SDL_CONTROLLER_AXIS_RIGHTX) * dt * 2.5f;
                mview.pitch += ax(SDL_CONTROLLER_AXIS_RIGHTY) * dt * 2.0f;
                if (mview.pitch > 1.4f) mview.pitch = 1.4f;
                if (mview.pitch < -1.4f) mview.pitch = -1.4f;
                mview.zoom *= 1.0f - ax(SDL_CONTROLLER_AXIS_LEFTY) * dt * 1.2f;
                if (mview.zoom < 0.4f) mview.zoom = 0.4f;
                if (mview.zoom > 4.0f) mview.zoom = 4.0f;
            }
        }

        // ---------- geometri: viewport (aspect) + resolusi internal ----------
        SDL_GL_GetDrawableSize(win, &sw, &sh);
        if (sw < 2 || sh < 2) { SDL_Delay(16); continue; }
        vx = 0; vy = 0; vw = sw; vh = sh;
        if (st.v[ROW_ASPECT] == 1) {  // 16:9 dengan bar hitam
            if ((long)sw * 9 > (long)sh * 16) { vw = sh * 16 / 9; vx = (sw - vw) / 2; }
            else { vh = sw * 9 / 16; vy = (sh - vh) / 2; }
        }
        int iw = vw, ih = vh;
        if (RES_H[st.v[ROW_RES]] > 0 && RES_H[st.v[ROW_RES]] < vh) {
            ih = RES_H[st.v[ROW_RES]];
            iw = (int)((float)ih * vw / vh + 0.5f);
            if (iw < 2) iw = 2;
        }
        bool useFbo = (iw != vw || ih != vh);
        if (useFbo && !rt.ensure(iw, ih)) { useFbo = false; iw = vw; ih = vh; }
        W = (float)iw; H = (float)ih;

        float t = (float)SDL_GetTicks() / 1000.0f;

        // layar penuh hitam (termasuk bar)
        RenderTarget::unbind();
        glViewport(0, 0, sw, sh);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (useFbo) { rt.bind(); glViewport(0, 0, iw, ih); }
        else glViewport(vx, vy, vw, vh);
        glEnable(GL_SCISSOR_TEST);
        if (useFbo) glScissor(0, 0, iw, ih); else glScissor(vx, vy, vw, vh);

        auto bg = [&](float r, float g, float b) {
            glClearColor(r, g, b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        };

        if (scene == Scene::Menu || scene == Scene::Settings) {
            video.update();
            bg(0.04f + 0.03f * sinf(t * 0.5f), 0.10f + 0.05f * sinf(t * 0.7f + 1.0f), 0.07f + 0.04f * sinf(t * 0.3f + 2.0f));
            glDisable(GL_SCISSOR_TEST);
            video.draw(0.55f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            float lb = std::max(2.0f, H * 0.004f);

            if (scene == Scene::Menu) {
                rectTL(ui, W * 0.03f, H * 0.05f, W * 0.39f, H * 0.90f, 0.04f, 0.04f, 0.06f, 0.72f, W, H);
                outlineTL(ui, W * 0.03f, H * 0.05f, W * 0.39f, H * 0.90f, lb, 0.45f, 0.07f, 0.09f, 1.0f, W, H);
                glUseProgram(prog2d);
                glActiveTexture(GL_TEXTURE0);
                float tw = W * 0.31f, th = title.w ? tw * title.h / title.w : 0.0f;
                drawImage(title, W * 0.03f + (W * 0.39f - tw) * 0.5f, H * 0.09f, tw, th, (int)W, (int)H);
                layoutMenu(menuBtns, W, H);
                for (size_t i = 0; i < menuBtns.size(); ++i)
                    drawButton(ui, menuBtns[i], false, padActive && (int)i == menuFocus, W, H);
                float cx = W * 0.03f + W * 0.39f * 0.5f;
                drawTextCenter(ui, CREATED_BY, cx, H * 0.865f, H * 0.032f, 0.92f, 0.92f, 0.95f, 1.0f, W, H);
                drawTextCenter(ui, "Unofficial fan project.", cx, H * 0.912f, H * 0.022f, 0.65f, 0.65f, 0.70f, 1.0f, W, H);
                drawTextCenter(ui, "The Incredible Hulk belongs to its respective owners.", cx, H * 0.940f, H * 0.020f, 0.65f, 0.65f, 0.70f, 1.0f, W, H);
                float ps = H * 0.22f, bob = sinf(t * 1.5f) * H * 0.008f;
                glUseProgram(prog2d);
                drawImage(pp, W - ps - W * 0.04f, H * 0.06f + bob, ps, ps, (int)W, (int)H);
            } else {
                rectTL(ui, W * 0.03f, H * 0.03f, W * 0.94f, H * 0.94f, 0.04f, 0.04f, 0.06f, 0.80f, W, H);
                outlineTL(ui, W * 0.03f, H * 0.03f, W * 0.94f, H * 0.94f, lb, 0.45f, 0.07f, 0.09f, 1.0f, W, H);
                drawText(ui, "PENGATURAN", W * 0.06f, H * 0.045f, H * 0.065f, 0.85f, 0.10f, 0.14f, 1.0f, W, H);
                for (int r = 0; r < ROW_COUNT; ++r)
                    drawText(ui, ROW_NAME[r], W * 0.06f, H * 0.16f + r * H * 0.108f, H * 0.032f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
                layoutSettings(setBtns, W, H);
                for (const Btn& b : setBtns) {
                    bool sel = b.row < ROW_COUNT && st.v[b.row] == b.opt;
                    bool foc = padActive && b.row == setRow && b.opt == (setRow < ROW_COUNT ? setOpt : 0);
                    drawButton(ui, b, sel, foc, W, H);
                }
                drawText(ui, "Hz dan resolusi menyesuaikan HP. Perubahan disimpan otomatis.", W * 0.06f, H * 0.935f, H * 0.022f, 0.65f, 0.65f, 0.70f, 1.0f, W, H);
            }
            glDisable(GL_BLEND);
        } else if (scene == Scene::Models) {
            if (mview.ready) bg(0.10f, 0.11f, 0.14f); else bg(0.45f, 0.05f, 0.05f);  // merah = data tidak ditemukan
            mview.draw((int)W, (int)H);
            glDisable(GL_SCISSOR_TEST);
            if (mview.ready) {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "%s   (%zu part)", mview.cur.name.c_str(), mview.cur.parts.size());
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                drawText(ui, buf, W * 0.03f, H * 0.90f, H * 0.032f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
                drawText(ui, "Ketuk kiri/kanan: ganti model | tengah: Model/Tekstur | geser: putar | Back: menu", W * 0.03f, H * 0.945f, H * 0.022f, 0.70f, 0.70f, 0.75f, 1.0f, W, H);
                glDisable(GL_BLEND);
            } else {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                drawTextCenter(ui, "Data tidak ditemukan: index.ind, TEXTURES.ARC, MODELS.ARC", W * 0.5f, H * 0.5f, H * 0.035f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
                drawTextCenter(ui, "Taruh di /sdcard/Hulk/data/  (ketuk untuk kembali)", W * 0.5f, H * 0.56f, H * 0.028f, 0.9f, 0.9f, 0.9f, 1.0f, W, H);
                glDisable(GL_BLEND);
            }
        } else {
            if (viewer.ready) bg(0.12f, 0.12f, 0.14f); else bg(0.45f, 0.05f, 0.05f);
            glDisable(GL_SCISSOR_TEST);
            glUseProgram(prog2d);
            glActiveTexture(GL_TEXTURE0);
            viewer.draw((int)W, (int)H);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if (viewer.ready) {
                char buf[160];
                std::snprintf(buf, sizeof(buf), "%s   %dx%d", viewer.name.c_str(), viewer.texW, viewer.texH);
                drawText(ui, buf, W * 0.03f, H * 0.90f, H * 0.032f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
                drawText(ui, "Ketuk kiri/kanan: ganti tekstur | tengah: Model/Tekstur | Back: menu", W * 0.03f, H * 0.945f, H * 0.022f, 0.70f, 0.70f, 0.75f, 1.0f, W, H);
            } else {
                drawTextCenter(ui, "Data tidak ditemukan. Taruh di /sdcard/Hulk/data/  (ketuk untuk kembali)", W * 0.5f, H * 0.5f, H * 0.032f, 1.0f, 1.0f, 1.0f, 1.0f, W, H);
            }
            glDisable(GL_BLEND);
        }

        // perbesar hasil render internal ke layar
        if (useFbo) {
            RenderTarget::unbind();
            glDisable(GL_SCISSOR_TEST);
            glViewport(vx, vy, vw, vh);
            glUseProgram(prog2d);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rt.tex);
            drawQuad(-1.0f, 1.0f, 1.0f, -1.0f, true);
        }

        if (st.v[ROW_SHOWFPS]) {
            glViewport(0, 0, sw, sh);
            glEnable(GL_SCISSOR_TEST);
            drawFps(fps, sh);
            glDisable(GL_SCISSOR_TEST);
        }
        SDL_GL_SwapWindow(win);

        ++frames;
        uint32_t now = SDL_GetTicks();
        if (now - fpsTimer >= 1000) { fps = frames; frames = 0; fpsTimer = now; }
        uint32_t frameMs = 1000u / (uint32_t)FPS_V[st.v[ROW_FPS]];
        uint32_t spent = SDL_GetTicks() - start;
        if (spent < frameMs) SDL_Delay(frameMs - spent);
    }

    video.stop();
    rt.destroy();
    if (pad) SDL_GameControllerClose(pad);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
