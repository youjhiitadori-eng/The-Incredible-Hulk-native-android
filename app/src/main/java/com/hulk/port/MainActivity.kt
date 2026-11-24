package com.hulk.port

import android.graphics.SurfaceTexture
import android.media.MediaPlayer
import android.util.Log
import android.view.Surface
import org.libsdl.app.SDLActivity

// SDLActivity memuat libSDL2.so dan libmain.so (engine kita).
// Fungsi video* dipanggil dari C++ lewat JNI (lihat cpp/video.h) dari thread GL.
class MainActivity : SDLActivity() {
    private var player: MediaPlayer? = null
    private var texture: SurfaceTexture? = null
    private var surface: Surface? = null
    private val matrix = FloatArray(16)

    @Volatile
    private var ready = false

    fun videoStart(texId: Int, path: String): Boolean {
        videoStop()
        return try {
            val st = SurfaceTexture(texId)
            val sf = Surface(st)
            val mp = MediaPlayer()
            if (path.startsWith("asset:")) {
                val afd = assets.openFd(path.removePrefix("asset:"))
                mp.setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
                afd.close()
            } else {
                mp.setDataSource(path)
            }
            mp.setSurface(sf)
            mp.isLooping = true
            mp.setVolume(0f, 0f)
            mp.setOnPreparedListener { p ->
                p.start()
                ready = true
            }
            mp.setOnErrorListener { _, what, extra ->
                Log.e("HulkVideo", "error $what/$extra")
                ready = false
                true
            }
            mp.prepareAsync()
            texture = st
            surface = sf
            player = mp
            true
        } catch (e: Exception) {
            Log.e("HulkVideo", "start gagal", e)
            videoStop()
            false
        }
    }

    // Mengembalikan matriks transformasi 4x4 kalau ada frame baru, null kalau belum siap.
    fun videoUpdate(): FloatArray? {
        val st = texture ?: return null
        if (!ready) return null
        return try {
            st.updateTexImage()
            st.getTransformMatrix(matrix)
            matrix
        } catch (e: Exception) {
            null
        }
    }

    fun videoStop() {
        ready = false
        try { player?.release() } catch (e: Exception) { }
        try { surface?.release() } catch (e: Exception) { }
        try { texture?.release() } catch (e: Exception) { }
        player = null
        surface = null
        texture = null
    }

    // Preferensi refresh rate layar (0 = otomatis). Dipanggil dari C++ lewat JNI.
    fun setRefreshRate(hz: Float) {
        runOnUiThread {
            try {
                val lp = window.attributes
                lp.preferredRefreshRate = hz
                window.attributes = lp
            } catch (e: Exception) {
                Log.e("HulkDisplay", "setRefreshRate gagal", e)
            }
        }
    }

    override fun onPause() {
        super.onPause()
        try { if (ready) player?.pause() } catch (e: Exception) { }
    }

    override fun onResume() {
        super.onResume()
        try { if (ready) player?.start() } catch (e: Exception) { }
    }

    override fun onDestroy() {
        videoStop()
        super.onDestroy()
    }
}
