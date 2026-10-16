package io.github.kochevrin.kksip.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.kochevrin.kksip.R
import io.github.kochevrin.kksip.data.Contact

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun DialerScreen(
    number: String,
    onNumber: (String) -> Unit,
    canCall: Boolean,
    onCall: () -> Unit,
    hint: Contact? = null,
    modifier: Modifier = Modifier,
) {
    val colors = MaterialTheme.colorScheme
    val size by animateFloatAsState(
        when {
            number.length <= 11 -> 38f
            number.length <= 15 -> 30f
            else -> 24f
        },
        label = "digits",
    )
    Column(
        modifier.fillMaxSize().padding(horizontal = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Spacer(Modifier.weight(1f))
        Row(Modifier.fillMaxWidth().height(64.dp), verticalAlignment = Alignment.CenterVertically) {
            Spacer(Modifier.size(48.dp))
            Text(
                number,
                Modifier.weight(1f),
                fontSize = size.sp,
                color = colors.onBackground,
                textAlign = TextAlign.Center,
                maxLines = 1,
                overflow = TextOverflow.StartEllipsis,
                letterSpacing = 1.sp,
            )
            Backspace(number.isNotEmpty(), onClick = { onNumber(number.dropLast(1)) }, onLongClick = { onNumber("") })
        }
        // The contact these digits belong to; a tap takes its full number.
        Box(Modifier.height(28.dp), contentAlignment = Alignment.Center) {
            androidx.compose.animation.AnimatedVisibility(hint != null, enter = fadeIn(), exit = fadeOut()) {
                val h = hint ?: return@AnimatedVisibility
                Text(
                    if (h.name.isNotEmpty()) "${h.name} · ${h.number}" else h.number,
                    color = LocalCallColors.current.link, fontSize = 14.sp, maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.clip(CircleShape).combinedClickable(onClick = { onNumber(h.number) })
                        .padding(horizontal = 12.dp, vertical = 4.dp),
                )
            }
        }
        Spacer(Modifier.height(4.dp))
        Keypad(onKey = { onNumber(number + it) })
        Spacer(Modifier.height(20.dp))
        RoundButton(
            icon = R.drawable.ic_call,
            label = stringResource(R.string.call),
            fill = LocalCallColors.current.okFill,
            enabled = canCall && number.isNotEmpty(),
            onClick = onCall,
        )
        Spacer(Modifier.height(16.dp))
    }
}

/** Shows up with the first digit; a long press clears everything. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun Backspace(visible: Boolean, onClick: () -> Unit, onLongClick: () -> Unit) {
    Box(Modifier.size(48.dp), contentAlignment = Alignment.Center) {
        AnimatedVisibility(
            visible,
            enter = fadeIn() + scaleIn(initialScale = 0.6f),
            exit = fadeOut() + scaleOut(targetScale = 0.6f),
        ) {
            Box(
                Modifier
                    .size(48.dp)
                    .clip(CircleShape)
                    .combinedClickable(onClick = onClick, onLongClick = onLongClick),
                contentAlignment = Alignment.Center,
            ) {
                Icon(painterResource(R.drawable.ic_backspace), stringResource(R.string.delete),
                    tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
