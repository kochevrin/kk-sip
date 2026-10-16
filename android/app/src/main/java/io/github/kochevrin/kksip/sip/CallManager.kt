package io.github.kochevrin.kksip.sip

import android.content.Context
import android.media.AudioManager
import android.media.ToneGenerator
import android.net.ConnectivityManager
import android.net.Network
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.os.PowerManager
import android.os.SystemClock
import android.telecom.DisconnectCause
import android.util.Log
import androidx.core.telecom.CallEndpointCompat
import androidx.core.telecom.CallsManager
import io.github.kochevrin.kksip.BuildConfig
import io.github.kochevrin.kksip.Notifications
import io.github.kochevrin.kksip.data.CallLog
import io.github.kochevrin.kksip.data.CallRecord
import io.github.kochevrin.kksip.Prefs
import io.github.kochevrin.kksip.RegAlarm
import io.github.kochevrin.kksip.SipService
import io.github.kochevrin.kksip.telecom.TelecomCall
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

enum class Phase { Incoming, Calling, Ringing, Connecting, Active, Ended }

enum class RouteType { Earpiece, Speaker, Bluetooth, Wired, Other }

data class Route(val type: RouteType, val name: String)

data class CallUi(
    val id: Int,
    val accountId: String,
    val number: String,
    val name: String = "",
    val incoming: Boolean = false,
    val phase: Phase = Phase.Calling,
    val code: Int = 0,
    val reason: String = "",
    val connectedAt: Long = 0, // SystemClock.elapsedRealtime()
    val muted: Boolean = false,
    val onHold: Boolean = false,
    val earlyMedia: Boolean = false, // the far end plays its own ringback or announcement
    val startedAt: Long = System.currentTimeMillis(),
    val answered: Boolean = false, // an incoming call the user picked up
    val route: Route = Route(RouteType.Earpiece, ""),
    val routes: List<Route> = emptyList(),
) {
    val title: String get() = name.ifEmpty { number }
}

enum class Reg { Off, Registering, Online, Failed }

data class RegStatus(val state: Reg, val code: Int = 0, val reason: String = "")

/**
 * Owns the PJSIP endpoint and the one call kk-sip handles at a time.
 *
 * PJSIP runs on its own thread, which also pumps its events. It runs only while it is
 * needed: the app is on screen, a call is up, or "Receive calls" keeps the accounts
 * registered (then [SipService] keeps the process alive).
 */
object CallManager : SipListener, TelecomCall.Listener {
    private const val TAG = "kksip"
    private const val ENDED_LINGER_MS = 2000L
    // The PBX cancels unanswered calls itself; this only catches a caller that vanished.
    private const val RING_TIMEOUT_MS = 90_000L
    const val REG_EXPIRY = 600 // seconds; RegAlarm renews it while the phone sleeps
    private val DEBUG_SIP_PORT = if (BuildConfig.DEBUG) 5062 else 0 // emulator tests redirect to it

    private lateinit var app: Context
    private lateinit var prefs: Prefs
    private lateinit var store: AccountStore
    lateinit var log: CallLog
        private set
    private val sipThread = HandlerThread("sip").apply { start() }
    private val sip = Handler(sipThread.looper)
    private val main = Handler(Looper.getMainLooper())
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    // SIP thread only.
    private var running = false
    private val applied = mutableMapOf<String, Pair<Account, Boolean>>() // id -> (config, registered)

    @Volatile private var inForeground = false

    // Main thread only.
    private var callsManager: CallsManager? = null
    private var telecom: TelecomCall? = null
    private var fallbackAudio: AudioRoute? = null
    private var endpoints: List<CallEndpointCompat> = emptyList()
    private var ringback: ToneGenerator? = null
    private var proximity: PowerManager.WakeLock? = null
    private var ringWake: PowerManager.WakeLock? = null

    private val _accounts = MutableStateFlow<List<Account>>(emptyList())
    val accounts: StateFlow<List<Account>> = _accounts.asStateFlow()

    private val _currentId = MutableStateFlow<String?>(null)
    val currentId: StateFlow<String?> = _currentId.asStateFlow()

    private val _receiveCalls = MutableStateFlow(false)
    val receiveCalls: StateFlow<Boolean> = _receiveCalls.asStateFlow()

    private val _reg = MutableStateFlow<Map<String, RegStatus>>(emptyMap())
    val reg: StateFlow<Map<String, RegStatus>> = _reg.asStateFlow()

    private val _call = MutableStateFlow<CallUi?>(null)
    val call: StateFlow<CallUi?> = _call.asStateFlow()

