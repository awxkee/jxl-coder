/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 23/9/2023
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

package com.awxkee.jxlcoder

import android.graphics.Bitmap
import android.os.Build
import android.util.Size
import androidx.annotation.IntRange
import androidx.annotation.Keep
import java.nio.ByteBuffer

@Keep
class JxlCoder {

    /**
     * @author Radzivon Bartoshyk
     */
    fun decode(
        byteArray: ByteArray,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT
    ): Bitmap {
        return decodeSampledImpl(byteArray, -1, -1, preferredColorConfig.value, scaleMode.value)
    }

    /**
     * @author Radzivon Bartoshyk
     */
    fun decodeSampled(
        byteArray: ByteArray,
        width: Int,
        height: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT
    ): Bitmap {
        return decodeSampledImpl(
            byteArray,
            width,
            height,
            preferredColorConfig.value,
            scaleMode.value
        )
    }

    /**
     * @author Radzivon Bartoshyk
     */
    fun decodeSampled(
        byteArray: ByteBuffer,
        width: Int,
        height: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT
    ): Bitmap {
        return decodeByteBufferSampledImpl(
            byteArray,
            width,
            height,
            preferredColorConfig.value,
            scaleMode.value
        )
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
        compressionOption: JxlCompressionOption = JxlCompressionOption.LOSSY,
        @IntRange(from = 1L, to = 9L) effort: Int = 7,
        @IntRange(from = 0, to = 100) quality: Int = 100,
    ): ByteArray {
        if (effort < 1 || effort > 9) {
            throw Exception("Effort must be on 1..<10")
        }
        var dataSpaceValue: Int = -1
        var bitmapColorSpace: String? = null
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val colorSpaceValue = bitmap.colorSpace?.name
            if (colorSpaceValue != null) {
                bitmapColorSpace = colorSpaceValue
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                dataSpaceValue = bitmap.colorSpace?.dataSpace ?: -1
            }
        }

        return encodeImpl(
            bitmap,
            colorSpace.cValue,
            compressionOption.cValue,
            effort,
            bitmapColorSpace,
            dataSpaceValue,
            quality,
        )
    }

    fun getSize(byteArray: ByteArray): Size? {
        return getSizeImpl(byteArray)
    }

    private external fun getSizeImpl(byteArray: ByteArray): Size?

    private external fun decodeSampledImpl(
        byteArray: ByteArray,
        width: Int,
        height: Int,
        preferredColorConfig: Int,
        scaleMode: Int,
    ): Bitmap

    private external fun decodeByteBufferSampledImpl(
        byteArray: ByteBuffer,
        width: Int,
        height: Int,
        preferredColorConfig: Int,
        scaleMode: Int,
    ): Bitmap

    private external fun encodeImpl(
        bitmap: Bitmap,
        colorSpace: Int,
        compressionOption: Int,
        loosyLevel: Int,
        bitmapColorSpace: String?,
        dataSpaceValue: Int,
        quality: Int,
    ): ByteArray

    companion object {

        private val MAGIC_1 = byteArrayOf(0xFF.toByte(), 0x0A)
        private val MAGIC_2 = byteArrayOf(
            0x0.toByte(),
            0x0.toByte(),
            0x0.toByte(),
            0x0C.toByte(),
            0x4A,
            0x58,
            0x4C,
            0x20,
            0x0D,
            0x0A,
            0x87.toByte(),
            0x0A
        )

        init {
            if (Build.VERSION.SDK_INT >= 21) {
                System.loadLibrary("jxlcoder")
            }
        }

        fun isJXL(byteArray: ByteArray): Boolean {
            if (byteArray.size < MAGIC_2.size) {
                return false
            }
            val sample1 = byteArray.copyOfRange(0, 2)
            val sample2 = byteArray.copyOfRange(0, MAGIC_2.size)
            return sample1.contentEquals(MAGIC_1) || sample2.contentEquals(MAGIC_2)
        }
    }
}