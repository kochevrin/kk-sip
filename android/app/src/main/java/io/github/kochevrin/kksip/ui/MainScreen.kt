package io.github.kochevrin.kksip.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.data.CallRecord
import io.github.kochevrin.kksip.sip.Account
import io.github.kochevrin.kksip.sip.RegStatus

enum class Tab { Keypad, Recents }

/** Dial pad and recents under one bar: the account switcher on top, tabs below. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    tab: Tab,
    onTab: (Tab) -> Unit,
    account: Account?,
    accounts: List<Account>,
    reg: Map<String, RegStatus>,
    receiveCalls: Boolean,
    number: String,
    onNumber: (String) -> Unit,
    records: List<CallRecord>,
    onDial: (String, String?) -> Unit, // number, account id
    onDeleteRecord: (CallRecord) -> Unit,
    onSelectAccount: (String) -> Unit,
    onSettings: () -> Unit,
) {
    val colors = MaterialTheme.colorScheme
    var sheet by rememberSaveable { mutableStateOf(false) }
    Scaffold(
        containerColor = colors.background,
        topBar = {
            Row(
                Modifier.fillMaxWidth().statusBarsPadding().height(60.dp).padding(start = 8.dp, end = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Row(
                    Modifier
                        .weight(1f, fill = false)
                        .clip(RoundedCornerShape(12.dp))
                        .clickable { sheet = true }
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    if (receiveCalls && account != null) {
                        StatusDot(regColor(reg[account.id]))
                        Spacer(Modifier.width(8.dp))
                    }
                    Text(
                        account?.title ?: stringResource(R.string.app_name),
                        style = MaterialTheme.typography.titleMedium, color = colors.onBackground,
                        maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false),
                    )
                    Icon(painterResource(R.drawable.ic_chevron_down), null, Modifier.size(20.dp),
                        tint = colors.onSurfaceVariant)
                }
                IconButton(onClick = onSettings) {
                    Icon(painterResource(R.drawable.ic_settings), stringResource(R.string.settings),
                        tint = colors.onBackground)
                }
            }
        },
        bottomBar = {
            NavigationBar(containerColor = colors.surface, tonalElevation = 0.dp) {
                val itemColors = NavigationBarItemDefaults.colors(
                    selectedIconColor = colors.onPrimary,
                    selectedTextColor = colors.onBackground,
                    indicatorColor = colors.primary,
                    unselectedIconColor = colors.onSurfaceVariant,
                    unselectedTextColor = colors.onSurfaceVariant,
                )
                NavigationBarItem(
                    selected = tab == Tab.Keypad, onClick = { onTab(Tab.Keypad) },
                    icon = { Icon(painterResource(R.drawable.ic_dialpad), null) },
                    label = { Text(stringResource(R.string.keypad)) }, colors = itemColors,
                )
                NavigationBarItem(
                    selected = tab == Tab.Recents, onClick = { onTab(Tab.Recents) },
                    icon = { Icon(painterResource(R.drawable.ic_history), null) },
                    label = { Text(stringResource(R.string.recents)) }, colors = itemColors,
                )
            }
        },
    ) { padding ->
        AnimatedContent(
            tab,
            transitionSpec = { fadeIn() togetherWith fadeOut() },
            label = "tab",
            modifier = Modifier.padding(padding),
        ) { t ->
            when (t) {
                Tab.Keypad -> DialerScreen(
                    number = number, onNumber = onNumber, canCall = account != null,
                    onCall = { onDial(number, null) },
                )
                Tab.Recents -> RecentsScreen(
                    records = records,
                    onCall = { onDial(it.number, it.accountId) },
                    onDelete = onDeleteRecord,
                )
            }
        }
    }

    if (sheet) {
        ModalBottomSheet(onDismissRequest = { sheet = false }, containerColor = colors.surface) {
            Column(Modifier.navigationBarsPadding().padding(bottom = 8.dp)) {
                Text(
                    stringResource(R.string.call_from), Modifier.padding(horizontal = 24.dp, vertical = 8.dp),
                    style = MaterialTheme.typography.labelLarge, color = colors.onSurfaceVariant,
                )
                for (a in accounts) {
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .clickable { onSelectAccount(a.id); sheet = false }
                            .padding(horizontal = 24.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Avatar(a.label.ifEmpty { a.displayName }, 40.dp)
                        Spacer(Modifier.width(14.dp))
                        Column(Modifier.weight(1f)) {
                            Text(a.title, color = colors.onSurface, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            if (receiveCalls) Row(verticalAlignment = Alignment.CenterVertically) {
                                StatusDot(regColor(reg[a.id]))
                                Spacer(Modifier.width(6.dp))
                                Text(regText(reg[a.id]), fontSize = 13.sp, color = colors.onSurfaceVariant)
                            }
                        }
                        if (a.id == account?.id)
                            Icon(painterResource(R.drawable.ic_check), null, tint = LocalCallColors.current.link)
                    }
                }
                Row(
                    Modifier.fillMaxWidth().clickable { sheet = false; onSettings() }.padding(horizontal = 24.dp, vertical = 16.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(painterResource(R.drawable.ic_settings), null, tint = colors.onSurfaceVariant)
                    Spacer(Modifier.width(14.dp))
                    Text(stringResource(R.string.settings), color = colors.onSurface)
                }
            }
        }
    }
}
