/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 9/1/2024
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
import androidx.annotation.IntRange
import androidx.annotation.Keep
import java.io.Closeable

@Keep
class JxlAnimatedEncoder : Closeable {

    private var coordinator: Long = 0
    private val lock = Any()

    @Keep
    constructor(
        width: Int,
        height: Int,
        numLoops: Int = 0,
        preferredColorConfig: JxlChannelsConfiguration = JxlChannelsConfiguration.RGBA,
        compressionOption: JxlCompressionOption = JxlCompressionOption.LOSSY,
        @IntRange(from = 1L, to = 9L) effort: Int = 7,
        @IntRange(from = 0, to = 100) quality: Int = 0,
        decodingSpeed: JxlDecodingSpeed = JxlDecodingSpeed.SLOWEST,
        dataPixelFormat: JxlEncodingDataPixelFormat = JxlEncodingDataPixelFormat.UNSIGNED_8,
    ) {
        coordinator = createEncodeCoordinator(
            width, height, numLoops,
            preferredColorConfig.value,
            compressionOption.cValue,
            quality,
            -1,
            effort,
            decodingSpeed.value,
            dataPixelFormat.cValue,
        )
    }

    fun addFrame(bitmap: Bitmap, @IntRange(from = 1) duration: Int) {
        assertOpen()
        addFrameImpl(coordinator, bitmap, duration)
    }

    fun encode(): ByteArray {
        assertOpen()
        val bos = encodeAnimatedImpl(coordinator)
        close()
        return bos
    }

    private external fun createEncodeCoordinator(
        width: Int,
        height: Int,
        numLoops: Int,
        colorSpace: Int,
        compressionOption: Int,
        loosyLevel: Int,
        dataSpaceValue: Int,
        effort: Int,
        decodingSpeed: Int,
        dataPixelFormat: Int,
    ): Long

    private external fun encodeAnimatedImpl(coordinatorPtr: Long): ByteArray
    private external fun addFrameImpl(coordinatorPtr: Long, bitmap: Bitmap, duration: Int)
    private external fun closeAndReleaseAnimatedEncoder(coordinatorPtr: Long)

    private fun assertOpen() {
        synchronized(lock) {
            if (coordinator == -1L) {
                throw IllegalStateException("Animated image is already closed, call to it functions is impossible")
            }
        }
    }

    override fun close() {
        synchronized(lock) {
            if (coordinator != -1L) {
                closeAndReleaseAnimatedEncoder(coordinator)
                coordinator = -1L
            }
        }
    }

    protected fun finalize() {
        close()
    }

    init {
        if (Build.VERSION.SDK_INT >= 21) {
            System.loadLibrary("jxlcoder")
        }
    }
}