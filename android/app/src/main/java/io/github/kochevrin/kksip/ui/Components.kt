package io.github.kochevrin.kksip.ui

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Presses shrink a little and spring back: the one motion every button shares. */
@Composable
fun Modifier.pressScale(interaction: MutableInteractionSource, pressed: Float = 0.92f): Modifier {
    val isPressed by interaction.collectIsPressedAsState()
    val scale by animateFloatAsState(
        if (isPressed) pressed else 1f,
        spring(dampingRatio = 0.5f, stiffness = 600f),
        label = "press",
    )
    return graphicsLayer { scaleX = scale; scaleY = scale }
}

/** Initials on the logo's navy tile, orange like the logo's handset. */
@Composable
fun Avatar(title: String, size: Dp, modifier: Modifier = Modifier) {
    val initials = remember(title) { initialsOf(title) }
    Box(
        modifier
            .size(size)
            .clip(CircleShape)
            .background(Brush.verticalGradient(listOf(Color(0xFF1E3A5F), Navy))),
        contentAlignment = Alignment.Center,
    ) {
        if (initials.isEmpty()) {
            Icon(painterResource(io.github.kochevrin.kksip.R.drawable.ic_call), null,
                Modifier.size(size * 0.42f), tint = Orange)
        } else {
            Text(initials, color = Orange, fontSize = (size.value * 0.36f).sp, fontWeight = FontWeight.SemiBold)
        }
    }
}

private fun initialsOf(title: String): String {
    val words = title.trim().split(Regex("""\s+""")).filter { w -> w.firstOrNull()?.isLetter() == true }
    return when {
        words.isEmpty() -> ""
        words.size == 1 -> words[0].take(1).uppercase()
        else -> (words[0].take(1) + words[1].take(1)).uppercase()
    }
}

/** Soft rings spreading from behind the avatar while a call rings. */
@Composable
fun Modifier.ringPulse(active: Boolean, color: Color): Modifier {
    if (!active) return this
    val t = rememberInfiniteTransition(label = "pulse")
    val p by t.animateFloat(0f, 1f, infiniteRepeatable(tween(1800, easing = LinearEasing)), label = "p")
    return drawBehind {
        for (k in 0..1) {
            val f = (p + k * 0.5f) % 1f
            drawCircle(
                color.copy(alpha = 0.28f * (1f - f)),
                radius = size.minDimension / 2 * (1f + 0.55f * f),
            )
        }
    }
}

/** The big round call buttons: call, answer, decline, hang up. */
@Composable
fun RoundButton(
    icon: Int,
    label: String,
    fill: Color,
    enabled: Boolean = true,
    size: Dp = 72.dp,
    modifier: Modifier = Modifier,
    onClick: () -> Unit,
) {
    val interaction = remember { MutableInteractionSource() }
    val bg by animateColorAsState(if (enabled) fill else fill.copy(alpha = 0.35f), label = "fill")
    Box(
        modifier
            .pressScale(interaction, 0.9f)
            .size(size)
            .clip(CircleShape)
            .background(bg)
            .clickable(interaction, indication = null, enabled = enabled, role = Role.Button, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(painterResource(icon), label, Modifier.size(size * 0.44f), tint = Color.White)
    }
}

/** Round toggle with a caption under it (mute, speaker, hold, keypad). */
@Composable
fun ToggleButton(icon: Int, label: String, checked: Boolean, enabled: Boolean = true, onClick: () -> Unit) {
    val colors = MaterialTheme.colorScheme
    val interaction = remember { MutableInteractionSource() }
    val bg by animateColorAsState(if (checked) colors.primary else colors.surfaceVariant, label = "bg")
    val fg by animateColorAsState(
        when {
            !enabled -> colors.onSurfaceVariant.copy(alpha = 0.38f)
            checked -> colors.onPrimary
            else -> colors.onSurface
        },
        label = "fg",
    )
    Column(Modifier.width(84.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            Modifier
                .pressScale(interaction)
                .size(64.dp)
                .clip(CircleShape)
                .background(bg)
                .clickable(interaction, indication = null, enabled = enabled, role = Role.Switch, onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Icon(painterResource(icon), label, Modifier.size(28.dp), tint = fg)
        }
        Spacer(Modifier.height(6.dp))
        Text(
            label, fontSize = 13.sp, maxLines = 1,
            color = if (enabled) colors.onSurfaceVariant else colors.onSurfaceVariant.copy(alpha = 0.38f),
        )
    }
}

/** Registration state as a small dot: green online, amber connecting, red failed. */
@Composable
fun StatusDot(color: Color, modifier: Modifier = Modifier) {
    val c by animateColorAsState(color, label = "dot")
    Box(modifier.size(8.dp).clip(CircleShape).background(c))
}

/** Text button in the kk orange that stays readable on white. */
@Composable
fun KkTextButton(text: String, modifier: Modifier = Modifier, onClick: () -> Unit) {
    TextButton(
        onClick, modifier,
        colors = ButtonDefaults.textButtonColors(contentColor = LocalCallColors.current.link),
    ) { Text(text) }
}
