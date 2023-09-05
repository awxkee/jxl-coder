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
import com.awxkee.jxlcoder.ui.theme.JXLCoderTheme
import okio.buffer
import okio.source

class MainActivity : ComponentActivity() {
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
        val buffer4 = this.assets.open("wide_gamut.jxl").source().buffer().readByteArray()
        assert(JxlCoder.isJXL(buffer4))
        val largeImageSize = JxlCoder().getSize(buffer4)
        assert(largeImageSize != null)
        val image = JxlCoder().decodeSampled(
            buffer4,
            largeImageSize!!.width / 2,
            largeImageSize!!.height / 2
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val image10Bit = image.copy(Bitmap.Config.RGBA_F16, true)
//            image10Bit.setColorSpace(ColorSpace.get(ColorSpace.Named.DCI_P3))
            val compressedBuffer = JxlCoder().encode(
                image10Bit,
                colorSpace = JxlColorSpace.RGB,
                compressionOption = JxlCompressionOption.LOSSY,
                loosyLevel = 5.0f
            )
            val decompressedImage = JxlCoder().decode(compressedBuffer)

            setContent {
                JXLCoderTheme {
                    // A surface container using the 'background' color from the theme
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = MaterialTheme.colorScheme.background
                    ) {
                        Image(
                            bitmap = decompressedImage.asImageBitmap(),
                            contentDescription = "ok"
                        )
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