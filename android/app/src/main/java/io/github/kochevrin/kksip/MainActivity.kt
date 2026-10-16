package io.github.kochevrin.kksip

import android.Manifest
import android.app.AlarmManager
import android.app.NotificationManager
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import io.github.kochevrin.kksip.provision.IniImport
import io.github.kochevrin.kksip.sip.Account
import io.github.kochevrin.kksip.sip.CallManager
import io.github.kochevrin.kksip.sip.CallUi
import io.github.kochevrin.kksip.sip.Phase
import io.github.kochevrin.kksip.sip.SipUri
import io.github.kochevrin.kksip.ui.AccountScreen
import io.github.kochevrin.kksip.ui.CallScreen
import io.github.kochevrin.kksip.ui.IncomingScreen
import io.github.kochevrin.kksip.ui.KkTheme
import io.github.kochevrin.kksip.ui.MainScreen
import io.github.kochevrin.kksip.ui.SettingsScreen
import io.github.kochevrin.kksip.ui.SetupHint
import io.github.kochevrin.kksip.ui.Tab
import io.github.kochevrin.kksip.ui.ImportDialog
import io.github.kochevrin.kksip.ui.WelcomeScreen
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    /** What an intent asked for: a number for the dial pad, Recents, or a call back. */
    private val request = mutableStateOf<Request?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        if (savedInstanceState == null) take(intent)
        // The call screen may show over the lock screen and wake it, like the phone app's.
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.CREATED) {
                CallManager.call.collect { c ->
                    setShowWhenLocked(c != null)
                    setTurnScreenOn(c?.phase == Phase.Incoming)
                }
            }
        }
        setContent { KkTheme { AppRoot(request) } }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        take(intent)
    }

    private fun take(intent: Intent?) {
        intent ?: return
        request.value = when (intent.action) {
            ACTION_SHOW_RECENTS -> Request.Recents
            ACTION_CALL_BACK -> intent.getStringExtra(EXTRA_NUMBER)?.let {
                Request.CallBack(it, intent.getStringExtra(EXTRA_ACCOUNT))
            }
            Intent.ACTION_VIEW, Intent.ACTION_DIAL -> intent.data?.let { uri ->
                when (uri.scheme) {
                    "kksip" -> IniImport.fromLink(uri)?.let { Request.Import(it) }
                    "tel", "sip" -> Request.Number(SipUri.cleanNumber(Uri.decode(uri.schemeSpecificPart).removePrefix("//")))
                    else -> null
                }
            }
            else -> null
        } ?: request.value
    }

    sealed interface Request {
        data class Number(val number: String) : Request
        data class CallBack(val number: String, val accountId: String?) : Request
        data object Recents : Request
        /** INI text from a kksip://import link (the QR code from the computer). */
        data class Import(val ini: String) : Request
    }

    companion object {
        const val ACTION_SHOW_CALL = "io.github.kochevrin.kksip.SHOW_CALL"
        const val ACTION_SHOW_RECENTS = "io.github.kochevrin.kksip.SHOW_RECENTS"
        const val ACTION_CALL_BACK = "io.github.kochevrin.kksip.CALL_BACK"
        const val EXTRA_NUMBER = "number"
        const val EXTRA_ACCOUNT = "account"
    }
}

/** Which screen is up; [call] travels along so a screen sliding out still has its data. */
private data class Ui(val key: String, val call: CallUi? = null)

