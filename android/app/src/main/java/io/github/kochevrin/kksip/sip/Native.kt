package io.github.kochevrin.kksip.sip

/** Callbacks from libkksip.so; they arrive on the SIP thread. */
interface SipListener {
    /** [state] is pjsip_inv_state: 1 calling, 2 incoming, 3 early, 4 connecting, 5 confirmed, 6 disconnected. */
    fun onCallState(callId: Int, state: Int, code: Int, reason: String, remote: String)
    fun onCallMedia(callId: Int, active: Boolean, localHold: Boolean, remoteHold: Boolean)
    /** [expires] is 0 when the account is not registered. */
    fun onRegState(accountId: String, code: Int, reason: String, expires: Int)
    /** The call rings; libkksip sends 180 Ringing after this returns, unless it was answered. */
    fun onIncomingCall(callId: Int, accountId: String, remote: String)
    /** Progress of a blind transfer; on a final 2xx libkksip hangs our leg up itself. */
    fun onTransferStatus(callId: Int, code: Int, final: Boolean)
}

/** JNI entry points (app/src/main/cpp/kksip_jni.cpp). Call only from the SIP thread. */
object Native {
    init {
        System.loadLibrary("kksip")
    }

    /**
     * Returns null on success or the error text. [sipPort] 0 = random. [logFile] non-empty
     * writes the full PJSIP log, SIP messages included, to that file.
     */
    external fun start(userAgent: String, listener: SipListener, debugLog: Boolean, sipPort: Int, logFile: String): String?
    external fun stop()
    external fun handleEvents(msec: Int)

    /** Adds or replaces an account. Returns null on success or the error text. */
    external fun setAccount(
        id: String, server: String, domain: String, proxy: String, user: String,
        authUser: String, password: String, displayName: String, transport: String,
        srtp: Boolean, natRewrite: Boolean, register: Boolean, regExpiry: Int,
    ): String?
    external fun removeAccount(id: String)
    external fun renewRegistration(id: String)
    external fun ipChanged()

    /** Returns the call id, or -1 and [lastError] says why. */
    external fun makeCall(accountId: String, target: String): Int
    external fun lastError(): String
    external fun answer(callId: Int)
    /** [code]: 0 normal hang-up, 486 busy, 603 decline. */
    external fun hangup(callId: Int, code: Int)
    external fun setMute(callId: Int, mute: Boolean)
    external fun setHold(callId: Int, hold: Boolean)
    external fun dtmf(callId: Int, digits: String)
    /** In-dialog OPTIONS (NAT keepalive, detects a call the PBX already ended); returns RTP packets received, or -1. */
    external fun checkCall(callId: Int): Long
    /** Blind transfer; null when the REFER went out, else the error text. */
    external fun transfer(callId: Int, target: String): String?
}
