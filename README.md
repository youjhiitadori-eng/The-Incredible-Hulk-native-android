# Hulk Android port (The Incredible Hulk, 2008 PC)

Engine Android native (C++ NDK): SDL2 + OpenGL ES 2.0 + miniaudio + stb_image.

## Status
**Milestone 3: menu (video latar + logo) lalu viewer model 3D bertekstur dan viewer tekstur, bisa pakai layar sentuh atau gamepad PS.** Engine sudah bisa membaca `index.ind` + arsip `.ARC` milik game dan menampilkan
semua 1365 tekstur (DXT1/DXT3/DXT5/RGB32). Viewer model 3D juga jalan: 1543 model (4578 mesh) terbaca, tampil dengan tekstur aslinya (pose bawaan, tanpa animasi). Animasi, level/dunia, physics, audio, dan logika game BELUM ada.

Format arsip hasil analisis (lihat komentar di `engine/arc.h`):
- `index.ind` berisi 26 section (Textures, Models, Animations, Sounds, ...) berisi daftar (offset, size) ke file `<NAMA8>.ARC`.
- Tiap entry diawali nama aset (string C). Tekstur = pembungkus `TXFL` + file DDS biasa.

## Build APK via GitHub (tanpa setup lokal)
1. Upload semua isi folder ini ke repo GitHub baru.
2. Tab **Actions** > **Build APK** > **Run workflow**.
3. Download APK dari **Artifacts** (`hulk-debug-apk`).

## Taruh data game di HP
Dari game aslinya (folder `TheIncredibleHulk/data`), cukup 3 file untuk milestone ini: `index.ind`, `TEXTURES.ARC`, dan `MODELS.ARC`
(menyalin seluruh folder `data` juga boleh). Taruh di salah satu:
- `/sdcard/Hulk/data/` — aktifkan dulu: Pengaturan > Aplikasi > Hulk Port > **Akses semua file** (All files access).
- `/sdcard/Android/data/com.hulk.port/files/data/` — tanpa izin, tapi di Android 11+ sebagian file manager tidak bisa menulis ke sini.

Kontrol viewer:
- Sentuh: ketuk kiri/kanan layar = model/tekstur sebelumnya/berikutnya, ketuk tengah = ganti Model <-> Tekstur, geser jari = putar model, tombol Back = menu.
- Gamepad: D-pad kiri/kanan atau L1/R1 = sebelumnya/berikutnya, segitiga = ganti viewer, bulat = menu, stik kanan = putar, stik kiri atas/bawah = zoom.

Nama file harus persis (huruf besar/kecil): `index.ind`, `TEXTURES.ARC`, `MODELS.ARC`.

## Menu, video latar, gamepad
- **Video latar di dalam APK**: taruh `bg.mp4` di `app/src/main/assets/bg.mp4` di repo, lalu build ulang. Disarankan di bawah 20 MB (720p, 10-20 detik, H.264, tanpa audio). File `bg.mp4` di penyimpanan HP (lihat bawah) akan menimpa yang di APK.
- **Video latar dari penyimpanan** (opsional, tanpa build ulang): taruh video `bg.mp4` (H.264, disarankan 16:9, 720p biar enteng di HP kentang) di
  `/sdcard/Hulk/bg.mp4` atau `/sdcard/Android/data/com.hulk.port/files/bg.mp4`. Tanpa video, latarnya gradien hijau bergerak.
  Video diputar loop tanpa suara dan digelapkan 45% biar logo terbaca (ubah `videoDim` di `main.cpp`).
- **Logo/pp**: `app/src/main/assets/pp.png` (avatar) dan `title.png`. Timpa file itu dengan gambar lain (nama sama) untuk ganti.
- **Teks "Created by"**: ubah konstanta `CREATED_BY` di `app/src/main/cpp/main.cpp`.
- **Pengaturan** (menu > PENGATURAN, tersimpan otomatis di `settings.cfg` folder data app): Resolusi internal (Asli/360p-1080p, di-render kecil lalu diperbesar), FPS 20/24/30/60, Aspect Ratio (isi layar / 16:9), Screen Refresh Rate (Auto/60/90/120 Hz, tergantung HP), Kualitas Tekstur, tampilkan FPS, video latar nyala/mati.
- **Gamepad PS4/PS5** (Bluetooth atau kabel): X = mulai / kembali dari layar merah, Bulat = kembali ke menu,
  D-pad kiri/kanan atau L1/R1 = sebelumnya/berikutnya, Segitiga = pindah viewer model/tekstur, stik kanan = putar model, stik kiri atas/bawah = zoom. Xbox dan gamepad lain juga jalan lewat SDL.
- **Sentuh**: ketuk layar di menu = masuk viewer model. Di viewer: ketuk sepertiga kiri/kanan = model/tekstur sebelumnya/berikutnya,
  ketuk tengah = pindah antara viewer model dan viewer tekstur, geser jari = putar model. Tombol Back HP = kembali ke menu.

Buka app: layar **merah** = folder data tidak ditemukan. Ketuk sisi kanan layar = tekstur berikutnya, sisi kiri = sebelumnya.
Path yang dicari dicetak di logcat (`adb logcat -s SDL`).

## Tool PC / Termux
```
cd tools/arc_dump && cmake -B build && cmake --build build
./build/arc_dump /path/data sections
./build/arc_dump /path/data list Textures 20
./build/arc_dump /path/data tex 10 out.ppm
# uji parser model (butuh g++): g++ -std=c++17 -O2 -o model_dump model_dump.cpp
./model_dump /path/data stats
./build/model_dump /path/data stats
./build/model_dump /path/data obj 0 out.txt
```
Jangan upload aset atau file game ke repo. Hanya kode engine.