@Composable
private fun AppRoot(request: MutableState<MainActivity.Request?>) {
    val context = LocalContext.current
    val accounts by CallManager.accounts.collectAsStateWithLifecycle()
    val currentId by CallManager.currentId.collectAsStateWithLifecycle()
    val receiveCalls by CallManager.receiveCalls.collectAsStateWithLifecycle()
    val reg by CallManager.reg.collectAsStateWithLifecycle()
    val call by CallManager.call.collectAsStateWithLifecycle()
    val error by CallManager.error.collectAsStateWithLifecycle()
    val records by CallManager.log.records.collectAsStateWithLifecycle()
    val contacts by CallManager.contacts.contacts.collectAsStateWithLifecycle()
    val usable = accounts.filter { it.isUsable }
    val current = usable.firstOrNull { it.id == currentId } ?: usable.firstOrNull()

    var route by rememberSaveable { mutableStateOf("main") } // main | settings | account:<id> | account:new
    var tab by rememberSaveable { mutableStateOf(Tab.Keypad) }
    var number by rememberSaveable { mutableStateOf("") }
    var hintsVersion by remember { mutableIntStateOf(0) }
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { hintsVersion++ } // back from system settings

    LaunchedEffect(error) {
        error?.let {
            Toast.makeText(context, it, Toast.LENGTH_LONG).show()
            CallManager.clearError()
        }
    }

    // Runtime permissions: the microphone for any call, notifications for the call banner,
    // Bluetooth to list headsets. Asked when they are first needed.
    var afterGrant by remember { mutableStateOf<(() -> Unit)?>(null) }
    val permissions = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { granted ->
        val mic = granted[Manifest.permission.RECORD_AUDIO] ?: hasPermission(context, Manifest.permission.RECORD_AUDIO)
        if (mic) afterGrant?.invoke()
        else Toast.makeText(context, R.string.permission_mic, Toast.LENGTH_LONG).show()
        afterGrant = null
    }
    fun withCallPermissions(action: () -> Unit) {
        val needed = buildList {
            add(Manifest.permission.RECORD_AUDIO)
            if (Build.VERSION.SDK_INT >= 33) add(Manifest.permission.POST_NOTIFICATIONS)
            if (Build.VERSION.SDK_INT >= 31) add(Manifest.permission.BLUETOOTH_CONNECT)
        }.filterNot { hasPermission(context, it) }
        if (needed.isEmpty()) action()
        else { afterGrant = action; permissions.launch(needed.toTypedArray()) }
    }

    // First start: ask for what calls need right away, as the phone app does.
    val firstPermissions = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { }
    LaunchedEffect(Unit) {
        val prefs = Prefs(context)
        if (prefs.permissionsAsked) return@LaunchedEffect
        prefs.permissionsAsked = true
        val needed = buildList {
            add(Manifest.permission.RECORD_AUDIO)
            if (Build.VERSION.SDK_INT >= 33) add(Manifest.permission.POST_NOTIFICATIONS)
            if (Build.VERSION.SDK_INT >= 31) add(Manifest.permission.BLUETOOTH_CONNECT)
        }.filterNot { hasPermission(context, it) }
        if (needed.isNotEmpty()) firstPermissions.launch(needed.toTypedArray())
    }

    // The QR scanner needs the camera, asked for when the user opens it.
    val cameraPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { ok ->
        if (ok) route = "scan" else Toast.makeText(context, R.string.permission_camera, Toast.LENGTH_LONG).show()
    }
    fun openScanner() {
        if (hasPermission(context, Manifest.permission.CAMERA)) route = "scan"
        else cameraPermission.launch(Manifest.permission.CAMERA)
    }
    var debugLog by remember { mutableStateOf(Prefs(context).debugLog) }

    fun dial(target: String, accountId: String?) {
        val account = usable.firstOrNull { it.id == accountId } ?: current ?: return
        if (target.isBlank()) return
        withCallPermissions { CallManager.makeCall(account, target) }
    }

    // Accounts waiting for the user's OK, from a file or a QR link.
    var pendingImport by remember { mutableStateOf<IniImport.Result?>(null) }
    fun offerImport(text: String?) {
        val r = text?.let { IniImport.parse(it) }
        if (r == null || r.accounts.isEmpty()) Toast.makeText(context, R.string.import_nothing, Toast.LENGTH_LONG).show()
        else pendingImport = r
    }
    val pickFile = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) offerImport(
            runCatching { context.contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) } }.getOrNull()
        )
    }
    // Contacts: CSV in and out (the desktop's format), and picking from the phone's book.
    val importCsv = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        val text = uri?.let { u ->
            runCatching { context.contentResolver.openInputStream(u)?.use { it.readBytes().toString(Charsets.UTF_8) } }.getOrNull()
        } ?: return@rememberLauncherForActivityResult
        val found = io.github.kochevrin.kksip.data.ContactBook.parseCsv(text)
        CallManager.contacts.merge(found) { added ->
            android.os.Handler(android.os.Looper.getMainLooper()).post {
                Toast.makeText(context, context.getString(R.string.contacts_imported, added, found.size), Toast.LENGTH_SHORT).show()
            }
        }
    }
    val exportCsv = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/csv")) { uri ->
        if (uri != null) runCatching {
            context.contentResolver.openOutputStream(uri)?.use {
                it.write(io.github.kochevrin.kksip.data.ContactBook.toCsv(contacts).toByteArray())
            }
        }
    }
    // The system picker hands over just the chosen number; no permission to read the book.
    val pickPhoneContact = rememberLauncherForActivityResult(PickPhoneNumber()) { uri ->
        uri ?: return@rememberLauncherForActivityResult
        context.contentResolver.query(
            uri,
            arrayOf(android.provider.ContactsContract.CommonDataKinds.Phone.DISPLAY_NAME,
                android.provider.ContactsContract.CommonDataKinds.Phone.NUMBER),
            null, null, null,
        )?.use { c ->
            if (c.moveToFirst()) CallManager.contacts.save(
                io.github.kochevrin.kksip.data.Contact(0, c.getString(0).orEmpty(), SipUri.cleanNumber(c.getString(1).orEmpty()))
            )
        }
    }
    val contactActions = remember {
        io.github.kochevrin.kksip.ui.ContactActions(
            save = { CallManager.contacts.save(it) },
            delete = { CallManager.contacts.delete(it.id) },
            importCsv = { importCsv.launch(arrayOf("text/*", "application/octet-stream")) },
            exportCsv = { exportCsv.launch("contacts.csv") },
            pickFromPhone = { pickPhoneContact.launch(Unit) },
        )
    }

    pendingImport?.let { r ->
        ImportDialog(
            accounts = r.accounts,
            onImport = {
                CallManager.importAccounts(r.accounts)
                pendingImport = null
                route = "main"
                Toast.makeText(context, context.getString(R.string.imported_n, r.accounts.size), Toast.LENGTH_SHORT).show()
            },
            onCancel = { pendingImport = null },
        )
    }

    LaunchedEffect(request.value) {
        when (val r = request.value) {
            is MainActivity.Request.Number -> { number = r.number; tab = Tab.Keypad; route = "main" }
            is MainActivity.Request.CallBack -> dial(r.number, r.accountId)
            MainActivity.Request.Recents -> { tab = Tab.Recents; route = "main" }
            is MainActivity.Request.Import -> offerImport(r.ini)
            null -> Unit
        }
        request.value = null
    }

    val c = call
    val ui = when {
        c != null && c.phase == Phase.Incoming -> Ui("incoming", c)
        c != null -> Ui("call", c)
        accounts.isEmpty() && !route.startsWith("account:") && route != "scan" -> Ui("welcome")
        else -> Ui(route)
    }

    AnimatedContent(
        ui,
        contentKey = { it.key },
        transitionSpec = {
            when {
                // Calls rise from the bottom and sink back, like the system dialer.
                targetState.call != null && initialState.call == null ->
                    (slideInVertically(tween(320)) { it / 3 } + fadeIn(tween(320)) + scaleIn(initialScale = 0.96f)) togetherWith
                        fadeOut(tween(200))
                initialState.call != null && targetState.call == null ->
                    fadeIn(tween(250)) togetherWith (slideOutVertically(tween(300)) { it / 3 } + fadeOut(tween(300)))
                targetState.key != "main" && initialState.key == "main" ->
                    (slideInHorizontally { it / 4 } + fadeIn()) togetherWith (slideOutHorizontally { -it / 8 } + fadeOut())
                else ->
                    (slideInHorizontally { -it / 8 } + fadeIn()) togetherWith (slideOutHorizontally { it / 4 } + fadeOut())
            }
        },
        label = "screen",
    ) { s ->
        when {
            s.key == "incoming" -> IncomingScreen(
                call = s.call!!,
                accountTitle = accounts.firstOrNull { it.id == s.call.accountId }?.title.orEmpty(),
                onAnswer = { withCallPermissions { CallManager.answer() } },
                onDecline = CallManager::decline,
            )

            s.key == "call" -> CallScreen(
                call = s.call!!,
                onHangup = CallManager::hangup,
                onMute = CallManager::toggleMute,
                onSpeaker = CallManager::toggleSpeaker,
                onRoute = CallManager::setRoute,
                onHold = CallManager::toggleHold,
                onDtmf = CallManager::dtmf,
                onTransfer = CallManager::transfer,
            )

            s.key == "welcome" -> WelcomeScreen(
                onScan = ::openScanner,
                onImport = { pickFile.launch(arrayOf("*/*")) },
                onManual = { route = "account:new" },
            )

            s.key == "scan" -> {
                val back = if (accounts.isEmpty()) "main" else "settings"
                BackHandler { route = back }
                io.github.kochevrin.kksip.ui.QrScanScreen(
                    onResult = { text ->
                        route = back
                        val ini = runCatching { IniImport.fromLink(Uri.parse(text.trim())) }.getOrNull()
                        if (ini != null) offerImport(ini)
                        else Toast.makeText(context, R.string.not_kksip_qr, Toast.LENGTH_LONG).show()
                    },
                    onClose = { route = back },
                )
            }

            s.key.startsWith("account:") -> {
                val id = s.key.removePrefix("account:")
                val existing = accounts.firstOrNull { it.id == id }
                val back = if (accounts.isEmpty()) "main" else "settings"
                BackHandler { route = back }
                AccountScreen(
                    initial = existing ?: Account(),
                    canCancel = true,
                    onSave = { CallManager.saveAccount(it); route = if (accounts.isEmpty()) "main" else "settings" },
                    onCancel = { route = back },
                    onDelete = existing?.let { a -> { CallManager.deleteAccount(a.id); route = "settings" } },
                )
            }

            s.key == "settings" -> {
                BackHandler { route = "main" }
                SettingsScreen(
                    accounts = accounts,
                    reg = reg,
                    receiveCalls = receiveCalls,
                    hints = remember(hintsVersion, receiveCalls) { setupHints(context, receiveCalls) },
                    onBack = { route = "main" },
                    onEdit = { route = "account:${it.id}" },
                    onAdd = { route = "account:new" },
                    onImport = { pickFile.launch(arrayOf("*/*")) },
                    onScan = ::openScanner,
                    debugLog = debugLog,
                    onDebugLog = { debugLog = it; CallManager.setDebugLog(it) },
                    onShareLog = { shareLog(context) },
                    onReceiveCalls = { on ->
                        if (on) withCallPermissions {
                            CallManager.setReceiveCalls(true)
                            askBatteryOnce(context)
                        } else CallManager.setReceiveCalls(false)
                    },
                )
            }

            else -> {
                BackHandler(enabled = tab != Tab.Keypad) { tab = Tab.Keypad }
                MainScreen(
                    tab = tab, onTab = { tab = it },
                    account = current, accounts = usable, reg = reg, receiveCalls = receiveCalls,
                    number = number, onNumber = { number = it },
                    records = records,
                    contacts = contacts,
                    contactActions = contactActions,
                    onDial = ::dial,
                    onDeleteRecord = { CallManager.log.delete(it.id) },
                    onSelectAccount = CallManager::selectAccount,
                    onSettings = { route = "settings" },
                )
            }
        }
    }
}

