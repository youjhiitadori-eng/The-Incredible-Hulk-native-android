// Video latar (loop, tanpa suara) lewat MediaPlayer + SurfaceTexture milik MainActivity.kt (dipanggil via JNI).
// Frame video masuk sebagai tekstur GL_TEXTURE_EXTERNAL_OES. Semua panggilan harus dari thread GL/SDL_main.
#pragma once
#include <jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cstdio>
#include <string>
#include "gfx.h"

struct VideoBg {
    JNIEnv* env = nullptr;
    jobject act = nullptr;
    jmethodID mStart = nullptr, mUpdate = nullptr, mStop = nullptr, mRefresh = nullptr;
    GLuint tex = 0, prog = 0;
    GLint uMat = -1, uTex = -1, uDim = -1;
    float mat[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool jniReady = false, active = false, gotFrame = false;

    bool hasFrame() const { return active && gotFrame; }

    bool clearEx() {
        if (env && env->ExceptionCheck()) { env->ExceptionClear(); return true; }
        return false;
    }

    bool initJni() {
        env = (JNIEnv*)SDL_AndroidGetJNIEnv();
        jobject local = (jobject)SDL_AndroidGetActivity();
        if (!env || !local) return false;
        act = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        jclass cls = env->GetObjectClass(act);
        mStart = env->GetMethodID(cls, "videoStart", "(ILjava/lang/String;)Z");
        mUpdate = env->GetMethodID(cls, "videoUpdate", "()[F");
        mStop = env->GetMethodID(cls, "videoStop", "()V");
        mRefresh = env->GetMethodID(cls, "setRefreshRate", "(F)V");
        env->DeleteLocalRef(cls);
        if (clearEx() || !mStart || !mUpdate || !mStop || !mRefresh) { SDL_Log("Video: method JNI tidak ketemu"); return false; }
        jniReady = true;
        return true;
    }

    bool initGl() {
        if (!tex) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        if (!prog) {
            const char* vs =
                "attribute vec2 aPos; attribute vec2 aUV; uniform mat4 uMat; varying vec2 vUV;\n"
                "void main(){ gl_Position = vec4(aPos, 0.0, 1.0); vUV = (uMat * vec4(aUV, 0.0, 1.0)).xy; }\n";
            const char* fs =
                "#extension GL_OES_EGL_image_external : require\n"
                "precision mediump float; varying vec2 vUV; uniform samplerExternalOES uTex; uniform float uDim;\n"
                "void main(){ gl_FragColor = vec4(texture2D(uTex, vUV).rgb * uDim, 1.0); }\n";
            prog = linkProgram(vs, fs);
            uMat = glGetUniformLocation(prog, "uMat");
            uTex = glGetUniformLocation(prog, "uTex");
            uDim = glGetUniformLocation(prog, "uDim");
        }
        return tex && prog;
    }

    // Path biasa = file di penyimpanan. "asset:nama.mp4" = file yang dibundel di dalam APK (app/src/main/assets).
    bool fileExists(const std::string& p) {
        if (p.compare(0, 6, "asset:") == 0) {
            SDL_RWops* rw = SDL_RWFromFile(p.c_str() + 6, "rb");
            if (!rw) return false;
            SDL_RWclose(rw);
            return true;
        }
        FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) return false;
        std::fclose(f);
        return true;
    }

    // hz = 0 -> otomatis. Aman dipanggil kapan saja (kalau JNI gagal, diabaikan).
    void setRefresh(float hz) {
        if (!jniReady && !initJni()) return;
        env->CallVoidMethod(act, mRefresh, (jfloat)hz);
        clearEx();
    }

    bool start(const std::string& path) {
        if (path.empty() || !fileExists(path)) return false;
        if (!jniReady && !initJni()) return false;
        if (!initGl()) return false;
        jstring js = env->NewStringUTF(path.c_str());
        jboolean ok = env->CallBooleanMethod(act, mStart, (jint)tex, js);
        env->DeleteLocalRef(js);
        if (clearEx()) ok = JNI_FALSE;
        active = ok == JNI_TRUE;
        gotFrame = false;
        SDL_Log("Video latar %s: %s", active ? "jalan" : "GAGAL", path.c_str());
        return active;
    }

    void update() {
        if (!active) return;
        jfloatArray arr = (jfloatArray)env->CallObjectMethod(act, mUpdate);
        if (clearEx()) return;
        if (arr) {
            env->GetFloatArrayRegion(arr, 0, 16, mat);
            env->DeleteLocalRef(arr);
            gotFrame = true;
        }
    }

    void draw(float dim) {
        if (!hasFrame()) return;
        glUseProgram(prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
        glUniform1i(uTex, 0);
        glUniformMatrix4fv(uMat, 1, GL_FALSE, mat);
        glUniform1f(uDim, dim);
        drawQuad(-1.0f, 1.0f, 1.0f, -1.0f, true);
    }

    void stop() {
        if (!active) return;
        env->CallVoidMethod(act, mStop);
        clearEx();
        active = false;
        gotFrame = false;
    }
};
