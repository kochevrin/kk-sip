package io.github.kochevrin.kksip

import android.content.Context

/** App-wide switches that are not part of an account. */
class Prefs(context: Context) {
    private val p = context.getSharedPreferences("app", Context.MODE_PRIVATE)

    /** Stay registered in the background so calls can come in. */
    var receiveCalls: Boolean
        get() = p.getBoolean("receiveCalls", false)
        set(v) = p.edit().putBoolean("receiveCalls", v).apply()

    /** Asked once whether kk-sip may ignore battery optimisation. */
    var batteryAsked: Boolean
        get() = p.getBoolean("batteryAsked", false)
        set(v) = p.edit().putBoolean("batteryAsked", v).apply()

    /** Full PJSIP log into a file the user can share (Settings → Debug log). */
    var debugLog: Boolean
        get() = p.getBoolean("debugLog", false)
        set(v) = p.edit().putBoolean("debugLog", v).apply()

    /** The first start asked for the call permissions already. */
    var permissionsAsked: Boolean
        get() = p.getBoolean("permissionsAsked", false)
        set(v) = p.edit().putBoolean("permissionsAsked", v).apply()
}
