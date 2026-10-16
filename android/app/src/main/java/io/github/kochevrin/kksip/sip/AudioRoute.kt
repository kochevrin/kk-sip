package io.github.kochevrin.kksip.sip

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Build

/**
 * Phone-call audio without Telecom (only where the system refused a self-managed call):
 * communication mode, audio focus, earpiece / speaker / headset.
 */
class AudioRoute(context: Context) {
    private val am = context.getSystemService(AudioManager::class.java)
    private val focus = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
        .setAudioAttributes(
            AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build()
        )
        .build()

    private var active = false

    fun start() {
        if (active) return
        active = true
        am.requestAudioFocus(focus)
        am.mode = AudioManager.MODE_IN_COMMUNICATION
        routeDefault()
    }

    fun stop() {
        if (!active) return
        active = false
        if (Build.VERSION.SDK_INT >= 31) am.clearCommunicationDevice()
        else @Suppress("DEPRECATION") { am.isSpeakerphoneOn = false }
        am.mode = AudioManager.MODE_NORMAL
        am.abandonAudioFocusRequest(focus)
    }

    fun setSpeaker(on: Boolean) {
        if (Build.VERSION.SDK_INT >= 31) {
            if (on) pick(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) else routeDefault()
        } else {
            @Suppress("DEPRECATION")
            am.isSpeakerphoneOn = on
        }
    }

    /** A connected headset wins over the earpiece, as with a normal phone call. */
    private fun routeDefault() {
        if (Build.VERSION.SDK_INT < 31) return // the system routes to wired/BT on its own
        val preferred = listOf(
            AudioDeviceInfo.TYPE_BLE_HEADSET,
            AudioDeviceInfo.TYPE_BLUETOOTH_SCO,
            AudioDeviceInfo.TYPE_USB_HEADSET,
            AudioDeviceInfo.TYPE_WIRED_HEADSET,
            AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
            AudioDeviceInfo.TYPE_BUILTIN_EARPIECE,
        )
        for (type in preferred) if (pick(type)) return
    }

    private fun pick(type: Int): Boolean {
        if (Build.VERSION.SDK_INT < 31) return false
        val dev = am.availableCommunicationDevices.firstOrNull { it.type == type } ?: return false
        return am.setCommunicationDevice(dev)
    }
}
