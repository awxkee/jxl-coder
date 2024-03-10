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
object JxlCoder {

    /**
     * @author Radzivon Bartoshyk
     */
    fun decode(
        byteArray: ByteArray,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        toneMapper: JxlToneMapper = JxlToneMapper.LOGARITHMIC,
    ): Bitmap {
        return decodeSampledImpl(
            byteArray,
            -1,
            -1,
            preferredColorConfig.value,
            scaleMode.value,
            JxlResizeFilter.CATMULL_ROM.value,
            jxlToneMapper = toneMapper.value,
        )
    }

    /**
     * @author Radzivon Bartoshyk
     */
    fun decodeSampled(
        byteArray: ByteArray,
        width: Int,
        height: Int,
        preferredColorConfig: PreferredColorConfig = PreferredColorConfig.DEFAULT,
        scaleMode: ScaleMode = ScaleMode.FIT,
        jxlResizeFilter: JxlResizeFilter = JxlResizeFilter.CATMULL_ROM,
        toneMapper: JxlToneMapper = JxlToneMapper.LOGARITHMIC,
    ): Bitmap {
        return decodeSampledImpl(
            byteArray,
            width,
            height,
            preferredColorConfig.value,
            scaleMode.value,
            jxlResizeFilter.value,
            jxlToneMapper = toneMapper.value,
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
        scaleMode: ScaleMode = ScaleMode.FIT,
        jxlResizeFilter: JxlResizeFilter = JxlResizeFilter.MITCHELL_NETRAVALI,
        toneMapper: JxlToneMapper = JxlToneMapper.LOGARITHMIC,
    ): Bitmap {
        return decodeByteBufferSampledImpl(
            byteArray,
            width,
            height,
            preferredColorConfig.value,
            scaleMode.value,
            jxlResizeFilter.value,
            jxlToneMapper = toneMapper.value,
        )
    }

    fun encode(
        bitmap: Bitmap,
        colorSpace: JxlChannelsConfiguration = JxlChannelsConfiguration.RGB,
        compressionOption: JxlCompressionOption = JxlCompressionOption.LOSSY,
        effort: JxlEffort = JxlEffort.SQUIRREL,
        @IntRange(from = 0, to = 100) quality: Int = 0,
        decodingSpeed: JxlDecodingSpeed = JxlDecodingSpeed.SLOWEST,
    ): ByteArray {
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
            colorSpace.value,
            compressionOption.cValue,
            effort.value,
            bitmapColorSpace,
            dataSpaceValue,
            quality,
            decodingSpeed.value,
        )
    }

    object Convenience {

        /**
         * @param gifData - byte array contains the GIF data
         * @return Byte array contains converted JPEG XL
         */
        fun gif2JXL(
            gifData: ByteArray,
            @IntRange(from = 0, to = 100) quality: Int = 0,
            effort: JxlEffort = JxlEffort.SQUIRREL,
            decodingSpeed: JxlDecodingSpeed = JxlDecodingSpeed.SLOWEST,
        ): ByteArray {
            return gif2JXLImpl(gifData, quality, effort.value, decodingSpeed.value)
        }

        /**
         * @param apngData - byte array contains the APNG data
         * @return Byte array contains converted JPEG XL
         */
        fun apng2JXL(
            apngData: ByteArray,
            @IntRange(from = 0, to = 100) quality: Int = 0,
            effort: JxlEffort = JxlEffort.SQUIRREL,
            decodingSpeed: JxlDecodingSpeed = JxlDecodingSpeed.FAST,
        ): ByteArray {
            return apng2JXLImpl(apngData, quality, effort.value, decodingSpeed.value)
        }

        /**
         * @author Radzivon Bartoshyk
         * @param fromJpegData - JPEG data that will be used for JPEG XL lossless construction
         * @return Byte array that contains constructed JPEG XL
         */
        fun construct(fromJpegData: ByteArray): ByteArray {
            return constructImpl(fromJpegData)
        }

        /**
         * @author Radzivon Bartoshyk
         * @param fromJPEGXLData - JPEG XL data that will be used for JPEG lossless construction
         * @return Byte array that contains re-constructed JPEG
         */
        fun reconstructJPEG(fromJPEGXLData: ByteArray): ByteArray {
            return reconstructImpl(fromJPEGXLData)
        }

    }

    /**
     * @return NULL if byte array is not valid JPEG XL
     */
    fun getSize(byteArray: ByteArray): Size? {
        return getSizeImpl(byteArray)
    }

    private external fun apng2JXLImpl(
        apngData: ByteArray,
        quality: Int,
        effort: Int,
        decodingSpeed: Int,
    ): ByteArray

    private external fun gif2JXLImpl(
        gifData: ByteArray,
        quality: Int,
        effort: Int,
        decodingSpeed: Int,
    ): ByteArray

    private external fun reconstructImpl(fromJPEGXLData: ByteArray): ByteArray

    private external fun constructImpl(fromJpegData: ByteArray): ByteArray

    private external fun getSizeImpl(byteArray: ByteArray): Size?

    private external fun decodeSampledImpl(
        byteArray: ByteArray,
        width: Int,
        height: Int,
        preferredColorConfig: Int,
        scaleMode: Int,
        jxlResizeSampler: Int,
        jxlToneMapper: Int,
    ): Bitmap

    private external fun decodeByteBufferSampledImpl(
        byteArray: ByteBuffer,
        width: Int,
        height: Int,
        preferredColorConfig: Int,
        scaleMode: Int,
        jxlResizeSampler: Int,
        jxlToneMapper: Int,
    ): Bitmap

    private external fun encodeImpl(
        bitmap: Bitmap,
        colorSpace: Int,
        compressionOption: Int,
        loosyLevel: Int,
        bitmapColorSpace: String?,
        dataSpaceValue: Int,
        quality: Int,
        decodingSpeed: Int
    ): ByteArray

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