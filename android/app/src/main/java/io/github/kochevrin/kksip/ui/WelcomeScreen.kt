package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.background
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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R

/** First start: bring the accounts over from the computer, or type one in. */
@Composable
fun WelcomeScreen(onImport: () -> Unit, onManual: () -> Unit) {
    val colors = MaterialTheme.colorScheme
    Column(
        Modifier.fillMaxSize().background(colors.background).safeDrawingPadding().padding(24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Spacer(Modifier.weight(1f))
        // The logo tile: navy gradient, orange handset.
        androidx.compose.foundation.layout.Box(
            Modifier.size(96.dp).clip(RoundedCornerShape(24.dp))
                .background(Brush.verticalGradient(listOf(Color(0xFF1E3A5F), Navy))),
            contentAlignment = Alignment.Center,
        ) {
            Icon(painterResource(R.drawable.ic_call), null, Modifier.size(48.dp), tint = Orange)
        }
        Spacer(Modifier.height(24.dp))
        Text("kk-sip", fontSize = 30.sp, fontWeight = FontWeight.SemiBold, color = colors.onBackground)
        Spacer(Modifier.height(8.dp))
        Text(
            stringResource(R.string.welcome_text), color = colors.onSurfaceVariant,
            textAlign = TextAlign.Center, fontSize = 15.sp,
        )
        Spacer(Modifier.weight(1f))
        Button(
            onClick = onImport, modifier = Modifier.fillMaxWidth().height(52.dp),
            colors = ButtonDefaults.buttonColors(containerColor = colors.primary, contentColor = colors.onPrimary),
        ) {
            Icon(painterResource(R.drawable.ic_import), null, Modifier.size(20.dp))
            Spacer(Modifier.width(8.dp))
            Text(stringResource(R.string.import_from_file))
        }
        Spacer(Modifier.height(12.dp))
        OutlinedButton(onClick = onManual, modifier = Modifier.fillMaxWidth().height(52.dp)) {
            Text(stringResource(R.string.add_account), color = colors.onBackground)
        }
        Spacer(Modifier.height(16.dp))
        Row { Text(stringResource(R.string.welcome_qr), color = colors.onSurfaceVariant, fontSize = 13.sp, textAlign = TextAlign.Center) }
        Spacer(Modifier.height(8.dp))
    }
}
