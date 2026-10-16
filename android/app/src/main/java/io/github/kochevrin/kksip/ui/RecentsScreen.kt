package io.github.kochevrin.kksip.ui

import android.text.format.DateFormat
import android.text.format.DateUtils
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.launch
import android.content.ClipData
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.data.CallRecord

/** Recent calls, newest first, grouped by day; a tap calls back. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun RecentsScreen(
    records: List<CallRecord>,
    onCall: (CallRecord) -> Unit,
    onDelete: (CallRecord) -> Unit,
    modifier: Modifier = Modifier,
) {
    val colors = MaterialTheme.colorScheme
    if (records.isEmpty()) {
        Box(modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Icon(painterResource(R.drawable.ic_history), null, Modifier.size(48.dp),
                    tint = colors.onSurfaceVariant.copy(alpha = 0.5f))
                Spacer(Modifier.padding(6.dp))
                Text(stringResource(R.string.no_recents), color = colors.onSurfaceVariant, textAlign = TextAlign.Center)
            }
        }
        return
    }
    val context = LocalContext.current
    val groups = remember(records) {
        records.groupBy { r ->
            when {
                DateUtils.isToday(r.startedAt) -> context.getString(R.string.today)
                DateUtils.isToday(r.startedAt + DateUtils.DAY_IN_MILLIS) -> context.getString(R.string.yesterday)
                else -> DateUtils.formatDateTime(context, r.startedAt, DateUtils.FORMAT_SHOW_DATE or DateUtils.FORMAT_ABBREV_MONTH)
            }
        }
    }
    LazyColumn(modifier.fillMaxSize(), contentPadding = androidx.compose.foundation.layout.PaddingValues(bottom = 8.dp)) {
        groups.forEach { (day, list) ->
            item(key = "h$day") {
                Text(
                    day, Modifier.padding(start = 20.dp, top = 16.dp, bottom = 4.dp),
                    style = MaterialTheme.typography.labelLarge, color = colors.onSurfaceVariant,
                )
            }
            items(list, key = { it.id }) { r ->
                RecentRow(r, onCall = { onCall(r) }, onDelete = { onDelete(r) }, Modifier.animateItem())
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentRow(r: CallRecord, onCall: () -> Unit, onDelete: () -> Unit, modifier: Modifier) {
    val colors = MaterialTheme.colorScheme
    val call = LocalCallColors.current
    val context = LocalContext.current
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var menu by remember { mutableStateOf(false) }
    val (icon, tint) = when {
        r.missed -> R.drawable.ic_call_missed to call.danger
        r.incoming -> R.drawable.ic_call_in to call.ok
        else -> R.drawable.ic_call_out to call.outgoing
    }
    Box(modifier) {
        Row(
            Modifier
                .fillMaxWidth()
                .combinedClickable(onClick = onCall, onLongClick = { menu = true })
                .padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Avatar(r.name.ifEmpty { r.number }, 42.dp)
            Column(Modifier.weight(1f)) {
                Text(
                    r.name.ifEmpty { r.number }, maxLines = 1, overflow = TextOverflow.Ellipsis,
                    color = if (r.missed) call.danger else colors.onBackground, fontSize = 16.sp,
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(painterResource(icon), null, Modifier.size(14.dp), tint = tint)
                    Spacer(Modifier.width(4.dp))
                    val time = DateFormat.getTimeFormat(context).format(r.startedAt)
                    val detail = buildString {
                        if (r.name.isNotEmpty()) append(r.number).append(" · ")
                        append(time)
                        if (r.durationSec > 0) append(" · ").append(DateUtils.formatElapsedTime(r.durationSec.toLong()))
                    }
                    Text(detail, fontSize = 13.sp, color = colors.onSurfaceVariant, maxLines = 1,
                        overflow = TextOverflow.Ellipsis)
                }
            }
            IconButton(onClick = onCall) {
                Icon(painterResource(R.drawable.ic_call), stringResource(R.string.call), tint = call.ok)
            }
        }
        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
            DropdownMenuItem(text = { Text(stringResource(R.string.copy_number)) }, onClick = {
                scope.launch { clipboard.setClipEntry(ClipEntry(ClipData.newPlainText(r.number, r.number))) }
                menu = false
            })
            DropdownMenuItem(text = { Text(stringResource(R.string.delete)) }, onClick = {
                onDelete(); menu = false
            })
        }
    }
}
