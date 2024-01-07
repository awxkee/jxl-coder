package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.graphics.Matrix
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.lifecycleScope
import com.awxkee.jxlcoder.ui.theme.JXLCoderTheme
import com.bumptech.glide.integration.compose.ExperimentalGlideComposeApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import okio.buffer
import okio.source
import java.util.UUID

class MainActivity : ComponentActivity() {
    @OptIn(ExperimentalGlideComposeApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

//        val buffer1 = this.assets.open("hdr_cosmos.jxl").source().buffer().readByteArray()
//        assert(JxlCoder.isJXL(buffer1))
//        assert(JxlCoder().getSize(buffer1) != null)
//        val iccCosmosImage = JxlCoder().decode(buffer1)
//        val buffer2 = this.assets.open("second_jxl.jxl").source().buffer().readByteArray()
//        assert(JxlCoder.isJXL(buffer2))
//        assert(JxlCoder().getSize(buffer2) != null)
//        val buffer3 = this.assets.open("alpha_jxl.jxl").source().buffer().readByteArray()
//        assert(JxlCoder.isJXL(buffer3))
//        assert(JxlCoder().getSize(buffer3) != null)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val buffer4 = this.assets.open("pexels-thibaut-tattevin-18273081.jxl").source().buffer().readByteArray()
            assert(JxlCoder.isJXL(buffer4))
            val largeImageSize = JxlCoder().getSize(buffer4)
            assert(largeImageSize != null)
//
//            val image10Bit = image //.copy(Bitmap.Config.RGBA_F16, true)
////            image10Bit.setColorSpace(ColorSpace.get(ColorSpace.Named.DCI_P3))
//            val compressedBuffer = JxlCoder().encode(
//                image,
//                colorSpace = JxlColorSpace.RGBA,
//                compressionOption = JxlCompressionOption.LOSSY,
//                effort = 8,
//                quality = 100,
//            )
//            val decompressedImage = JxlCoder().decode(compressedBuffer, preferredColorConfig = PreferredColorConfig.RGBA_8888)

            setContent {
                JXLCoderTheme {
                    var imagesArray = remember {
                        mutableStateListOf<Bitmap>()
                    }
                    LaunchedEffect(key1 = Unit, block = {
                        for (i in 0 until 1) {
                            lifecycleScope.launch(Dispatchers.IO) {
                                val image = JxlCoder().decodeSampled(
                                    buffer4,
                                    5455,
                                    5455,
                                    preferredColorConfig = PreferredColorConfig.RGBA_8888,
                                    ScaleMode.FIT,
                                    JxlResizeFilter.BILINEAR,
                                )
                                lifecycleScope.launch(Dispatchers.Main) {
                                    imagesArray.add(image)
                                }
                            }
                        }
                    })
                    // A surface container using the 'background' color from the theme
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = MaterialTheme.colorScheme.background
                    ) {
//                        AsyncImage(
//                            model = "https://pdfconverter1984.blob.core.windows.net/simple/wide_gamut.jxl",
//                            contentDescription = null,
//                            imageLoader = ImageLoader.Builder(this)
//                                .components {
//                                    add(JxlDecoder.Factory())
//                                }
//                                .build()
//                        )
                        LazyColumn(modifier = Modifier
                            .fillMaxWidth()) {
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

//                        GlideImage(
//                            model = "https://wh.aimuse.online/preset/hdr_cosmos.jxl",
//                            contentDescription = ""
//                        )
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