package io.github.kochevrin.kksip.ui

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.ripple
import androidx.compose.runtime.remember
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val keys = listOf(
    listOf('1' to "", '2' to "ABC", '3' to "DEF"),
    listOf('4' to "GHI", '5' to "JKL", '6' to "MNO"),
    listOf('7' to "PQRS", '8' to "TUV", '9' to "WXYZ"),
    listOf('*' to "", '0' to "+", '#' to ""),
)

/** Phone keypad; a long press on 0 types +. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun Keypad(onKey: (Char) -> Unit, modifier: Modifier = Modifier) {
    val haptic = LocalHapticFeedback.current
    Column(
        modifier.widthIn(max = 360.dp).fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        for (row in keys) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                for ((digit, letters) in row) {
                    val interaction = remember { MutableInteractionSource() }
                    Box(
                        Modifier
                            .weight(1f)
                            .pressScale(interaction, 0.94f)
                            .height(60.dp)
                            .clip(RoundedCornerShape(16.dp))
                            .background(MaterialTheme.colorScheme.surfaceVariant)
                            .combinedClickable(
                                interactionSource = interaction,
                                indication = ripple(),
                                onClick = {
                                    haptic.performHapticFeedback(HapticFeedbackType.TextHandleMove)
                                    onKey(digit)
                                },
                                onLongClick = if (digit == '0') {
                                    { haptic.performHapticFeedback(HapticFeedbackType.LongPress); onKey('+') }
                                } else null,
                            ),
                        contentAlignment = Alignment.Center,
                    ) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text(digit.toString(), fontSize = 26.sp, color = MaterialTheme.colorScheme.onSurface)
                            if (letters.isNotEmpty())
                                Text(
                                    letters, fontSize = 10.sp, letterSpacing = 1.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                        }
                    }
                }
            }
        }
    }
}
