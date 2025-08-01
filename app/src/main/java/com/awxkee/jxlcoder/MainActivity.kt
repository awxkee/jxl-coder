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
                            val bitmap = BitmapFactory.decodeByteArray(buffer5, 0, buffer5.size)
                                .scale(1305, 1295)
//                            val encoder = JxlAnimatedEncoder(
//                                bitmap.width,
//                                bitmap.height,
//                                dataPixelFormat = JxlEncodingDataPixelFormat.BINARY_16
//                            )
//                            repeat(4) {
//                                encoder.addFrame(bitmap, 24)
//                            }
//                            val encoded = encoder.encode()

//                            val animated = JxlCoder.decode(encoded)
//                            lifecycleScope.launch {
//                                imagesArray.add(animated)
//                            }
//
//                            fun simpleRoundTrip(image: String) {
//                                val bufferPng = assets.open(image).source().buffer().readByteArray()
//                                val bitmap = BitmapFactory.decodeByteArray(bufferPng, 0, bufferPng.size)
//                                    .copy(Bitmap.Config.RGBA_1010102,true)
//                                lifecycleScope.launch {
//                                    imagesArray.add(bitmap)
//                                }
//                                val jxlBuffer = JxlCoder.encode(bitmap,
//                                    channelsConfiguration = JxlChannelsConfiguration.RGB,
//                                    compressionOption = JxlCompressionOption.LOSSY,
//                                    effort = JxlEffort.LIGHTNING,
//                                    decodingSpeed = JxlDecodingSpeed.SLOW)
////                                val decodedEncoded = JxlCoder.decode(jxlBuffer,
////                                    preferredColorConfig = PreferredColorConfig.RGBA_1010102)
//                                val decodedEncoded = JxlAnimatedImage(jxlBuffer).getFrame(0, bitmap.width /2 , bitmap.height / 2)
//                                lifecycleScope.launch {
//                                    imagesArray.add(decodedEncoded)
//                                }
//                                val fos = FileOutputStream(File(cacheDir, image))
//                                fos.sink().buffer().use {
//                                    it.writeAll(ByteArrayInputStream(jxlBuffer).source().buffer())
//                                    it.flush()
//                                }
//                            }
//
//                            simpleRoundTrip("screenshot_discord_5.png")
//                            simpleRoundTrip("screen_discord.png")
//                            simpleRoundTrip("screen_discord_2.png")
//
//                            val buffer5 = assets.open("elephant.png").source().buffer().readByteArray()
//                            val jxlBufferPNG = JxlCoder.Convenience.apng2JXL(buffer5, quality = 55)
//                            val buffer = assets.open("abstract_alpha.png").source().buffer().readByteArray()
//                            val bitmap = BitmapFactory.decodeByteArray(buffer, 0, buffer.size)
//                                .copy(Bitmap.Config.ARGB_8888, true)
//                            lifecycleScope.launch {
//                                drawables.add(BitmapDrawable(resources, bitmap))
//                            }
//                            val space = bitmap.copy(Bitmap.Config.ARGB_8888, true)
//                            val encoded =
//                                JxlCoder.encode(
//                                    space,
//                                    channelsConfiguration = JxlChannelsConfiguration.RGB,
//                                    effort = JxlEffort.LIGHTNING,
//                                    compressionOption = JxlCompressionOption.LOSSLESS,
//                                    quality = 100,
//                                )
//                            val decoded = JxlCoder.decodeSampled(
//                                encoded,
//                                preferredColorConfig = PreferredColorConfig.HARDWARE, width = 1305,
//                                height = 1295
//                            )
//                            lifecycleScope.launch {
//                                imagesArray.add(decoded)
//                            }

                            val display: Display = this@MainActivity.windowManager.defaultDisplay
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                                val hdrCapabilities = display.hdrCapabilities
                                val maxNits = hdrCapabilities.desiredMaxLuminance
                                val whitePoint = hdrCapabilities.desiredMaxAverageLuminance
                                Log.d("Max HDR value", "$maxNits whitePoint $whitePoint")
                            }

                            var assets =
                                (this@MainActivity.assets.list("") ?: return@launch).toList()
//                            assets = assets.filter { it.contains("20181110_213419__MMC1561-HDR.jxl") }
//                            assets = assets.take(15)
                            assets = assets.filter { it.contains("Lake_HDR.jxl") }
                            for (asset in assets) {
                                try {
                                    val buffer4 =
                                        this@MainActivity.assets.open(asset).source().buffer()
                                            .readByteArray()

                                    val largeImageSize = JxlCoder.getSize(buffer4)
                                    if (largeImageSize != null) {
                                        val decodingTime = measureTimeMillis {
//                                            val srcImage = JxlCoder.decode(
//                                                buffer4,
//                                                preferredColorConfig = PreferredColorConfig.HARDWARE,
//                                                com.awxkee.jxlcoder.ScaleMode.FIT,
//                                                toneMapper = JxlToneMapper.REC2408,
//                                            )

                                            // Resizable version
                                            val srcImage = JxlCoder.decodeSampled(
                                                buffer4,
                                                width = largeImageSize.width / 3,
                                                height = largeImageSize.height / 3,
                                                preferredColorConfig = PreferredColorConfig.HARDWARE,
                                                com.awxkee.jxlcoder.ScaleMode.FIT,
                                                toneMapper = JxlToneMapper.REC2408,
                                                jxlResizeFilter = JxlResizeFilter.LANCZOS
                                            )
                                            lifecycleScope.launch {
                                                imagesArray.add(srcImage)
                                            }
                                            val srcImage1 = JxlCoder.decodeSampled(
                                                buffer4,
                                                width = largeImageSize.width / 4,
                                                height = largeImageSize.height / 5,
                                                preferredColorConfig = PreferredColorConfig.HARDWARE,
                                                com.awxkee.jxlcoder.ScaleMode.FIT,
                                                toneMapper = JxlToneMapper.REC2408_PERCEPTUAL,
                                                jxlResizeFilter = JxlResizeFilter.NEAREST
                                            )
                                            lifecycleScope.launch {
                                                imagesArray.add(srcImage1)
                                            }
                                            val srcImage2 = JxlCoder.decodeSampled(
                                                buffer4,
                                                width = largeImageSize.width / 2,
                                                height = largeImageSize.height / 2,
                                                preferredColorConfig = PreferredColorConfig.HARDWARE,
                                                com.awxkee.jxlcoder.ScaleMode.FIT,
                                                toneMapper = JxlToneMapper.FILMIC,
                                                jxlResizeFilter = JxlResizeFilter.NEAREST
                                            )
                                            lifecycleScope.launch {
                                                imagesArray.add(srcImage2)
                                            }
                                            val srcImage3 = JxlCoder.decodeSampled(
                                                buffer4,
                                                width = largeImageSize.width / 2,
                                                height = largeImageSize.height / 2,
                                                preferredColorConfig = PreferredColorConfig.HARDWARE,
                                                com.awxkee.jxlcoder.ScaleMode.FIT,
                                                toneMapper = JxlToneMapper.ACES,
                                                jxlResizeFilter = JxlResizeFilter.NEAREST
                                            )
                                            lifecycleScope.launch {
                                                imagesArray.add(srcImage3)
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