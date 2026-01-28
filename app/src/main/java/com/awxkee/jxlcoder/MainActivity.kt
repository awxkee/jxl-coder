package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.graphics.drawable.Drawable
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.Display
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.tooling.preview.Preview
import androidx.core.graphics.scale
import androidx.lifecycle.lifecycleScope
import com.awxkee.jxlcoder.ui.theme.JXLCoderTheme
import com.bumptech.glide.integration.compose.ExperimentalGlideComposeApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import okio.buffer
import okio.source
import java.io.FileNotFoundException
import java.util.UUID
import kotlin.system.measureTimeMillis


class MainActivity : ComponentActivity() {
    @OptIn(ExperimentalGlideComposeApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
//            val buffer4 = this.assets.open("happy_india.jxl").source().buffer().readByteArray()
//            assert(JxlCoder.isJXL(buffer4))
//            val largeImageSize = JxlCoder().getSize(buffer4)
//            assert(largeImageSize != null)
//
//            val image10Bit = image //.copy(Bitmap.Config.RGBA_F16, true)
////            image10Bit.setColorSpace(ColorSpace.get(ColorSpace.Named.DCI_P3))

//            val decompressedImage =
//                JxlCoder().decodeSampled(
//                    buffer4, largeImageSize!!.width * 2, largeImageSize.height * 2,
//                    preferredColorConfig = PreferredColorConfig.RGBA_F16
//                )
//
////            val first = JxlCoder().decode(
////                this.assets.open("happy_india.jxl").source().buffer()
////                    .readByteArray(),
////                preferredColorConfig = PreferredColorConfig.RGB_565
////            )
////
////            val resized = getResizedBitmap(first, decompressedImage.width, decompressedImage.height)
////            first.recycle()
//
//            val compressedBuffer = JxlCoder().encode(
//                decompressedImage,
//                colorSpace = JxlColorSpace.RGB,
//                compressionOption = JxlCompressionOption.LOSSY,
//                effort = 4,
//                quality = 90,
//            )
//
//            val image =
//                JxlCoder().decode(
//                    compressedBuffer,
//                    preferredColorConfig = PreferredColorConfig.RGB_565
//                )

//            val encoder = JxlAnimatedEncoder(
//                width = decompressedImage.width,
//                height = decompressedImage.height,
//            )
//            encoder.addFrame(decompressedImage, 2000)
//            encoder.addFrame(resized, 2000)
//            val compressedBuffer: ByteArray = encoder.encode()

//            val image = JxlCoder().decode(compressedBuffer, preferredColorConfig = PreferredColorConfig.RGBA_8888)

//            val animatedImage = JxlAnimatedImage(compressedBuffer)
//            val drawable = animatedImage.animatedDrawable

            setContent {
                JXLCoderTheme {
                    var imagesArray = remember {
                        mutableStateListOf<Bitmap>()
                    }
                    var drawablesArray = remember {
                        mutableStateListOf<Drawable>()
                    }
                    LaunchedEffect(key1 = Unit, block = {
                        lifecycleScope.launch(Dispatchers.IO) {
                            val buffer5 =
                                assets.open("test_image_1.jpg").source().buffer().readByteArray()

                            var assets =
                                (this@MainActivity.assets.list("") ?: return@launch).toList()
//                            assets = assets.filter { it.contains("20181110_213419__MMC1561-HDR.jxl") }
//                            assets = assets.take(15)
//                            assets = assets.filter { it.contains("test_img_q80.jxl") }
                            for (asset in assets) {
                                try {
                                    val buffer4 =
                                        this@MainActivity.assets.open(asset).source().buffer()
                                            .readByteArray()

                                    val largeImageSize = JxlCoder.getSize(buffer4)
                                    if (largeImageSize != null) {
                                        val decodingTime = measureTimeMillis {
                                            // Resizable version
                                            val srcImage = JxlCoder.decodeSampled(
                                                buffer4,
                                                width = largeImageSize.width / 3,
                                                height = largeImageSize.height / 3,
                                                preferredColorConfig = PreferredColorConfig.RGBA_8888,
                                                com.awxkee.jxlcoder.ScaleMode.FIT,
                                                jxlResizeFilter = JxlResizeFilter.LANCZOS
                                            )
                                            lifecycleScope.launch {
                                                imagesArray.add(srcImage)
                                            }
                                        }
                                        Log.d("JXLMain", "Decoding time ${decodingTime}ms")
                                        launch {
                                            delay(1500)
                                            Log.d("JXLMain", "Decoding time ${decodingTime}ms")
                                        }
                                    }
                                } catch (e: Exception) {
                                    if (e !is FileNotFoundException) {
                                        throw e
                                    }
                                }
                            }
                        }
                    })
                    // A surface container using the 'background' color from the theme
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = MaterialTheme.colorScheme.background
                    ) {
                        LazyColumn(
                            modifier = Modifier
                                .fillMaxSize()
                        ) {
                            items(imagesArray.count(), key = {
                                return@items UUID.randomUUID().toString()
                            }) {
                                Image(
                                    bitmap = imagesArray[it].asImageBitmap(),
                                    modifier = Modifier.fillMaxWidth(),
                                    contentScale = ContentScale.FillWidth,
                                    contentDescription = "ok"
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

fun getResizedBitmap(bm: Bitmap, newWidth: Int, newHeight: Int): Bitmap {
    val width = bm.width
    val height = bm.height
    val scaleWidth = newWidth.toFloat() / width
    val scaleHeight = newHeight.toFloat() / height
    val matrix = Matrix()
    matrix.postScale(scaleWidth, scaleHeight)
    return Bitmap.createBitmap(
        bm, 0, 0, width, height, matrix, false
    )
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = "Hello $name!",
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    JXLCoderTheme {
        Greeting("Android")
    }
}