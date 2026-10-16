package io.github.kochevrin.kksip

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.PowerManager
import android.os.SystemClock
import io.github.kochevrin.kksip.sip.CallManager

/**
 * PJSIP's own timers stop while the phone sleeps, so the registration would run out and
 * the PBX would stop sending calls. This alarm wakes the phone shortly before that and
 * re-registers.
 */
class RegAlarm : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val wake = context.getSystemService(PowerManager::class.java)
            .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "kksip:register")
        wake.acquire(20_000L) // long enough for REGISTER and its answer
        CallManager.init(context)
        CallManager.renewRegistrations()
        schedule(context, if (CallManager.receiveCalls.value) CallManager.REG_EXPIRY else 0)
    }

    companion object {
        /** [expirySec] 0 cancels. */
        fun schedule(context: Context, expirySec: Int) {
            val am = context.getSystemService(AlarmManager::class.java)
            val pi = PendingIntent.getBroadcast(
                context, 0, Intent(context, RegAlarm::class.java),
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )
            am.cancel(pi)
            if (expirySec <= 0) return
            val at = SystemClock.elapsedRealtime() + (expirySec - 90).coerceAtLeast(60) * 1000L
            if (Build.VERSION.SDK_INT < 31 || am.canScheduleExactAlarms())
                am.setExactAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP, at, pi)
            else
                am.setAndAllowWhileIdle(AlarmManager.ELAPSED_REALTIME_WAKEUP, at, pi)
        }
    }
}
