package io.github.kochevrin.kksip

import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import io.github.kochevrin.kksip.sip.CallManager
import io.github.kochevrin.kksip.sip.Phase
import io.github.kochevrin.kksip.sip.Reg

/**
 * Keeps kk-sip in the foreground while a call is up, and while "Receive calls" keeps the
 * accounts registered. Its notification is the call (incoming or ongoing) or, between
 * calls, a quiet "receiving calls" line.
 */
class SipService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        instance = this
    }

    override fun onDestroy() {
        instance = null
        super.onDestroy()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        CallManager.init(this)
        when (intent?.action) {
            ACTION_ANSWER -> CallManager.answer()
            ACTION_DECLINE -> CallManager.decline()
            ACTION_HANGUP -> CallManager.hangup()
            ACTION_STOP_RECEIVING -> CallManager.setReceiveCalls(false)
        }
        refresh()
        return if (CallManager.serviceWanted) START_STICKY else START_NOT_STICKY
    }

    private fun refresh() {
        if (!CallManager.serviceWanted) {
            ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
            stopSelf()
            return
        }
        val call = CallManager.call.value
        val notification = if (call != null) {
            Notifications.forCall(this, call)
        } else {
            val reg = CallManager.reg.value.values
            val state = when {
                reg.any { it.state == Reg.Online } -> Reg.Online
                reg.isNotEmpty() && reg.all { it.state == Reg.Failed } -> Reg.Failed
                else -> Reg.Registering
            }
            val names = CallManager.accounts.value.filter { it.isUsable }.joinToString { it.title }
            Notifications.forStatus(this, state, names)
        }
        // Between calls the service only keeps the registration; during a call it is a
        // phone call that records the microphone.
        val receiving = if (Build.VERSION.SDK_INT >= 34) ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE
        else ServiceInfo.FOREGROUND_SERVICE_TYPE_PHONE_CALL
        val type = when {
            call == null -> receiving
            call.phase == Phase.Incoming -> ServiceInfo.FOREGROUND_SERVICE_TYPE_PHONE_CALL
            else -> ServiceInfo.FOREGROUND_SERVICE_TYPE_PHONE_CALL or
                (if (Build.VERSION.SDK_INT >= 30) ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE else 0)
        }
        val id = if (call != null) Notifications.ID_CALL else Notifications.ID_STATUS
        try {
            ServiceCompat.startForeground(this, id, notification, type)
        } catch (e: Exception) {
            // The microphone type is refused when the app was in the background; the call
            // keeps going, Telecom's phone-call type still covers it.
            Log.w("kksip", "startForeground($type): $e")
            runCatching {
                ServiceCompat.startForeground(
                    this, id, notification,
                    type and ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE.inv(),
                )
            }.onFailure { Log.e("kksip", "startForeground: $it") }
        }
        // Only one of the two is up at a time.
        getSystemService(NotificationManager::class.java).cancel(
            if (call != null) Notifications.ID_STATUS else Notifications.ID_CALL
        )
    }

    companion object {
        const val ACTION_ANSWER = "io.github.kochevrin.kksip.ANSWER"
        const val ACTION_DECLINE = "io.github.kochevrin.kksip.DECLINE"
        const val ACTION_HANGUP = "io.github.kochevrin.kksip.HANGUP"
        const val ACTION_STOP_RECEIVING = "io.github.kochevrin.kksip.STOP_RECEIVING"

        @Volatile private var instance: SipService? = null

        /** Starts, updates or stops the service to match [CallManager]. Any thread. */
        fun sync(context: Context) {
            val s = instance
            if (s != null) {
                s.mainExecutor.execute { s.refresh() }
                return
            }
            if (!CallManager.serviceWanted) return
            try {
                ContextCompat.startForegroundService(context, Intent(context, SipService::class.java))
            } catch (e: Exception) {
                // Not allowed from the background (e.g. a call came in while the service was
                // not running); the next start from the app or the boot receiver fixes it.
                Log.w("kksip", "start service: $e")
            }
        }
    }
}