    private val _error = MutableStateFlow<String?>(null)
    /** Last failure to show the user once; the UI clears it. */
    val error: StateFlow<String?> = _error.asStateFlow()
    fun clearError() { _error.value = null }

    val currentAccount: Account?
        get() = _accounts.value.filter { it.isUsable }
            .let { list -> list.firstOrNull { it.id == _currentId.value } ?: list.firstOrNull() }

    fun init(context: Context) {
        if (::app.isInitialized) return
        app = context.applicationContext
        prefs = Prefs(app)
        store = AccountStore(app)
        log = CallLog(app)
        _accounts.value = store.load()
        _currentId.value = store.currentId
        _receiveCalls.value = prefs.receiveCalls
        val pm = app.getSystemService(PowerManager::class.java)
        if (pm.isWakeLockLevelSupported(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK))
            proximity = pm.newWakeLock(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK, "kksip:proximity")
        ringWake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "kksip:call")
        callsManager = runCatching {
            CallsManager(app).apply { registerAppWithTelecom(CallsManager.CAPABILITY_BASELINE) }
        }.onFailure { Log.w(TAG, "telecom unavailable: $it") }.getOrNull()
        watchNetwork()
        reconfigure()
    }

    // --- accounts and registration ------------------------------------------------------

    fun saveAccount(account: Account) {
        val list = _accounts.value.toMutableList()
        val i = list.indexOfFirst { it.id == account.id }
        if (i >= 0) list[i] = account else list += account
        _accounts.value = list
        store.save(list)
        if (_currentId.value == null || i < 0) selectAccount(account.id)
        reconfigure()
    }

    /** Accounts from an INI file or a QR link; see [io.github.kochevrin.kksip.provision.IniImport]. */
    fun importAccounts(imported: List<Account>) {
        val list = io.github.kochevrin.kksip.provision.IniImport.merge(_accounts.value, imported)
        _accounts.value = list
        store.save(list)
        if (currentAccount == null || _currentId.value == null) selectAccount(list.firstOrNull { it.isUsable }?.id)
        reconfigure()
    }

    fun deleteAccount(id: String) {
        val list = _accounts.value.filterNot { it.id == id }
        _accounts.value = list
        store.save(list)
        if (_currentId.value == id) selectAccount(list.firstOrNull()?.id)
        reconfigure()
    }

    fun selectAccount(id: String?) {
        _currentId.value = id
        store.currentId = id
    }

    fun setReceiveCalls(on: Boolean) {
        _receiveCalls.value = on
        prefs.receiveCalls = on
        reconfigure()
    }

    private val wantsRegistration: Boolean
        get() = _receiveCalls.value && _accounts.value.any { it.isUsable }

    /** [SipService] keeps the process in the foreground while this is true. */
    val serviceWanted: Boolean
        get() = _call.value != null || wantsRegistration

    /** Brings PJSIP accounts and the service in line with the settings. */
    fun reconfigure() {
        val accounts = _accounts.value.filter { it.isUsable }
        val register = _receiveCalls.value
        if (!register) _reg.value = emptyMap()
        SipService.sync(app)
        sip.post {
            if (!engineWanted()) { stopEngine(); return@post }
            if (!ensureStarted()) return@post
            applyAccounts(accounts, register)
        }
        main.post { RegAlarm.schedule(app, if (register && accounts.isNotEmpty()) REG_EXPIRY else 0) }
    }

    private fun applyAccounts(accounts: List<Account>, register: Boolean) {
        val ids = accounts.map { it.id }.toSet()
        for (id in applied.keys.toList() - ids) {
            Native.removeAccount(id)
            applied.remove(id)
        }
        for (a in accounts) {
            if (applied[a.id] == a to register) continue
            val err = Native.setAccount(
                a.id, a.server, a.domain, a.proxy, a.user, a.authUser, a.password,
                a.displayName, a.transport, a.srtp, a.natRewrite, register, REG_EXPIRY,
            )
            if (err != null) {
                Log.w(TAG, "account ${a.title}: $err")
                if (register) _reg.update { it + (a.id to RegStatus(Reg.Failed, 0, err)) }
                applied.remove(a.id)
                continue
            }
            applied[a.id] = a to register
            if (register) _reg.update { it + (a.id to RegStatus(Reg.Registering)) }
        }
    }

    /** RegAlarm woke the phone: refresh every registration before it runs out. */
    fun renewRegistrations() {
        sip.post {
            if (!running) return@post
            for ((id, cfg) in applied) if (cfg.second) Native.renewRegistration(id)
        }
    }

    private fun watchNetwork() {
        val cm = app.getSystemService(ConnectivityManager::class.java)
        cm.registerDefaultNetworkCallback(object : ConnectivityManager.NetworkCallback() {
            private var last: Network? = null
            override fun onAvailable(network: Network) {
                val changed = last != null && last != network
                last = network
                if (changed) sip.post { if (running) Native.ipChanged() }
            }
        })
    }

    // --- engine lifetime (SIP thread) ---------------------------------------------------

    private fun engineWanted() = inForeground || wantsRegistration || _call.value != null

    private val pump = object : Runnable {
        override fun run() {
            if (!running) return
            // Short waits during a call keep media signalling snappy; idle, it mostly sleeps.
            Native.handleEvents(if (_call.value != null) 20 else 100)
            sip.post(this)
        }
    }

    private fun ensureStarted(): Boolean {
        if (running) return true
        val err = Native.start(
            "kk-sip-android/${BuildConfig.VERSION_NAME}", this, BuildConfig.DEBUG, DEBUG_SIP_PORT,
        )
        if (err != null) {
            Log.e(TAG, "start: $err")
            _error.value = err
            return false
        }
        running = true
        applied.clear()
        sip.post(pump)
        return true
    }

    private fun stopEngine() {
        if (!running) return
        running = false
        sip.removeCallbacks(pump)
        Native.stop()
        applied.clear()
    }

    /** The app came on screen: warm the SIP stack up so dialing is instant. */
    fun onForeground() {
        inForeground = true
        reconfigure()
    }

    fun onBackground() {
        inForeground = false
        sip.post { if (!engineWanted()) stopEngine() }
    }

    // --- call control (main thread) -----------------------------------------------------

    fun makeCall(account: Account, input: String) {
        if (_call.value != null) return
        val target = SipUri.toTarget(input, account)
        if (target.isEmpty()) return
        val number = SipUri.parse(target).user
        _call.value = CallUi(id = -1, accountId = account.id, number = number)
        startCallSession(number, "", incoming = false)
        sip.post {
            if (!ensureStarted()) { main.post { finish() }; return@post }
            applyAccounts(_accounts.value.filter { it.isUsable }, _receiveCalls.value)
            val id = Native.makeCall(account.id, target)
            if (id < 0) {
                _error.value = Native.lastError()
                main.post { finish() }
                return@post
            }
            _call.update { c -> if (c != null && c.id == -1) c.copy(id = id) else c }
        }
    }

    fun answer() {
        val c = _call.value ?: return
        if (c.phase != Phase.Incoming) return
        telecom?.answer()
        doAnswer(c)
    }

    private fun doAnswer(c: CallUi) {
        _call.value = c.copy(phase = Phase.Connecting, answered = true)
        SipService.sync(app)
        sip.post { Native.answer(c.id) }
        updateAudioState()
    }

    fun decline() {
        val c = _call.value ?: return
        if (c.phase != Phase.Incoming) return
        telecom?.disconnect(DisconnectCause.REJECTED)
        sip.post { Native.hangup(c.id, 603) }
    }

    fun hangup() {
        val c = _call.value ?: return
        if (c.phase == Phase.Incoming) { decline(); return }
        if (c.phase == Phase.Ended) return
        telecom?.disconnect(DisconnectCause.LOCAL)
        if (c.id < 0) { finish(); return } // still starting up
        sip.post { Native.hangup(c.id, 0) }
    }

    fun toggleMute() {
        val c = _call.value ?: return
        setMute(!c.muted)
    }

    private fun setMute(mute: Boolean) {
        val c = _call.value ?: return
        if (c.muted == mute) return
        _call.value = c.copy(muted = mute)
        if (c.id >= 0) sip.post { Native.setMute(c.id, mute) }
    }

    fun toggleHold() {
        val c = _call.value ?: return
        if (c.id < 0 || c.phase != Phase.Active) return
        val hold = !c.onHold
        if (hold) telecom?.setInactive() else telecom?.setActive()
        sip.post { Native.setHold(c.id, hold) }
    }

    /** Speaker on/off; off goes back to the headset if one is connected, else the earpiece. */
    fun toggleSpeaker() {
        val c = _call.value ?: return
        val toSpeaker = c.route.type != RouteType.Speaker
        val t = telecom
        if (t != null && endpoints.isNotEmpty()) {
            val pick = if (toSpeaker) endpoints.firstOrNull { it.type == CallEndpointCompat.TYPE_SPEAKER }
            else endpoints.firstOrNull { it.type == CallEndpointCompat.TYPE_BLUETOOTH }
                ?: endpoints.firstOrNull { it.type == CallEndpointCompat.TYPE_WIRED_HEADSET }
                ?: endpoints.firstOrNull { it.type == CallEndpointCompat.TYPE_EARPIECE }
            pick?.let { t.requestEndpoint(it) }
        } else {
            fallbackAudio?.setSpeaker(toSpeaker)
            _call.value = c.copy(route = Route(if (toSpeaker) RouteType.Speaker else RouteType.Earpiece, ""))
            updateProximity()
        }
    }

    fun setRoute(route: Route) {
        val e = endpoints.firstOrNull { routeOf(it) == route } ?: return
        telecom?.requestEndpoint(e)
    }

    fun dtmf(digit: Char) {
        val c = _call.value ?: return
        if (c.id < 0 || c.phase != Phase.Active) return
        sip.post { Native.dtmf(c.id, digit.toString()) }
    }

    // --- system call session, audio, tones (main thread) --------------------------------

    private fun startCallSession(number: String, name: String, incoming: Boolean) {
        val cm = callsManager
        if (cm != null) {
            telecom = TelecomCall(scope, cm, this).also { it.start(name, number, incoming) }
        } else {
            useFallbackAudio()
        }
        SipService.sync(app)
    }

    private fun useFallbackAudio() {
        if (fallbackAudio == null) fallbackAudio = AudioRoute(app)
        if (_call.value?.phase != Phase.Incoming) fallbackAudio?.start()
    }

    private fun routeOf(e: CallEndpointCompat) = Route(
        when (e.type) {
            CallEndpointCompat.TYPE_EARPIECE -> RouteType.Earpiece
            CallEndpointCompat.TYPE_SPEAKER -> RouteType.Speaker
            CallEndpointCompat.TYPE_BLUETOOTH -> RouteType.Bluetooth
            CallEndpointCompat.TYPE_WIRED_HEADSET -> RouteType.Wired
            else -> RouteType.Other
        },
        e.name.toString(),
    )

    /** Ringback, proximity and fallback audio follow the call's phase. */
    private fun updateAudioState() {
        val c = _call.value
        val wantRingback = c != null && !c.incoming && c.phase == Phase.Ringing && !c.earlyMedia
        if (wantRingback && ringback == null) {
            ringback = runCatching { ToneGenerator(AudioManager.STREAM_VOICE_CALL, 70) }.getOrNull()
            ringback?.startTone(ToneGenerator.TONE_SUP_RINGTONE)
        } else if (!wantRingback && ringback != null) {
            ringback?.stopTone()
            ringback?.release()
            ringback = null
        }
        if (c != null && c.phase != Phase.Incoming && fallbackAudio != null) fallbackAudio?.start()
        updateProximity()
    }

    private fun updateProximity() {
        val lock = proximity ?: return
        val c = _call.value
        val atEar = c != null && c.phase != Phase.Incoming && c.phase != Phase.Ended &&
            c.route.type == RouteType.Earpiece
        if (atEar && !lock.isHeld) lock.acquire(4 * 60 * 60 * 1000L)
        else if (!atEar && lock.isHeld) lock.release(PowerManager.RELEASE_FLAG_WAIT_FOR_NO_PROXIMITY)
    }

    // --- TelecomCall.Listener (main thread) ---------------------------------------------

    override fun onTelecomAnswer() {
        val c = _call.value ?: return
        if (c.phase == Phase.Incoming) doAnswer(c)
    }

    override fun onTelecomDisconnect() {
        val c = _call.value ?: return
        if (c.phase == Phase.Ended) return
        sip.post { Native.hangup(c.id, if (c.phase == Phase.Incoming) 603 else 0) }
    }

    override fun onTelecomHold(hold: Boolean) {
        val c = _call.value ?: return
        if (c.id < 0 || c.phase != Phase.Active || c.onHold == hold) return
        sip.post { Native.setHold(c.id, hold) }
    }

    override fun onTelecomEndpoints(current: CallEndpointCompat?, available: List<CallEndpointCompat>) {
        endpoints = available
        _call.update { c ->
            c?.copy(
                route = current?.let(::routeOf) ?: c.route,
                routes = available.map(::routeOf),
            )
        }
        updateProximity()
    }

    override fun onTelecomMute(muted: Boolean) = setMute(muted)

    override fun onTelecomFailed() {
        telecom = null
        if (_call.value != null) useFallbackAudio()
    }

    // --- SipListener (SIP thread) -------------------------------------------------------

    override fun onRegState(accountId: String, code: Int, reason: String, expires: Int) {
        Log.i(TAG, "reg $accountId $code $reason expires=$expires")
        if (!_receiveCalls.value) return
        val status = when {
            expires > 0 && code / 100 == 2 -> RegStatus(Reg.Online, code, reason)
            code / 100 == 2 -> RegStatus(Reg.Off, code, reason) // unregistered
            else -> RegStatus(Reg.Failed, code, reason)
        }
        _reg.update { it + (accountId to status) }
        main.post { SipService.sync(app) }
    }

    override fun onIncomingCall(callId: Int, accountId: String, remote: String) {
        val parsed = SipUri.parse(remote)
        Log.i(TAG, "incoming $callId from $remote")
        if (_call.value != null) { // one call at a time
            Native.hangup(callId, 486)
            return
        }
        _call.value = CallUi(
            id = callId, accountId = accountId, number = parsed.user, name = parsed.displayName,
            incoming = true, phase = Phase.Incoming,
        )
        main.post {
            ringWake?.acquire(RING_TIMEOUT_MS)
            startCallSession(parsed.user, parsed.displayName, incoming = true)
            main.postDelayed({
                val c = _call.value
                if (c?.id == callId && c.phase == Phase.Incoming) {
                    telecom?.disconnect(DisconnectCause.MISSED)
                    sip.post { Native.hangup(callId, 480) }
                }
            }, RING_TIMEOUT_MS)
        }
    }

    override fun onCallState(callId: Int, state: Int, code: Int, reason: String, remote: String) {
        Log.i(TAG, "call $callId state $state $code $reason")
        val parsed = SipUri.parse(remote)
        _call.update { c ->
            if (c == null || (c.id != callId && c.id != -1)) return@update c
            val phase = when (state) {
                1 -> Phase.Calling
                3 -> if (c.incoming) c.phase else if (code == 180 || code == 183) Phase.Ringing else c.phase
                4 -> Phase.Connecting
                5 -> Phase.Active
                6 -> Phase.Ended
                else -> c.phase
            }
            c.copy(
                id = callId,
                phase = phase,
                code = code,
                reason = reason,
                name = parsed.displayName.ifEmpty { c.name },
                connectedAt = if (phase == Phase.Active && c.connectedAt == 0L)
                    SystemClock.elapsedRealtime() else c.connectedAt,
            )
        }
        main.post {
            val c = _call.value
            if (c?.id != callId) return@post
            when (c.phase) {
                Phase.Active -> {
                    telecom?.setActive()
                    ringWake?.takeIf { it.isHeld }?.release()
                }
                Phase.Ended -> {
                    val cause = when {
                        c.incoming && !c.answered ->
                            if (code == 603) DisconnectCause.REJECTED else DisconnectCause.MISSED
                        code == 486 || code == 600 -> DisconnectCause.BUSY
                        code >= 400 -> DisconnectCause.ERROR
                        else -> DisconnectCause.REMOTE
                    }
                    telecom?.disconnect(cause)
                    main.postDelayed({ finish(callId) }, if (c.incoming) 0 else ENDED_LINGER_MS)
                }
                else -> Unit
            }
            updateAudioState()
            SipService.sync(app)
        }
    }

    override fun onCallMedia(callId: Int, active: Boolean, localHold: Boolean) {
        _call.update { c ->
            if (c?.id != callId) c
            else c.copy(onHold = localHold, earlyMedia = active && c.phase != Phase.Active)
        }
        main.post { updateAudioState() }
    }

    // --- end of call (main thread) ------------------------------------------------------

    private fun record(c: CallUi) {
        if (c.number.isEmpty()) return
        // Declined on purpose is not "missed"; a caller who gave up (487) is.
        val missed = c.incoming && !c.answered && c.code != 603
        val duration = if (c.connectedAt > 0) ((SystemClock.elapsedRealtime() - c.connectedAt) / 1000).toInt() else 0
        log.add(CallRecord(0, c.accountId, c.number, c.name, c.incoming, missed, c.startedAt, duration))
        if (missed) Notifications.missed(app, c)
    }

    /** Clears the call and releases audio, the service and maybe the SIP stack. */
    private fun finish(callId: Int? = null) {
        val c = _call.value ?: return
        if (callId != null && c.id != callId) return
        _call.value = null
        record(c)
        telecom?.disconnect(DisconnectCause.LOCAL)
        telecom = null
        endpoints = emptyList()
        fallbackAudio?.stop()
        fallbackAudio = null
        ringWake?.takeIf { it.isHeld }?.release()
        updateAudioState()
        SipService.sync(app)
        sip.post { if (!engineWanted()) stopEngine() }
    }
}
