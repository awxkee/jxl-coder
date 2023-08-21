package com.awxkee.jxlcoder

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

        val buffer1 = this.assets.open("first_jxl.jxl").source().buffer().readByteArray()
        assert(JxlCoder.isJXL(buffer1))
        val buffer2 = this.assets.open("second_jxl.jxl").source().buffer().readByteArray()
        assert(JxlCoder.isJXL(buffer2))
        val buffer3 = this.assets.open("alpha_jxl.jxl").source().buffer().readByteArray()
        assert(JxlCoder.isJXL(buffer3))
        val image = JxlCoder().decode(buffer3)
        val compressedBuffer = JxlCoder().encode(
            image,
            colorSpace = JxlColorSpace.RGBA,
            compressionOption = JxlCompressionOption.LOOSY,
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
                    Image(bitmap = decompressedImage.asImageBitmap(), contentDescription = "ok")
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