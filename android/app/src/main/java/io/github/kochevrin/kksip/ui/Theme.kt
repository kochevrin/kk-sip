package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color

// The desktop palette (src/ui/Theme.cpp): the logo's navy and one orange accent.
val Orange = Color(0xFFF57C00)
val Navy = Color(0xFF12243E)

/** kk colours Material has no slot for; contrast as on the desktop (WCAG AA). */
@Immutable
data class CallColors(
    val okFill: Color,
    val dangerFill: Color,
    val ok: Color,
    val danger: Color,
    val link: Color, // orange for text: the logo orange is too light on white
    val warn: Color,
    val outgoing: Color,
)

private val LightCall = CallColors(
    okFill = Color(0xFF147B47), dangerFill = Color(0xFFC32222),
    ok = Color(0xFF15844D), danger = Color(0xFFB81E1E),
    link = Color(0xFFB34A05), warn = Color(0xFFD17205), outgoing = Color(0xFF2563EB),
)
private val DarkCall = CallColors(
    okFill = Color(0xFF167E4A), dangerFill = Color(0xFFC92626),
    ok = Color(0xFF36C97B), danger = Color(0xFFF7726E),
    link = Color(0xFFFF9933), warn = Color(0xFFF6AE31), outgoing = Color(0xFF6EA8FE),
)
val LocalCallColors = staticCompositionLocalOf { DarkCall }

private val Light = lightColorScheme(
    primary = Color(0xFFE66700),
    onPrimary = Navy,
    secondary = Navy,
    onSecondary = Color.White,
    background = Color(0xFFF2F4F8),
    onBackground = Color(0xFF12233A),
    surface = Color.White,
    onSurface = Color(0xFF12233A),
    surfaceVariant = Color(0xFFE8ECF3),
    onSurfaceVariant = Color(0xFF515E70),
    surfaceContainerHighest = Color(0xFFD7DFEA),
    outline = Color(0xFF9FABBC),
    outlineVariant = Color(0xFFD0D8E2),
    error = Color(0xFFB81E1E),
)

private val Dark = darkColorScheme(
    primary = Orange,
    onPrimary = Navy,
    secondary = Color(0xFF22354F),
    onSecondary = Color(0xFFEBEFF4),
    background = Color(0xFF0C131D),
    onBackground = Color(0xFFEBEFF4),
    surface = Color(0xFF111A27),
    onSurface = Color(0xFFEBEFF4),
    surfaceVariant = Color(0xFF1A2432),
    onSurfaceVariant = Color(0xFF9AA6B6),
    surfaceContainerHighest = Color(0xFF243042),
    outline = Color(0xFF404F64),
    outlineVariant = Color(0xFF253141),
    error = Color(0xFFF7726E),
)

@Composable
fun KkTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    androidx.compose.runtime.CompositionLocalProvider(LocalCallColors provides if (dark) DarkCall else LightCall) {
        MaterialTheme(colorScheme = if (dark) Dark else Light, content = content)
    }
}
