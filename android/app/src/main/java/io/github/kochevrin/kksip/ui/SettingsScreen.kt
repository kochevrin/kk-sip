package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.BuildConfig
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.sip.Account
import io.github.kochevrin.kksip.sip.Reg
import io.github.kochevrin.kksip.sip.RegStatus

/** A fix the user still has to allow in system settings, shown as a warning row. */
data class SetupHint(val title: String, val text: String, val action: () -> Unit)

@Composable
fun SettingsScreen(
    accounts: List<Account>,
    reg: Map<String, RegStatus>,
    receiveCalls: Boolean,
    hints: List<SetupHint>,
    onBack: () -> Unit,
    onEdit: (Account) -> Unit,
    onAdd: () -> Unit,
    onImport: () -> Unit,
    onScan: () -> Unit,
    onReceiveCalls: (Boolean) -> Unit,
    debugLog: Boolean,
    onDebugLog: (Boolean) -> Unit,
    onShareLog: () -> Unit,
) {
    val colors = MaterialTheme.colorScheme
    Column(Modifier.fillMaxSize().background(colors.background).safeDrawingPadding()) {
        Row(Modifier.fillMaxWidth().height(56.dp).padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(painterResource(R.drawable.ic_back), stringResource(R.string.back), tint = colors.onBackground)
            }
            Text(stringResource(R.string.settings), style = MaterialTheme.typography.titleLarge, color = colors.onBackground)
        }
        Column(
            Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Section(stringResource(R.string.accounts)) {
                accounts.forEachIndexed { i, a ->
                    if (i > 0) HorizontalDivider(color = colors.outlineVariant)
                    Row(
                        Modifier.fillMaxWidth().clickable { onEdit(a) }.padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Avatar(a.label.ifEmpty { a.displayName }, 36.dp,
                            Modifier.graphicsLayer { alpha = if (a.enabled) 1f else 0.4f })
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(a.title, color = if (a.enabled) colors.onSurface else colors.onSurfaceVariant,
                                maxLines = 1, overflow = TextOverflow.Ellipsis)
                            Text(a.server, color = colors.onSurfaceVariant, fontSize = 13.sp, maxLines = 1)
                        }
                        if (!a.enabled) Text(stringResource(R.string.account_off), fontSize = 12.sp, color = colors.onSurfaceVariant)
                        else if (receiveCalls) RegLabel(reg[a.id])
                    }
                }
                if (accounts.isNotEmpty()) HorizontalDivider(color = colors.outlineVariant)
                Row(
                    Modifier.fillMaxWidth().clickable(onClick = onAdd).padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(painterResource(R.drawable.ic_plus), null, tint = LocalCallColors.current.link)
                    Spacer(Modifier.width(12.dp))
                    Text(stringResource(R.string.add_account), color = LocalCallColors.current.link)
                }
                HorizontalDivider(color = colors.outlineVariant)
                Row(
                    Modifier.fillMaxWidth().clickable(onClick = onImport).padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(painterResource(R.drawable.ic_import), null, tint = LocalCallColors.current.link)
                    Spacer(Modifier.width(12.dp))
                    Text(stringResource(R.string.import_from_file), color = LocalCallColors.current.link)
                }
                HorizontalDivider(color = colors.outlineVariant)
                Row(
                    Modifier.fillMaxWidth().clickable(onClick = onScan).padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(painterResource(R.drawable.ic_qr), null, tint = LocalCallColors.current.link)
                    Spacer(Modifier.width(12.dp))
                    Text(stringResource(R.string.scan_qr), color = LocalCallColors.current.link)
                }
            }

            Section(stringResource(R.string.calls)) {
                Row(
                    Modifier.fillMaxWidth().clickable { onReceiveCalls(!receiveCalls) }.padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.receive_calls), color = colors.onSurface)
                        Text(stringResource(R.string.receive_calls_hint), color = colors.onSurfaceVariant, fontSize = 13.sp)
                    }
                    Spacer(Modifier.width(12.dp))
                    Switch(
                        receiveCalls, onReceiveCalls,
                        colors = SwitchDefaults.colors(checkedTrackColor = colors.primary, checkedThumbColor = colors.onPrimary),
                    )
                }
                for (h in hints) {
                    HorizontalDivider(color = colors.outlineVariant)
                    Row(
                        Modifier.fillMaxWidth().clickable(onClick = h.action).padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        StatusDot(LocalCallColors.current.warn)
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(h.title, color = colors.onSurface)
                            Text(h.text, color = colors.onSurfaceVariant, fontSize = 13.sp)
                        }
                    }
                }
            }

            Section(stringResource(R.string.troubleshooting)) {
                Row(
                    Modifier.fillMaxWidth().clickable { onDebugLog(!debugLog) }.padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.debug_log), color = colors.onSurface)
                        Text(stringResource(R.string.debug_log_hint), color = colors.onSurfaceVariant, fontSize = 13.sp)
                    }
                    Spacer(Modifier.width(12.dp))
                    Switch(
                        debugLog, onDebugLog,
                        colors = SwitchDefaults.colors(checkedTrackColor = colors.primary, checkedThumbColor = colors.onPrimary),
                    )
                }
                HorizontalDivider(color = colors.outlineVariant)
                Row(
                    Modifier.fillMaxWidth().clickable(onClick = onShareLog).padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(stringResource(R.string.share_log), color = LocalCallColors.current.link)
                }
            }

            Text(
                "kk-sip ${BuildConfig.VERSION_NAME} · GPL-2.0-or-later",
                Modifier.fillMaxWidth().padding(8.dp),
                color = colors.onSurfaceVariant, fontSize = 12.sp,
            )
        }
    }
}

@Composable
private fun Section(title: String, content: @Composable () -> Unit) {
    val colors = MaterialTheme.colorScheme
    Column {
        Text(title, Modifier.padding(start = 4.dp, bottom = 6.dp), style = MaterialTheme.typography.labelLarge,
            color = colors.onSurfaceVariant)
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(colors.surface)) { content() }
    }
}

@Composable
fun regColor(s: RegStatus?): Color {
    val c = LocalCallColors.current
    return when (s?.state) {
        Reg.Online -> c.ok
        Reg.Failed -> c.danger
        Reg.Registering -> c.warn
        else -> MaterialTheme.colorScheme.outline
    }
}

@Composable
fun regText(s: RegStatus?): String = when (s?.state) {
    Reg.Online -> stringResource(R.string.reg_online)
    Reg.Failed -> if (s.code > 0) "${s.reason} (${s.code})" else s.reason.ifEmpty { stringResource(R.string.reg_failed) }
    Reg.Registering -> stringResource(R.string.reg_connecting)
    else -> stringResource(R.string.reg_off)
}

@Composable
private fun RegLabel(s: RegStatus?) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        StatusDot(regColor(s))
        Spacer(Modifier.width(6.dp))
        Text(regText(s), fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1,
            modifier = Modifier.widthIn(max = 120.dp), overflow = TextOverflow.Ellipsis)
    }
}
