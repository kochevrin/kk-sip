package io.github.kochevrin.kksip.ui

import android.os.SystemClock
import android.text.format.DateUtils
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.sip.CallUi
import io.github.kochevrin.kksip.sip.Phase
import io.github.kochevrin.kksip.sip.Route
import io.github.kochevrin.kksip.sip.RouteType
import kotlinx.coroutines.delay

/** Header shared by the incoming and the in-call screen: avatar, name, number, status. */
@Composable
private fun CallHeader(call: CallUi, status: String, statusIsError: Boolean, ringing: Boolean) {
    val colors = MaterialTheme.colorScheme
    Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
        Spacer(Modifier.height(48.dp))
        Avatar(call.title, 112.dp, Modifier.ringPulse(ringing, Orange))
        Spacer(Modifier.height(24.dp))
        Text(
            call.title, fontSize = 30.sp, color = colors.onBackground, textAlign = TextAlign.Center,
            maxLines = 2, overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(horizontal = 24.dp),
        )
        if (call.name.isNotEmpty()) Text(call.number, color = colors.onSurfaceVariant, fontSize = 16.sp)
        Spacer(Modifier.height(8.dp))
        AnimatedContent(
            status,
            transitionSpec = { fadeIn(tween(200)) togetherWith fadeOut(tween(200)) },
            label = "status",
        ) { s ->
            Text(
                s, fontSize = 17.sp,
                color = if (statusIsError) LocalCallColors.current.danger else colors.onSurfaceVariant,
            )
        }
    }
}

@Composable
fun IncomingScreen(call: CallUi, accountTitle: String, onAnswer: () -> Unit, onDecline: () -> Unit) {
    val colors = MaterialTheme.colorScheme
    val callColors = LocalCallColors.current
    // The answer button bobs gently, like the system dialer's.
    val bob = rememberInfiniteTransition(label = "bob")
    val lift by bob.animateFloat(0f, -10f, infiniteRepeatable(tween(700), RepeatMode.Reverse), label = "lift")
    Column(
        Modifier.fillMaxSize().background(colors.background).safeDrawingPadding().padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        CallHeader(call, stringResource(R.string.incoming_call), statusIsError = false, ringing = true)
        if (accountTitle.isNotEmpty()) {
            Text(stringResource(R.string.to_account, accountTitle), fontSize = 13.sp, color = colors.onSurfaceVariant)
        }
        Spacer(Modifier.weight(1f))
        Row(Modifier.fillMaxWidth().padding(horizontal = 24.dp), horizontalArrangement = Arrangement.SpaceBetween) {
            LabeledRound(R.drawable.ic_hangup, stringResource(R.string.decline), callColors.dangerFill, onClick = onDecline)
            LabeledRound(
                R.drawable.ic_call, stringResource(R.string.answer), callColors.okFill,
                Modifier.graphicsLayer { translationY = lift.dp.toPx() }, onAnswer,
            )
        }
        Spacer(Modifier.height(32.dp))
    }
}

