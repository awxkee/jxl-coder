package com.awxkee.jxlcoder

import android.graphics.Bitmap
import androidx.annotation.Keep

@Keep
class JxlCoder {

    fun decode(byteArray: ByteArray): Bitmap {
        return decodeImpl(byteArray)
    }

    fun encode(bitmap: Bitmap): ByteArray {
        return encodeImpl(bitmap)
    }

    private external fun decodeImpl(
        byteArray: ByteArray,
    ): Bitmap

    private external fun encodeImpl(bitmap: Bitmap): ByteArray

    companion object {
        // Used to load the 'jxlcoder' library on application startup.
        init {
            System.loadLibrary("jxlcoder")
        }
    }
}