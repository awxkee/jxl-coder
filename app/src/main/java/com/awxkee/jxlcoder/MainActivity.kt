package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.graphics.ColorSpace
import android.hardware.DataSpace
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.tooling.preview.Preview
import coil.ImageLoader
import coil.compose.AsyncImage
import com.awxkee.jxlcoder.ui.theme.JXLCoderTheme
import com.bumptech.glide.integration.compose.ExperimentalGlideComposeApi
import com.bumptech.glide.integration.compose.GlideImage
import okio.buffer
import okio.source

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
            val buffer4 = this.assets.open("summer_nature.jxl").source().buffer().readByteArray()
            assert(JxlCoder.isJXL(buffer4))
            val largeImageSize = JxlCoder().getSize(buffer4)
            assert(largeImageSize != null)
            val image = JxlCoder().decodeSampled(
                buffer4,
                largeImageSize!!.width / 2,
                largeImageSize!!.height / 2,
                preferredColorConfig = PreferredColorConfig.RGBA_8888,
                ScaleMode.FIT,
                JxlResizeFilter.LANCZOS
            )
//
//            val image10Bit = image //.copy(Bitmap.Config.RGBA_F16, true)
////            image10Bit.setColorSpace(ColorSpace.get(ColorSpace.Named.DCI_P3))
//            val compressedBuffer = JxlCoder().encode(
//                image,
//                colorSpace = JxlColorSpace.RGB,
//                compressionOption = JxlCompressionOption.LOSSY,
//                effort = 8,
//                quality = 100,
//            )
//            val decompressedImage = JxlCoder().decode(compressedBuffer, preferredColorConfig = PreferredColorConfig.RGBA_8888)

            setContent {
                JXLCoderTheme {
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
                        Image(
                            bitmap = image.asImageBitmap(),
                            contentDescription = "ok"
                        )

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