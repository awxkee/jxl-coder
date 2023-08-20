package com.awxkee.jxlcoder

import android.graphics.Bitmap
import androidx.annotation.FloatRange
import androidx.annotation.IntRange
import androidx.annotation.Keep

@Keep
class JxlCoder {

    fun decode(byteArray: ByteArray): Bitmap {
        return decodeImpl(byteArray)
    }

    /**
     * @param loosyLevel Sets the distance level for lossy compression: target max butteraugli
     *  * distance, lower = higher quality. Range: 0 .. 15.
     *  * 0.0 = mathematically lossless (however, use JxlEncoderSetFrameLossless
     *  * instead to use true lossless, as setting distance to 0 alone is not the only
     *  * requirement). 1.0 = visually lossless. Recommended range: 0.5 .. 3.0. Default
     *  * value: 1.0.
     */
    fun encode(
        bitmap: Bitmap,
        colorSpace: JxlColorSpace = JxlColorSpace.RGB,
        compressionOption: JxlCompressionOption = JxlCompressionOption.LOOSY,
        @FloatRange(from = 0.0, to = 15.0) loosyLevel: Float = 1.0f,
    ): ByteArray {
        return encodeImpl(bitmap, colorSpace.cValue, compressionOption.cValue, loosyLevel)
    }

    private external fun decodeImpl(
        byteArray: ByteArray
    ): Bitmap

    private external fun encodeImpl(
        bitmap: Bitmap,
        colorSpace: Int,
        compressionOption: Int,
        loosyLevel: Float,
    ): ByteArray

    companion object {
        // Used to load the 'jxlcoder' library on application startup.
        init {
            System.loadLibrary("jxlcoder")
        }
    }
}