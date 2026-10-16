package io.github.kochevrin.kksip.ui

import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.NotFoundException
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.common.HybridBinarizer
import io.github.kochevrin.kksip.R
import java.util.concurrent.Executors

/** Camera viewfinder that reads the QR code kk-sip on the computer shows. */
@Composable
fun QrScanScreen(onResult: (String) -> Unit, onClose: () -> Unit) {
    val context = LocalContext.current
    val owner = LocalLifecycleOwner.current
    val result by rememberUpdatedState(onResult)
    val previewView = remember { PreviewView(context).apply { scaleType = PreviewView.ScaleType.FILL_CENTER } }

    DisposableEffect(owner) {
        val executor = Executors.newSingleThreadExecutor()
        val providerFuture = ProcessCameraProvider.getInstance(context)
        var done = false
        val reader = MultiFormatReader().apply {
            setHints(mapOf(DecodeHintType.POSSIBLE_FORMATS to listOf(BarcodeFormat.QR_CODE), DecodeHintType.TRY_HARDER to true))
        }
        providerFuture.addListener({
            val provider = providerFuture.get()
            val preview = Preview.Builder().build().also { it.surfaceProvider = previewView.surfaceProvider }
            val analysis = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()
            analysis.setAnalyzer(executor) { image ->
                val text = if (done) null else decode(reader, image)
                image.close()
                if (text != null && !done) {
                    done = true
                    ContextCompat.getMainExecutor(context).execute { result(text) }
                }
            }
            runCatching {
                provider.unbindAll()
                // The back camera, or the front one on devices that only have that.
                val selector = if (provider.hasCamera(CameraSelector.DEFAULT_BACK_CAMERA))
                    CameraSelector.DEFAULT_BACK_CAMERA else CameraSelector.DEFAULT_FRONT_CAMERA
                provider.bindToLifecycle(owner, selector, preview, analysis)
            }
        }, ContextCompat.getMainExecutor(context))
        onDispose {
            runCatching { providerFuture.get().unbindAll() }
            executor.shutdown()
        }
    }

    Box(Modifier.fillMaxSize().background(Color.Black)) {
        AndroidView({ previewView }, Modifier.fillMaxSize())
        // A square with orange corners: where the code goes.
        Canvas(Modifier.fillMaxSize()) {
            val side = size.minDimension * 0.66f
            val left = (size.width - side) / 2
            val top = (size.height - side) / 2
            val len = side * 0.16f
            val stroke = Stroke(width = 6.dp.toPx(), cap = StrokeCap.Round)
            drawRect(Color.Black.copy(alpha = 0.35f), Offset.Zero, Size(size.width, top))
            drawRect(Color.Black.copy(alpha = 0.35f), Offset(0f, top + side), Size(size.width, size.height - top - side))
            for ((x, y, dx, dy) in listOf(
                listOf(left, top, 1f, 1f), listOf(left + side, top, -1f, 1f),
                listOf(left, top + side, 1f, -1f), listOf(left + side, top + side, -1f, -1f),
            )) {
                drawLine(Orange, Offset(x, y), Offset(x + dx * len, y), stroke.width, StrokeCap.Round)
                drawLine(Orange, Offset(x, y), Offset(x, y + dy * len), stroke.width, StrokeCap.Round)
            }
        }
        Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp)) {
            IconButton(
                onClick = onClose,
                colors = IconButtonDefaults.iconButtonColors(containerColor = Color.Black.copy(alpha = 0.4f)),
                modifier = Modifier.size(48.dp),
            ) {
                Icon(painterResource(R.drawable.ic_close), stringResource(R.string.cancel), tint = Color.White)
            }
        }
        Text(
            stringResource(R.string.scan_hint),
            color = Color.White, fontSize = 15.sp, textAlign = TextAlign.Center,
            modifier = Modifier.align(Alignment.BottomCenter).safeDrawingPadding().padding(32.dp)
                .background(Color.Black.copy(alpha = 0.45f), CircleShape).padding(horizontal = 16.dp, vertical = 8.dp),
        )
    }
}

/** QR text from the luminance (Y) plane, or null. */
private fun decode(reader: MultiFormatReader, image: ImageProxy): String? {
    val plane = image.planes[0]
    val buf = plane.buffer
    val bytes = ByteArray(buf.remaining()).also { buf.get(it) }
    val source = PlanarYUVLuminanceSource(
        bytes, plane.rowStride, image.height, 0, 0, image.width, image.height, false,
    )
    return try {
        reader.decodeWithState(BinaryBitmap(HybridBinarizer(source))).text
    } catch (_: NotFoundException) {
        null
    } catch (_: Exception) {
        null
    } finally {
        reader.reset()
    }
}