/** Hands the debug log to any app (mail, messenger) through a content URI. */
private fun shareLog(context: android.content.Context) {
    val f = CallManager.logFile()
    if (!f.exists() || f.length() == 0L) {
        Toast.makeText(context, R.string.log_empty, Toast.LENGTH_LONG).show()
        return
    }
    val uri = androidx.core.content.FileProvider.getUriForFile(context, "${context.packageName}.logs", f)
    val send = Intent(Intent.ACTION_SEND).setType("text/plain")
        .putExtra(Intent.EXTRA_STREAM, uri)
        .putExtra(Intent.EXTRA_SUBJECT, "kk-sip ${BuildConfig.VERSION_NAME} log")
        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    context.startActivity(Intent.createChooser(send, context.getString(R.string.share_log)))
}

private fun hasPermission(context: android.content.Context, p: String) =
    ContextCompat.checkSelfPermission(context, p) == PackageManager.PERMISSION_GRANTED

/** The first time "Receive calls" goes on, ask to be left out of battery optimisation. */
private fun askBatteryOnce(context: android.content.Context) {
    val prefs = Prefs(context)
    val pm = context.getSystemService(PowerManager::class.java)
    if (prefs.batteryAsked || pm.isIgnoringBatteryOptimizations(context.packageName)) return
    prefs.batteryAsked = true
    requestBattery(context)
}

