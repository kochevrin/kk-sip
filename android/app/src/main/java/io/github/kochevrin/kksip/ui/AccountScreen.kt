package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.sip.Account

@Composable
fun AccountScreen(
    initial: Account,
    canCancel: Boolean,
    onSave: (Account) -> Unit,
    onCancel: () -> Unit,
    onDelete: (() -> Unit)?,
) {
    val colors = MaterialTheme.colorScheme
    var server by rememberSaveable { mutableStateOf(initial.server) }
    var user by rememberSaveable { mutableStateOf(initial.user) }
    var password by rememberSaveable { mutableStateOf(initial.password) }
    var displayName by rememberSaveable { mutableStateOf(initial.displayName) }
    var label by rememberSaveable { mutableStateOf(initial.label) }
    var domain by rememberSaveable { mutableStateOf(initial.domain) }
    var proxy by rememberSaveable { mutableStateOf(initial.proxy) }
    var authUser by rememberSaveable { mutableStateOf(initial.authUser) }
    var transport by rememberSaveable { mutableStateOf(initial.transport) }
    var srtp by rememberSaveable { mutableStateOf(initial.srtp) }
    var natRewrite by rememberSaveable { mutableStateOf(initial.natRewrite) }
    var enabled by rememberSaveable { mutableStateOf(initial.enabled) }
    var advanced by rememberSaveable { mutableStateOf(false) }

    Column(
        Modifier
            .fillMaxSize()
            .background(colors.background)
            .safeDrawingPadding()
            .imePadding()
            .padding(horizontal = 16.dp),
    ) {
        Row(Modifier.fillMaxWidth().height(56.dp), verticalAlignment = Alignment.CenterVertically) {
            if (canCancel) {
                IconButton(onClick = onCancel) {
                    Icon(painterResource(R.drawable.ic_close), stringResource(R.string.cancel), tint = colors.onBackground)
                }
            }
            Text(
                stringResource(if (onDelete == null) R.string.new_account else R.string.account),
                Modifier.weight(1f).padding(start = if (canCancel) 4.dp else 0.dp),
                style = MaterialTheme.typography.titleLarge,
                color = colors.onBackground,
            )
            Button(
                onClick = {
                    onSave(
                        initial.copy(
                            server = server.trim(), user = user.trim(), password = password,
                            displayName = displayName.trim(), label = label.trim(),
                            domain = domain.trim(), proxy = proxy.trim(), authUser = authUser.trim(),
                            transport = transport, srtp = srtp, natRewrite = natRewrite, enabled = enabled,
                        )
                    )
                },
                enabled = server.isNotBlank() && user.isNotBlank(),
            ) { Text(stringResource(R.string.save)) }
        }

        Column(
            Modifier.verticalScroll(rememberScrollState()).padding(bottom = 16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            SwitchRow(stringResource(R.string.account_enabled), enabled) { enabled = it }
            Field(stringResource(R.string.server), server, { server = it },
                hint = stringResource(R.string.server_hint), keyboard = KeyboardType.Uri)
            Field(stringResource(R.string.user), user, { user = it }, keyboard = KeyboardType.Ascii)
            Field(stringResource(R.string.password), password, { password = it }, secret = true)
            Field(stringResource(R.string.display_name), displayName, { displayName = it })

            KkTextButton(stringResource(R.string.advanced) + if (advanced) " ▴" else " ▾") { advanced = !advanced }
            if (advanced) {
                Field(stringResource(R.string.label), label, { label = it })
                Field(stringResource(R.string.domain), domain, { domain = it }, keyboard = KeyboardType.Uri)
                Field(stringResource(R.string.proxy), proxy, { proxy = it }, keyboard = KeyboardType.Uri)
                Field(stringResource(R.string.auth_user), authUser, { authUser = it }, keyboard = KeyboardType.Ascii)
                Text(stringResource(R.string.transport), color = colors.onSurfaceVariant)
                val options = listOf("udp", "tcp")
                SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                    options.forEachIndexed { i, t ->
                        SegmentedButton(
                            selected = transport == t,
                            onClick = { transport = t },
                            shape = SegmentedButtonDefaults.itemShape(i, options.size),
                        ) { Text(t.uppercase()) }
                    }
                }
                SwitchRow(stringResource(R.string.srtp), srtp) { srtp = it }
                SwitchRow(stringResource(R.string.nat_rewrite), natRewrite) { natRewrite = it }
            }
            if (onDelete != null) {
                Spacer(Modifier.height(8.dp))
                OutlinedButton(
                    onClick = onDelete,
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = LocalCallColors.current.danger),
                ) { Text(stringResource(R.string.delete_account)) }
            }
            Spacer(Modifier.height(8.dp))
        }
    }
}

@Composable
private fun Field(
    label: String,
    value: String,
    onChange: (String) -> Unit,
    hint: String? = null,
    secret: Boolean = false,
    keyboard: KeyboardType = KeyboardType.Text,
) {
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        label = { Text(label) },
        placeholder = hint?.let { { Text(it) } },
        singleLine = true,
        visualTransformation = if (secret) PasswordVisualTransformation()
        else androidx.compose.ui.text.input.VisualTransformation.None,
        keyboardOptions = KeyboardOptions(
            keyboardType = if (secret) KeyboardType.Password else keyboard,
            autoCorrectEnabled = false,
        ),
        colors = OutlinedTextFieldDefaults.colors(),
        modifier = Modifier.fillMaxWidth(),
    )
}

@Composable
private fun SwitchRow(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), color = MaterialTheme.colorScheme.onBackground)
        Switch(
            checked = checked, onCheckedChange = onChange,
            colors = androidx.compose.material3.SwitchDefaults.colors(
                checkedTrackColor = MaterialTheme.colorScheme.primary,
                checkedThumbColor = MaterialTheme.colorScheme.onPrimary,
            ),
        )
    }
}
