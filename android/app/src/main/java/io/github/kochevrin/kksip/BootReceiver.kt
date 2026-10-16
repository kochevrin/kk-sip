package io.github.kochevrin.kksip

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import io.github.kochevrin.kksip.sip.CallManager

/** After a reboot or an update, "Receive calls" starts receiving again by itself. */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        CallManager.init(context) // registers and starts SipService when receiving calls
    }
}