@android.annotation.SuppressLint("BatteryLife") // a calling app without push is an allowed case
private fun requestBattery(context: android.content.Context) {
    runCatching {
        context.startActivity(
            Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:${context.packageName}"))
        )
    }
}

/** System settings that stop calls from coming in reliably, each with the screen that fixes it. */
private fun setupHints(context: android.content.Context, receiveCalls: Boolean): List<SetupHint> {
    val hints = mutableListOf<SetupHint>()
    val nm = context.getSystemService(NotificationManager::class.java)
    val appSettings = {
        context.startActivity(
            Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
        )
    }
    if (!nm.areNotificationsEnabled()) {
        hints += SetupHint(context.getString(R.string.hint_notifications), context.getString(R.string.hint_notifications_text), appSettings)
    }
    if (!receiveCalls) return hints
    if (Build.VERSION.SDK_INT >= 34 && !nm.canUseFullScreenIntent()) {
        hints += SetupHint(context.getString(R.string.hint_fullscreen), context.getString(R.string.hint_fullscreen_text)) {
            context.startActivity(
                Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT, Uri.parse("package:${context.packageName}"))
            )
        }
    }
    val pm = context.getSystemService(PowerManager::class.java)
    if (!pm.isIgnoringBatteryOptimizations(context.packageName)) {
        hints += SetupHint(context.getString(R.string.hint_battery), context.getString(R.string.hint_battery_text)) {
            requestBattery(context)
        }
    }
    if (Build.VERSION.SDK_INT >= 31 && !context.getSystemService(AlarmManager::class.java).canScheduleExactAlarms()) {
        hints += SetupHint(context.getString(R.string.hint_alarms), context.getString(R.string.hint_alarms_text)) {
            context.startActivity(
                Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM, Uri.parse("package:${context.packageName}"))
            )
        }
    }
    return hints
}

/** ACTION_PICK on phone numbers: the result URI is readable without READ_CONTACTS. */
private class PickPhoneNumber : androidx.activity.result.contract.ActivityResultContract<Unit, Uri?>() {
    override fun createIntent(context: android.content.Context, input: Unit) =
        Intent(Intent.ACTION_PICK).setType(android.provider.ContactsContract.CommonDataKinds.Phone.CONTENT_TYPE)

    override fun parseResult(resultCode: Int, intent: Intent?): Uri? =
        intent?.data?.takeIf { resultCode == android.app.Activity.RESULT_OK }
}
