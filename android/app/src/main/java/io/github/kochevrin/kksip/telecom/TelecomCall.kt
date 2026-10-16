package io.github.kochevrin.kksip.telecom

import android.net.Uri
import android.telecom.DisconnectCause
import android.telecom.PhoneAccount
import android.util.Log
import androidx.core.telecom.CallAttributesCompat
import androidx.core.telecom.CallControlScope
import androidx.core.telecom.CallEndpointCompat
import androidx.core.telecom.CallsManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.launch

/**
 * One SIP call as the system sees it (Jetpack core-telecom, a self-managed call). Telecom
 * then routes audio (earpiece, speaker, headset, car), ducks navigation and music instead
 * of cutting them, lets a watch or car answer or end the call, and holds us when a cellular
 * call comes in.
 */
class TelecomCall(
    private val scope: CoroutineScope,
    private val manager: CallsManager,
    private val listener: Listener,
) {
    interface Listener {
        /** Answered from outside the app: a watch, the car, a headset button. */
        fun onTelecomAnswer()
        /** Ended from outside the app. */
        fun onTelecomDisconnect()
        /** Telecom wants the call held (e.g. a cellular call) or resumed. */
        fun onTelecomHold(hold: Boolean)
        fun onTelecomEndpoints(current: CallEndpointCompat?, available: List<CallEndpointCompat>)
        fun onTelecomMute(muted: Boolean)
        /** Telecom refused the call; the caller falls back to plain audio routing. */
        fun onTelecomFailed()
    }

    private var control: CallControlScope? = null
    private val pending = mutableListOf<suspend CallControlScope.() -> Unit>()
    private var ended = false

    fun start(name: String, number: String, incoming: Boolean) {
        val attributes = CallAttributesCompat(
            displayName = name.ifEmpty { number },
            address = Uri.fromParts(PhoneAccount.SCHEME_SIP, number, null),
            direction = if (incoming) CallAttributesCompat.DIRECTION_INCOMING
            else CallAttributesCompat.DIRECTION_OUTGOING,
            callType = CallAttributesCompat.CALL_TYPE_AUDIO_CALL,
            callCapabilities = CallAttributesCompat.SUPPORTS_SET_INACTIVE,
        )
        scope.launch {
            try {
                manager.addCall(
                    attributes,
                    onAnswer = { listener.onTelecomAnswer() },
                    onDisconnect = { listener.onTelecomDisconnect() },
                    onSetActive = { listener.onTelecomHold(false) },
                    onSetInactive = { listener.onTelecomHold(true) },
                ) {
                    control = this
                    for (action in pending) launch { runCatching { action() } }
                    pending.clear()
                    launch {
                        combine(currentCallEndpoint, availableEndpoints) { c, a -> c to a }
                            .collect { (c, a) -> listener.onTelecomEndpoints(c, a) }
                    }
                    launch { isMuted.collect { listener.onTelecomMute(it) } }
                }
            } catch (e: Exception) {
                Log.w("kksip", "telecom: $e")
                if (!ended) listener.onTelecomFailed()
            } finally {
                control = null
            }
        }
    }

    private fun run(action: suspend CallControlScope.() -> Unit) {
        val c = control
        if (c != null) c.launch { runCatching { c.action() } } else pending += action
    }

    fun setActive() = run { setActive() }
    fun setInactive() = run { setInactive() }
    fun answer() = run { answer(CallAttributesCompat.CALL_TYPE_AUDIO_CALL) }
    fun requestEndpoint(endpoint: CallEndpointCompat) = run { requestEndpointChange(endpoint) }

    /** [cause] is a [DisconnectCause] code: LOCAL, REMOTE, REJECTED, MISSED, BUSY, ERROR. */
    fun disconnect(cause: Int) {
        if (ended) return
        ended = true
        run { disconnect(DisconnectCause(cause)) }
    }
}
