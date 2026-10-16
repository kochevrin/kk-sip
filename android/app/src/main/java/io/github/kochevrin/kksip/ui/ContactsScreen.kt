package io.github.kochevrin.kksip.ui

import android.content.ClipData
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.data.Contact
import io.github.kochevrin.kksip.data.ContactBook
import kotlinx.coroutines.launch

/** The phone book: search, add, call; CSV in and out like the desktop's. */
@Composable
fun ContactsScreen(
    contacts: List<Contact>,
    onCall: (Contact) -> Unit,
    onSave: (Contact) -> Unit,
    onDelete: (Contact) -> Unit,
    onImportCsv: () -> Unit,
    onExportCsv: () -> Unit,
    onPickFromPhone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val colors = MaterialTheme.colorScheme
    var query by rememberSaveable { mutableStateOf("") }
    var editing by remember { mutableStateOf<Contact?>(null) }
    var menu by remember { mutableStateOf(false) }
    val shown = remember(contacts, query) {
        val q = query.trim()
        val digits = ContactBook.digits(q)
        if (q.isEmpty()) contacts
        else contacts.filter {
            it.name.contains(q, ignoreCase = true) || (digits.isNotEmpty() && ContactBook.digits(it.number).contains(digits))
        }
    }

    Column(modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 4.dp, top = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            OutlinedTextField(
                value = query, onValueChange = { query = it },
                placeholder = { Text(stringResource(R.string.search)) },
                leadingIcon = { Icon(painterResource(R.drawable.ic_search), null, tint = colors.onSurfaceVariant) },
                singleLine = true, shape = RoundedCornerShape(28.dp),
                modifier = Modifier.weight(1f),
            )
            IconButton(onClick = { editing = Contact(0, "", "") }) {
                Icon(painterResource(R.drawable.ic_plus), stringResource(R.string.add_contact), tint = colors.onBackground)
            }
            Box {
                IconButton(onClick = { menu = true }) {
                    Icon(painterResource(R.drawable.ic_more), stringResource(R.string.more), tint = colors.onBackground)
                }
                DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                    DropdownMenuItem(text = { Text(stringResource(R.string.from_phone_contacts)) },
                        onClick = { menu = false; onPickFromPhone() })
                    DropdownMenuItem(text = { Text(stringResource(R.string.import_csv)) },
                        onClick = { menu = false; onImportCsv() })
                    DropdownMenuItem(text = { Text(stringResource(R.string.export_csv)) },
                        enabled = contacts.isNotEmpty(), onClick = { menu = false; onExportCsv() })
                }
            }
        }

        if (shown.isEmpty()) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Icon(painterResource(R.drawable.ic_contacts), null, Modifier.size(48.dp),
                        tint = colors.onSurfaceVariant.copy(alpha = 0.5f))
                    Spacer(Modifier.padding(6.dp))
                    Text(
                        stringResource(if (contacts.isEmpty()) R.string.no_contacts else R.string.nothing_found),
                        color = colors.onSurfaceVariant, textAlign = TextAlign.Center,
                    )
                }
            }
        } else {
            LazyColumn(Modifier.fillMaxSize(), contentPadding = PaddingValues(vertical = 8.dp)) {
                items(shown, key = { it.id }) { c ->
                    ContactRow(c, onCall = { onCall(c) }, onEdit = { editing = c }, onDelete = { onDelete(c) },
                        Modifier.animateItem())
                }
            }
        }
    }

    editing?.let { c ->
        ContactDialog(
            initial = c,
            onSave = { onSave(it); editing = null },
            onDelete = if (c.id > 0) ({ onDelete(c); editing = null }) else null,
            onCancel = { editing = null },
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun ContactRow(c: Contact, onCall: () -> Unit, onEdit: () -> Unit, onDelete: () -> Unit, modifier: Modifier) {
    val colors = MaterialTheme.colorScheme
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var menu by remember { mutableStateOf(false) }
    Box(modifier) {
        Row(
            Modifier.fillMaxWidth().combinedClickable(onClick = onCall, onLongClick = { menu = true })
                .padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Avatar(c.name.ifEmpty { c.number }, 42.dp)
            Column(Modifier.weight(1f)) {
                Text(c.name.ifEmpty { c.number }, color = colors.onBackground, fontSize = 16.sp,
                    maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (c.name.isNotEmpty()) Text(c.number, color = colors.onSurfaceVariant, fontSize = 13.sp, maxLines = 1)
            }
            IconButton(onClick = onCall) {
                Icon(painterResource(R.drawable.ic_call), stringResource(R.string.call), tint = LocalCallColors.current.ok)
            }
        }
        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
            DropdownMenuItem(text = { Text(stringResource(R.string.edit)) }, onClick = { menu = false; onEdit() })
            DropdownMenuItem(text = { Text(stringResource(R.string.copy_number)) }, onClick = {
                scope.launch { clipboard.setClipEntry(ClipEntry(ClipData.newPlainText(c.number, c.number))) }
                menu = false
            })
            DropdownMenuItem(text = { Text(stringResource(R.string.delete)) }, onClick = { menu = false; onDelete() })
        }
    }
}

@Composable
fun ContactDialog(initial: Contact, onSave: (Contact) -> Unit, onDelete: (() -> Unit)?, onCancel: () -> Unit) {
    var name by rememberSaveable { mutableStateOf(initial.name) }
    var number by rememberSaveable { mutableStateOf(initial.number) }
    AlertDialog(
        onDismissRequest = onCancel,
        containerColor = MaterialTheme.colorScheme.surface,
        title = { Text(stringResource(if (initial.id > 0) R.string.edit_contact else R.string.add_contact)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(name, { name = it }, label = { Text(stringResource(R.string.name)) }, singleLine = true)
                OutlinedTextField(
                    number, { number = it }, label = { Text(stringResource(R.string.number)) }, singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Phone),
                )
                if (onDelete != null) KkTextButton(stringResource(R.string.delete), onClick = onDelete)
            }
        },
        confirmButton = {
            KkTextButton(stringResource(R.string.save)) {
                if (number.isNotBlank()) onSave(initial.copy(name = name.trim(), number = number.trim()))
            }
        },
        dismissButton = { KkTextButton(stringResource(R.string.cancel), onClick = onCancel) },
    )
}
