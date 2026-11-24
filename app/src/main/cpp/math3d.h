// Matriks 4x4 kolom-utama (format OpenGL), cukup untuk kamera orbit.
#pragma once
#include <cmath>
#include <cstring>

namespace m3 {

inline void identity(float* m) { std::memset(m, 0, 16 * sizeof(float)); m[0] = m[5] = m[10] = m[15] = 1.0f; }

// o = a * b
inline void mul(const float* a, const float* b, float* o) {
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
            t[c * 4 + r] = s;
        }
    std::memcpy(o, t, sizeof(t));
}

inline void perspective(float fovy, float aspect, float zn, float zf, float* m) {
    float f = 1.0f / std::tan(fovy * 0.5f);
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = 2.0f * zf * zn / (zn - zf);
}

inline void translate(float x, float y, float z, float* m) { identity(m); m[12] = x; m[13] = y; m[14] = z; }

inline void rotX(float a, float* m) {
    identity(m);
    float c = std::cos(a), s = std::sin(a);
    m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

inline void rotY(float a, float* m) {
    identity(m);
    float c = std::cos(a), s = std::sin(a);
    m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}

// MVP kamera orbit: P * T(0,0,-dist) * Rx(pitch) * Ry(yaw) * T(-center)
inline void orbitMvp(float aspect, float dist, float yaw, float pitch, const float* center, float radius, float* out) {
    float P[16], Tz[16], Rx[16], Ry[16], Tc[16], a[16], b[16];
    float zn = radius * 0.05f, zf = dist + radius * 4.0f;
    if (zn < 0.001f) zn = 0.001f;
    perspective(0.9f, aspect, zn, zf, P);
    translate(0.0f, 0.0f, -dist, Tz);
    rotX(pitch, Rx);
    rotY(yaw, Ry);
    translate(-center[0], -center[1], -center[2], Tc);
    mul(Rx, Ry, a);
    mul(Tz, a, b);
    mul(b, Tc, a);
    mul(P, a, out);
}

}  // namespace m3