@Composable
private fun LabeledRound(icon: Int, label: String, fill: androidx.compose.ui.graphics.Color,
                         modifier: Modifier = Modifier, onClick: () -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        RoundButton(icon, label, fill, size = 76.dp, modifier = modifier, onClick = onClick)
        Spacer(Modifier.height(8.dp))
        Text(label, fontSize = 14.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
fun CallScreen(
    call: CallUi,
    onHangup: () -> Unit,
    onMute: () -> Unit,
    onSpeaker: () -> Unit,
    onRoute: (Route) -> Unit,
    onHold: () -> Unit,
    onDtmf: (Char) -> Unit,
) {
    val colors = MaterialTheme.colorScheme
    var showKeypad by rememberSaveable { mutableStateOf(false) }
    var dtmfTyped by rememberSaveable { mutableStateOf("") }

    var now by remember { mutableLongStateOf(SystemClock.elapsedRealtime()) }
    LaunchedEffect(call.phase) {
        while (call.phase == Phase.Active) {
            now = SystemClock.elapsedRealtime()
            delay(500)
        }
    }
    val status = when (call.phase) {
        Phase.Incoming, Phase.Connecting -> stringResource(R.string.connecting)
        Phase.Calling -> stringResource(R.string.calling)
        Phase.Ringing -> stringResource(R.string.ringing)
        Phase.Active -> if (call.onHold) stringResource(R.string.on_hold)
        else DateUtils.formatElapsedTime((now - call.connectedAt).coerceAtLeast(0) / 1000)
        Phase.Ended -> if (call.code >= 300 && call.reason.isNotEmpty()) "${call.reason} (${call.code})"
        else stringResource(R.string.call_ended)
    }
    val active = call.phase == Phase.Active
    val live = call.phase != Phase.Ended

    Column(
        Modifier.fillMaxSize().background(colors.background).safeDrawingPadding().padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        CallHeader(
            call, status,
            statusIsError = call.phase == Phase.Ended && call.code >= 300,
            ringing = call.phase == Phase.Calling || call.phase == Phase.Ringing,
        )
        Spacer(Modifier.weight(1f))
        AnimatedContent(
            showKeypad && active,
            transitionSpec = {
                (fadeIn() + slideInVertically { it / 6 }) togetherWith (fadeOut() + slideOutVertically { it / 6 })
            },
            label = "controls",
        ) { keypad ->
            if (keypad) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(dtmfTyped, color = colors.onBackground, fontSize = 24.sp, maxLines = 1,
                        overflow = TextOverflow.StartEllipsis)
                    Spacer(Modifier.height(8.dp))
                    Keypad(onKey = { if (it != '+') { dtmfTyped += it; onDtmf(it) } })
                    KkTextButton(stringResource(R.string.hide)) { showKeypad = false }
                }
            } else {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                        ToggleButton(
                            if (call.muted) R.drawable.ic_mic_off else R.drawable.ic_mic,
                            stringResource(R.string.mute), call.muted, live, onMute,
                        )
                        ToggleButton(R.drawable.ic_dialpad, stringResource(R.string.keypad), false, active) {
                            showKeypad = true
                        }
                    }
                    Spacer(Modifier.height(16.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                        AudioButton(call, live, onSpeaker, onRoute)
                        ToggleButton(R.drawable.ic_pause, stringResource(R.string.hold), call.onHold, active, onHold)
                    }
                }
            }
        }
        Spacer(Modifier.weight(1f))
        RoundButton(
            icon = R.drawable.ic_hangup,
            label = stringResource(R.string.hang_up),
            fill = LocalCallColors.current.dangerFill,
            enabled = live,
            onClick = onHangup,
        )
        Spacer(Modifier.height(24.dp))
    }
}

/** Speaker on/off; with a headset connected it becomes a route picker like the system dialer's. */
@Composable
private fun AudioButton(call: CallUi, enabled: Boolean, onSpeaker: () -> Unit, onRoute: (Route) -> Unit) {
    val headset = call.routes.any { it.type == RouteType.Bluetooth || it.type == RouteType.Wired }
    var menu by remember { mutableStateOf(false) }
    Box {
        ToggleButton(
            iconFor(call.route.type).takeIf { headset } ?: R.drawable.ic_speaker,
            if (headset) stringResource(R.string.audio) else stringResource(R.string.speaker),
            checked = !headset && call.route.type == RouteType.Speaker,
            enabled = enabled,
        ) { if (headset) menu = true else onSpeaker() }
        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
            for (r in call.routes) {
                DropdownMenuItem(
                    text = { Text(routeName(r)) },
                    leadingIcon = { Icon(painterResource(iconFor(r.type)), null, Modifier.size(20.dp)) },
                    trailingIcon = if (r == call.route) {
                        { Icon(painterResource(R.drawable.ic_check), null, Modifier.size(18.dp)) }
                    } else null,
                    onClick = { onRoute(r); menu = false },
                )
            }
        }
    }
}

private fun iconFor(t: RouteType) = when (t) {
    RouteType.Speaker -> R.drawable.ic_speaker
    RouteType.Bluetooth -> R.drawable.ic_bluetooth
    RouteType.Wired -> R.drawable.ic_headset
    else -> R.drawable.ic_phone_ear
}

@Composable
private fun routeName(r: Route) = when (r.type) {
    RouteType.Earpiece -> stringResource(R.string.route_phone)
    RouteType.Speaker -> stringResource(R.string.speaker)
    else -> r.name.ifEmpty { stringResource(R.string.route_headset) }
}
