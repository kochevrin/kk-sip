package io.github.kochevrin.kksip

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.RingtoneManager
import android.os.SystemClock
import androidx.core.app.NotificationCompat
import androidx.core.app.Person
import io.github.kochevrin.kksip.sip.CallUi
import io.github.kochevrin.kksip.sip.Phase
import io.github.kochevrin.kksip.sip.Reg


/** Notification channels and the notifications [SipService] shows. */
private const val BRAND_ORANGE = 0xFFF57C00.toInt()

object Notifications {
    const val ID_STATUS = 1
    // A call gets its own id: the full-screen call screen only opens for a newly posted
    // notification, never for an update of the "receiving calls" one.
    const val ID_CALL = 3

    // A channel's sound can't change after it is created; a new sound needs a new id.
    private const val CH_INCOMING = "incoming_v1"
    private const val CH_CALL = "call"
    private const val CH_STATUS = "status"
    private const val CH_MISSED = "missed"
    private const val ID_MISSED = 2

    fun createChannels(context: Context) {
        val nm = context.getSystemService(NotificationManager::class.java)
        val ring = NotificationChannel(
            CH_INCOMING, context.getString(R.string.channel_incoming), NotificationManager.IMPORTANCE_HIGH,
        ).apply {
            setSound(
                RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE),
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_NOTIFICATION_RINGTONE)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build(),
            )
            enableVibration(true)
            vibrationPattern = longArrayOf(0, 800, 800)
            lockscreenVisibility = Notification.VISIBILITY_PUBLIC
        }
        val call = NotificationChannel(
            CH_CALL, context.getString(R.string.channel_call), NotificationManager.IMPORTANCE_DEFAULT,
        ).apply { setSound(null, null); enableVibration(false) }
        val status = NotificationChannel(
            CH_STATUS, context.getString(R.string.channel_status), NotificationManager.IMPORTANCE_MIN,
        ).apply { setShowBadge(false) }
        val missed = NotificationChannel(
            CH_MISSED, context.getString(R.string.channel_missed), NotificationManager.IMPORTANCE_DEFAULT,
        )
        nm.createNotificationChannels(listOf(ring, call, status, missed))
    }

    private fun activity(context: Context, code: Int, action: String? = null): PendingIntent =
        PendingIntent.getActivity(
            context, code,
            Intent(context, MainActivity::class.java).setAction(action)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

    private fun service(context: Context, code: Int, action: String): PendingIntent =
        PendingIntent.getService(
            context, code, Intent(context, SipService::class.java).setAction(action),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )

    fun forCall(context: Context, call: CallUi): Notification {
        val person = Person.Builder().setName(call.title).setImportant(true).build()
        val b = if (call.phase == Phase.Incoming) {
            NotificationCompat.Builder(context, CH_INCOMING)
                .setStyle(
                    NotificationCompat.CallStyle.forIncomingCall(
                        person,
                        service(context, 2, SipService.ACTION_DECLINE),
                        // Answering from the banner keeps whatever is on screen (a map, say);
                        // the call screen is one tap on the notification away.
                        service(context, 3, SipService.ACTION_ANSWER),
                    )
                )
                // Locked or screen off: the full call screen. In use: just the banner.
                .setFullScreenIntent(activity(context, 4, MainActivity.ACTION_SHOW_CALL), true)
                .setPriority(NotificationCompat.PRIORITY_MAX)
        } else {
            NotificationCompat.Builder(context, CH_CALL)
                .setStyle(
                    NotificationCompat.CallStyle.forOngoingCall(
                        person, service(context, 5, SipService.ACTION_HANGUP),
                    )
                )
                .setSilent(true)
                .apply {
                    if (call.phase == Phase.Active && call.connectedAt > 0) {
                        setUsesChronometer(true)
                        setWhen(System.currentTimeMillis() - (SystemClock.elapsedRealtime() - call.connectedAt))
                    } else {
                        setShowWhen(false)
                        setContentText(context.getString(
                            when (call.phase) {
                                Phase.Ringing -> R.string.ringing
                                Phase.Connecting -> R.string.connecting
                                Phase.Ended -> R.string.call_ended
                                else -> R.string.calling
                            }
                        ))
                    }
                }
        }
        val n = b.setSmallIcon(R.drawable.ic_call)
            .setColor(BRAND_ORANGE)
            .setContentIntent(activity(context, 6, MainActivity.ACTION_SHOW_CALL))
            .setCategory(NotificationCompat.CATEGORY_CALL)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setOngoing(true)
            .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
            .build()
        if (call.phase == Phase.Incoming) n.flags = n.flags or Notification.FLAG_INSISTENT
        return n
    }

    /** "Receiving calls": the quiet notification that keeps the registration alive. */
    fun forStatus(context: Context, state: Reg, account: String): Notification {
        val text = context.getString(
            when (state) {
                Reg.Online -> R.string.status_online
                Reg.Failed -> R.string.status_failed
                else -> R.string.status_connecting
            }
        )
        return NotificationCompat.Builder(context, CH_STATUS)
            .setSmallIcon(R.drawable.ic_call)
            .setColor(BRAND_ORANGE)
            .setContentTitle(text)
            .setContentText(account)
            .setContentIntent(activity(context, 7))
            .addAction(0, context.getString(R.string.stop_receiving), service(context, 8, SipService.ACTION_STOP_RECEIVING))
            .setOngoing(true)
            .setSilent(true)
            .setShowWhen(false)
            .setPriority(NotificationCompat.PRIORITY_MIN)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
    }

    /** "Missed call from …"; tapping it opens Recents, "Call back" dials right away. */
    fun missed(context: Context, call: CallUi) {
        val nm = context.getSystemService(NotificationManager::class.java)
        val callBack = PendingIntent.getActivity(
            context, 9,
            Intent(context, MainActivity::class.java).setAction(MainActivity.ACTION_CALL_BACK)
                .putExtra(MainActivity.EXTRA_NUMBER, call.number)
                .putExtra(MainActivity.EXTRA_ACCOUNT, call.accountId)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val n = NotificationCompat.Builder(context, CH_MISSED)
            .setSmallIcon(R.drawable.ic_call_missed)
            .setColor(BRAND_ORANGE)
            .setContentTitle(context.getString(R.string.missed_call))
            .setContentText(if (call.name.isNotEmpty()) "${call.name} · ${call.number}" else call.number)
            .setContentIntent(activity(context, 10, MainActivity.ACTION_SHOW_RECENTS))
            .addAction(0, context.getString(R.string.call_back), callBack)
            .setCategory(NotificationCompat.CATEGORY_MISSED_CALL)
            .setAutoCancel(true)
            .build()
        runCatching { nm.notify(ID_MISSED, n) }
    }
}
