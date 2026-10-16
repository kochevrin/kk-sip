package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.sip.Account

/** "Import these accounts?" with the list, so nothing lands on the phone unseen. */
@Composable
fun ImportDialog(accounts: List<Account>, onImport: () -> Unit, onCancel: () -> Unit) {
    val colors = MaterialTheme.colorScheme
    AlertDialog(
        onDismissRequest = onCancel,
        containerColor = colors.surface,
        title = { Text(pluralStringResource(R.plurals.import_title, accounts.size, accounts.size)) },
        text = {
            Column {
                LazyColumn(Modifier.heightIn(max = 320.dp)) {
                    items(accounts) { a ->
                        Row(Modifier.padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                            StatusDot(if (a.enabled) LocalCallColors.current.ok else colors.outline)
                            Spacer(Modifier.width(10.dp))
                            Column {
                                Text(a.title, maxLines = 1, overflow = TextOverflow.Ellipsis, color = colors.onSurface)
                                Text(
                                    a.server + if (a.enabled) "" else " · " + stringResource(R.string.account_off),
                                    fontSize = 12.sp, color = colors.onSurfaceVariant, maxLines = 1,
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = { KkTextButton(stringResource(R.string.do_import), onClick = onImport) },
        dismissButton = { KkTextButton(stringResource(R.string.cancel), onClick = onCancel) },
    )
}
